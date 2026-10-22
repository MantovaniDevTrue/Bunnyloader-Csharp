using System;
using System.Collections.Generic;
using System.Threading;

namespace BunnyLoader.Source;

/// <summary>
/// A waterfall texture and its hooks, following the tModLoader ModWaterfallStyle contract.
/// Instances belong to one mod and one loader session. Gameplay hooks do not call JavaScript.
/// </summary>
public abstract class ModWaterfallStyle
{
    public int Slot { get; internal set; } = -1;
    public string FullName { get; internal set; } = "";

    // Override if the texture is not located at the namespace-derived path.
    public virtual string Texture => (GetType().FullName ?? GetType().Name).Replace('.', '/') + ".png";

    public virtual bool IsLoadingEnabled() => true;
    public virtual void Load() { }
    public virtual void SetStaticDefaults() { }
    public virtual void Unload() { }

    // Tile coordinates. Use WaterfallLighting.AddLight to submit a native light.
    public virtual void AddLight(int i, int j) { }

    // The RGB values are floats, matching tModLoader, NOT the 0..255 JS Ref API.
    public virtual void ColorMultiplier(ref float r, ref float g, ref float b, float a) { }
}

/// <summary>One native light request, produced on the managed worker and applied by the game thread.</summary>
public readonly record struct WaterfallLight(int TileX, int TileY, float R, float G, float B);

/// <summary>The host owns the queue; the mod is never allowed to touch IL2CPP from the Mono worker.</summary>
public interface IWaterfallLightSink
{
    void Enqueue(in WaterfallLight light);
}

public static class WaterfallLighting
{
    [ThreadStatic] private static IWaterfallLightSink? current;
    [ThreadStatic] private static int remaining;

    // Invoked by the dispatcher around ModWaterfallStyle.AddLight only.
    internal static void Dispatch(IWaterfallLightSink sink, Action action)
    {
        IWaterfallLightSink? previous = current;
        int previousRemaining = remaining;
        current = sink;
        remaining = 12;
        try { action(); }
        finally { current = previous; remaining = previousRemaining; }
    }

    /// <summary>
    /// tModLoader-like lighting call. Coordinates are in tiles; brightness may exceed 1.
    /// The native host consumes the bounded output on the Terraria game thread.
    /// </summary>
    public static void AddLight(int tileX, int tileY, float r, float g, float b)
    {
        if (current is null)
            throw new InvalidOperationException("WaterfallLighting.AddLight must run inside ModWaterfallStyle.AddLight.");
        if (remaining <= 0)
            throw new InvalidOperationException("Too many lights in one waterfall callback (maximum 12).");
        if (!float.IsFinite(r) || !float.IsFinite(g) || !float.IsFinite(b) ||
            r < 0 || g < 0 || b < 0)
            throw new ArgumentOutOfRangeException(nameof(r), "Light components must be finite and non-negative.");
        if (tileX < 0 || tileY < 0)
            throw new ArgumentOutOfRangeException(nameof(tileX), "Tile coordinates must be non-negative.");
        remaining--;
        current.Enqueue(new WaterfallLight(tileX, tileY, r, g, b));
    }
}

/// <summary>Immutable content descriptor transferred to the native texture installer.</summary>
public readonly record struct WaterfallStyleDescriptor(int Slot, string ModId, string FullName, string Texture);

/// <summary>
/// Registration and bounded hook dispatch for custom waterfalls. Each game session has its own
/// loader. Native texture installation and native waterfall detection must precede Activate().
/// </summary>
public sealed class WaterfallStyleLoader
{
    public const int VanillaCount = 28;
    public const int MaximumStyles = 256;

    private readonly List<ModWaterfallStyle> staged = new();
    private readonly List<WaterfallStyleDescriptor> stagedDescriptors = new();
    private readonly HashSet<string> names = new(StringComparer.Ordinal);
    private readonly object gate = new();
    private ModWaterfallStyle[] published = Array.Empty<ModWaterfallStyle>();
    private bool sealedContent;
    private int active;
    private long colorCalls;
    private long lightCalls;
    private long rejectedCalls;

    public int TotalCount => VanillaCount + Volatile.Read(ref published).Length;
    public long ColorCalls => Interlocked.Read(ref colorCalls);
    public long LightCalls => Interlocked.Read(ref lightCalls);
    public long RejectedCalls => Interlocked.Read(ref rejectedCalls);
    public bool IsActive => Volatile.Read(ref active) != 0;

    /// <summary>
    /// Called once by C# content discovery, after all mods have stable identities.
    /// Files are checked before the native host commits the texture table.
    /// </summary>
    public int Register(string modId, ModWaterfallStyle style, Func<string, bool> textureExists)
    {
        ArgumentNullException.ThrowIfNull(style);
        ArgumentNullException.ThrowIfNull(textureExists);
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 80 ||
            !IsSafeIdentifier(modId))
            throw new ArgumentException("Invalid mod identity.", nameof(modId));

