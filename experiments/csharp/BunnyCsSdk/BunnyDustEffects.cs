using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>Enqueues local dust emission; only Terraria's thread touches native entities.</summary>
public static class BunnyDustEffects
{
    /// <returns>True when queued. World exit, missing content or a full dust pool can discard emission.</returns>
    public static bool EmitAtPlayer<T>(int playerIndex, int count = 8) where T : BunnyDust
    {
        if (playerIndex < 0 || playerIndex >= 255 || count < 1 || count > 16)
            throw new ArgumentOutOfRangeException(nameof(count), "Expected player 0..254 and count 1..16");
        string mod = BunnyDustSourceDispatcher.ModForAssembly(typeof(T).Assembly);
        long session = Native.Session();
        return session > 0 && Native.Emit(session, mod, typeof(T).Name, playerIndex, count) == 1;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_dust_emission_session", CallingConvention = CallingConvention.Cdecl)]
        internal static extern long Session();
        [DllImport("bunny", EntryPoint = "bunny_dust_emit", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Emit(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name, int player, int count);
    }
}
