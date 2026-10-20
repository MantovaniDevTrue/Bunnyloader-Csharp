using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyInvasionHud
{
    public static bool Publish(long worldSession, string modId,
        BunnyInvasionController controller)
    {
        ArgumentNullException.ThrowIfNull(controller);
        if (worldSession <= 0 || !BunnyInvasionWireState.ValidName(modId, 128))
            return false;
        var snapshot = controller.Snapshot();
        return Native.Publish(worldSession, modId, controller.Definition.Id,
            (int)snapshot.Phase, snapshot.Progress, controller.Definition.Goal) == 1;
    }

    public static bool Publish(long worldSession, BunnyInvasionWireState state)
    {
        if (worldSession <= 0 || !state.IsValid()) return false;
        return Native.Publish(worldSession, state.ModId, state.InvasionId,
            (int)state.Phase, state.Progress, state.Goal) == 1;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_hud_publish",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Publish(long worldSession,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string invasionId,
            int phase, int progress, int goal);
    }
}
