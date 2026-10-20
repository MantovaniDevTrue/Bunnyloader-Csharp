using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyInvasionNotifications
{
    public static bool Publish(long worldSession, string modId,
        BunnyInvasionController controller)
    {
        if (worldSession <= 0 || controller == null ||
            !BunnyInvasionWireState.ValidName(modId, 128) ||
            !BunnyInvasionWireState.ValidName(controller.Definition.Id, 64))
            return false;
        BunnyInvasionSnapshot snapshot = controller.Snapshot();
        return Native.Publish(worldSession, modId, controller.Definition.Id,
            (int)snapshot.Phase, snapshot.Progress, controller.Definition.Goal) == 1;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_notice",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Publish(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string id,
            int phase, int progress, int goal);
    }
}