        lock (gate)
        {
            if (sealedContent)
                throw new InvalidOperationException("Waterfall content is already sealed.");
            if (staged.Count >= MaximumStyles)
                throw new InvalidOperationException("Custom waterfall limit reached.");
            if (style.Slot != -1 || style.FullName.Length != 0)
                throw new InvalidOperationException("Waterfall instance already registered.");
            string name = style.GetType().FullName ?? style.GetType().Name;
            string fullName = modId + "/" + name;
            if (names.Contains(fullName))
                throw new InvalidOperationException("Duplicate waterfall: " + fullName);
            if (!style.IsLoadingEnabled()) return -1;

            string texture = style.Texture;
            if (!IsSafeTexture(texture) || !textureExists(texture))
                throw new InvalidOperationException("Missing or unsafe waterfall texture: " + texture);

            style.Load();
            int slot = VanillaCount + staged.Count;
            style.Slot = slot;
            style.FullName = fullName;
            try
            {
                style.SetStaticDefaults();
                staged.Add(style);
                stagedDescriptors.Add(new WaterfallStyleDescriptor(slot, modId, fullName, texture));
                names.Add(fullName);
                return slot;
            }
            catch
            {
                style.Slot = -1;
                style.FullName = "";
                style.Unload();
                throw;
            }
        }
    }

    /// <summary>
    /// Publish a stable slot table. An install failure leaves the dispatcher disabled; no
    /// half-installed texture or slot can become visible to a live waterfall.
    /// </summary>
    public IReadOnlyList<WaterfallStyleDescriptor> Seal()
    {
        lock (gate)
        {
            if (!sealedContent)
            {
                Volatile.Write(ref published, staged.ToArray());
                sealedContent = true;
            }
            return stagedDescriptors.ToArray();
        }
    }

    /// <summary>
    /// The native host calls this only after verifying that EVERY texture is installed
    /// at its assigned slot in WaterfallManager.waterfallTexture.
    /// </summary>
    public void Activate()
    {
        lock (gate)
        {
            if (!sealedContent)
                throw new InvalidOperationException("Seal the waterfall registry before activation.");
            Volatile.Write(ref active, 1);
        }
    }

    public ModWaterfallStyle? GetWaterfallStyle(int slot)
    {
        ModWaterfallStyle[] snapshot = Volatile.Read(ref published);
        int index = slot - VanillaCount;
        return (uint)index < (uint)snapshot.Length ? snapshot[index] : null;
    }

    /// <summary>
    /// Dispatch one waterfall's color calculation; a failed hook preserves the game's
    /// original color. Call only for slots at or above VanillaCount.
    /// </summary>
    public bool TryColorMultiplier(int slot, ref float r, ref float g, ref float b, float alpha)
    {
        if (!IsActive || !float.IsFinite(alpha)) return false;
        ModWaterfallStyle? style = GetWaterfallStyle(slot);
        if (style is null) return false;
        float nr = r, ng = g, nb = b;
        try
        {
            style.ColorMultiplier(ref nr, ref ng, ref nb, alpha);
            if (!float.IsFinite(nr) || !float.IsFinite(ng) || !float.IsFinite(nb))
                throw new ArithmeticException("Non-finite waterfall color.");
            r = nr; g = ng; b = nb;
            Interlocked.Increment(ref colorCalls);
            return true;
        }
        catch
        {
            Interlocked.Increment(ref rejectedCalls);
            return false;
        }
    }

    /// <summary>
    /// The game reports one waterfall's starting tile and length. As in the Bunny native
    /// waterfall path, sample at most 24 tiles and call AddLight every three tiles.
    /// </summary>
    public bool TryAddLight(int slot, int tileX, int tileY, int stopAtStep, IWaterfallLightSink sink)
    {
        if (!IsActive || sink is null || tileX < 0 || tileY < 0 || stopAtStep < 0)
            return false;
        ModWaterfallStyle? style = GetWaterfallStyle(slot);
        if (style is null) return false;
        try
        {
            for (int s = 0, last = Math.Min(stopAtStep, 24); s <= last; s += 3)
            {
                int y = checked(tileY + s);
                WaterfallLighting.Dispatch(sink, () => style.AddLight(tileX, y));
            }
            Interlocked.Increment(ref lightCalls);
            return true;
        }
        catch
        {
            Interlocked.Increment(ref rejectedCalls);
            return false;
        }
    }

    /// <summary>Clear every mod/session-owned callback before another world or package reload.</summary>
    public void Unload()
    {
        lock (gate)
        {
            Volatile.Write(ref active, 0);
            for (int i = staged.Count - 1; i >= 0; i--)
            {
                var style = staged[i];
                try { style.Unload(); }
                finally { style.Slot = -1; style.FullName = ""; }
            }
            staged.Clear();
            stagedDescriptors.Clear();
            names.Clear();
            Volatile.Write(ref published, Array.Empty<ModWaterfallStyle>());
            sealedContent = false;
            Interlocked.Exchange(ref colorCalls, 0);
            Interlocked.Exchange(ref lightCalls, 0);
            Interlocked.Exchange(ref rejectedCalls, 0);
        }
    }

    private static bool IsSafeIdentifier(string value)
    {
        foreach (char c in value)
            if (!(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-')) return false;
        return true;
    }

    private static bool IsSafeTexture(string? value)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > 240 ||
            value[0] == '/' || value.IndexOfAny(new[] {'\\', ':', '\0', '\r', '\n'}) >= 0 ||
            !value.EndsWith(".png", StringComparison.OrdinalIgnoreCase))
            return false;
        foreach (string segment in value.Split('/'))
            if (segment.Length == 0 || segment == "." || segment == "..") return false;
        return true;
    }
}
