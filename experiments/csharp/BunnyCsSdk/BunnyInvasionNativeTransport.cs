using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public readonly record struct BunnyNativeInvasionReply(
    int Epoch, int Ticket, int Error, int Slot, long Identity)
{
    public bool Success => Error == 0 && Slot >= 0 && Identity != 0;
}

public static class BunnyInvasionNativeTransport
{
    public static long WorldSession => Native.WorldSession();

    public static bool Submit(long worldSession, BunnyInvasionSpawn spawn,
        string modId, int playerIndex, float x, float y)
    {
        if (worldSession <= 0 || spawn.Epoch <= 0 || spawn.Ticket <= 0 ||
            string.IsNullOrWhiteSpace(modId) || string.IsNullOrWhiteSpace(spawn.NpcClass) ||
            !float.IsFinite(x) || !float.IsFinite(y)) return false;
        return Native.Submit(worldSession, spawn.Epoch, spawn.Ticket, modId,
            spawn.NpcClass, playerIndex, x, y) == 1;
    }

    public static bool TryPoll(long worldSession, string modId,
        out BunnyNativeInvasionReply result)
    {
        result = default;
        if (worldSession <= 0 || string.IsNullOrWhiteSpace(modId)) return false;
        int received = Native.Poll(worldSession, modId, out int epoch, out int ticket,
            out int error, out int slot, out long identity);
        if (received != 1) return false;
        result = new BunnyNativeInvasionReply(epoch, ticket, error, slot, identity);
        return true;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_world_session",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern long WorldSession();

        [DllImport("bunny", EntryPoint = "bunny_invasion_submit",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Submit(long worldSession, int epoch, int ticket,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string npcClass,
            int playerIndex, float x, float y);

        [DllImport("bunny", EntryPoint = "bunny_invasion_poll",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Poll(long worldSession,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            out int epoch, out int ticket, out int error, out int slot, out long identity);
    }
}
