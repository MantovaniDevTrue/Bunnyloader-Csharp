using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>A texture registered in Terraria's background tables. The PNG is owned by the mod.</summary>
public abstract class BunnyBackgroundTexture
{
    /// <summary>Path inside content/, with or without .png. Declare a literal for early asset discovery.</summary>
    public string Texture { get; set; } = "";
    public abstract void SetStaticDefaults();
}

/// <summary>Resolve background slots at runtime. Store the UID/path, never the numeric slot in a save.</summary>
public static class BunnyBackgroundTextures
{
    [DllImport("bunny", EntryPoint = "bunny_background_texture_slot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSlot([MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    public static int GetSlot(string modId, string path)
    {
        Validate(modId, path);
        int slot;
        try { slot = NativeSlot(modId, Normalize(path)); }
        catch (Exception error) when (error is DllNotFoundException or EntryPointNotFoundException)
        {
            throw new InvalidOperationException("[CSharp][ERROR][BackgroundTexture] Mod=" + modId +
                " Path=" + path + " Stage=ResolveSlot Result=Rejected Reason=NativeBridgeUnavailable", error);
        }
        if (slot < 0) throw new InvalidOperationException("[CSharp][ERROR][BackgroundTexture] Mod=" + modId +
            " Path=" + path + " Stage=ResolveSlot Expected=PublishedTexture Actual=" + slot +
            " Result=Rejected Reason=TextureNotRegisteredOrNotReady");
        return slot;
    }

    public static bool TryGetSlot(string modId, string path, out int slot)
    {
        Validate(modId, path);
        // A missing native bridge is a real integration failure, not a missing asset.
        slot = NativeSlot(modId, Normalize(path));
        return slot >= 0;
    }

    public static int GetSlot<T>(string modId) where T : BunnyBackgroundTexture, new()
    {
        var texture = new T();
        texture.SetStaticDefaults();
        return GetSlot(modId, texture.Texture);
    }

    private static string Normalize(string path) => path.EndsWith(".png", StringComparison.OrdinalIgnoreCase)
        ? path.Substring(0, path.Length - 4) : path;

    private static void Validate(string mod, string path)
    {
        foreach (char c in mod ?? "") if (c < 32 || c == 127) throw new ArgumentException("Control character in mod identity", nameof(mod));
        foreach (char c in path ?? "") if (c < 32 || c == 127) throw new ArgumentException("Control character in asset path", nameof(path));
        if (string.IsNullOrWhiteSpace(mod) || mod.Length > 128 || mod.IndexOfAny(new[] {'/', '\\', '|', '\n', '\r'}) >= 0)
            throw new ArgumentException("Expected a mod UID or native registry identity", nameof(mod));
        if (string.IsNullOrWhiteSpace(path) || path.Length > 240 || path.StartsWith("/", StringComparison.Ordinal) ||
            path.Contains("..", StringComparison.Ordinal) || path.IndexOfAny(new[] {'\\', ':', '|', '\n', '\r'}) >= 0)
            throw new ArgumentException("Expected a relative background asset path inside content/", nameof(path));
    }
}
