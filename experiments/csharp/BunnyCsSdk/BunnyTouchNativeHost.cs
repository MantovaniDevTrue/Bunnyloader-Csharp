using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>
/// Ponte direta MonoVM -> libbunny.so. Este contrato não passa pelo QuickJS.
/// O host informa Ready somente depois de instalar/validar os hooks IL2CPP.
/// </summary>
internal static class BunnyTouchNativeHost
{
    [DllImport("bunny", EntryPoint = "bunny_csharp_touch_host_ready", CallingConvention = CallingConvention.Cdecl)]
    private static extern int HostReady();

    [DllImport("bunny", EntryPoint = "bunny_csharp_touch_register", CallingConvention = CallingConvention.Cdecl)]
    private static extern int RegisterNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string owner,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string label,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string icon);

    [DllImport("bunny", EntryPoint = "bunny_csharp_touch_read", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint ReadNative(int id);

    [DllImport("bunny", EntryPoint = "bunny_csharp_touch_world_reset", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void ResetWorld();

    [DllImport("bunny", EntryPoint = "bunny_csharp_touch_mod_unload", CallingConvention = CallingConvention.Cdecl)]
    private static extern void UnloadNative([MarshalAs(UnmanagedType.LPUTF8Str)] string owner);

    internal static bool Ready() => HostReady() == 1;

    internal static int Register(BunnyTouchButton button) =>
        RegisterNative(button.ModId, button.Name, button.DisplayName, button.Icon);

    internal static BunnyTouchButtonState Read(int id)
    {
        uint flags = ReadNative(id);
        return new BunnyTouchButtonState(
            (flags & 1u) != 0,
            (flags & 2u) != 0,
            (flags & 4u) != 0,
            (flags & 8u) != 0,
            (flags & 16u) != 0,
            (flags & 32u) != 0);
    }

    internal static void Unload(string owner) => UnloadNative(owner);
}
