using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyNativeItemUse
{
    public static bool Bind(long worldSession, string modId, string itemName) =>
        worldSession > 0 && !string.IsNullOrWhiteSpace(modId) &&
        !string.IsNullOrWhiteSpace(itemName) &&
        Native.Bind(worldSession, modId, itemName) == 1;

    public static bool IsReady(long worldSession, string modId, string itemName) =>
        worldSession > 0 && !string.IsNullOrWhiteSpace(modId) &&
        !string.IsNullOrWhiteSpace(itemName) &&
        Native.Ready(worldSession, modId, itemName) == 1;

    public static bool TryPoll(long worldSession, string modId, string itemName,
        out int playerIndex)
    {
        playerIndex = -1;
        if (worldSession <= 0 || string.IsNullOrWhiteSpace(modId) ||
            string.IsNullOrWhiteSpace(itemName)) return false;
        return Native.Poll(worldSession, modId, itemName, out playerIndex) == 1 &&
            playerIndex >= 0 && playerIndex < 255;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_item_use_bind",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Bind(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        [DllImport("bunny", EntryPoint = "bunny_item_use_ready",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Ready(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        [DllImport("bunny", EntryPoint = "bunny_item_use_poll",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Poll(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
            out int playerIndex);
    }
}
