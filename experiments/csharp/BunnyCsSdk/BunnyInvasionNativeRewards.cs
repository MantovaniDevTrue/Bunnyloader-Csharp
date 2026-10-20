using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyInvasionNativeRewards
{
    public static bool Submit(long worldSession, string modId, string invasionId,
        int epoch, string itemClass, int stack, int playerIndex)
    {
        if (worldSession <= 0 || epoch <= 0 ||
            stack < 1 || stack > 999 ||
            playerIndex < 0 || playerIndex >= 255 ||
            !BunnyInvasionWireState.ValidName(modId, 128) ||
            !BunnyInvasionWireState.ValidName(invasionId, 64) ||
            !BunnyInvasionWireState.ValidName(itemClass, 128)) return false;
        return Native.Submit(worldSession, epoch, modId, invasionId,
            itemClass, stack, playerIndex) == 1;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_reward_submit",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Submit(long session, int epoch,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string invasion,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string item, int amount, int player);
    }
}
