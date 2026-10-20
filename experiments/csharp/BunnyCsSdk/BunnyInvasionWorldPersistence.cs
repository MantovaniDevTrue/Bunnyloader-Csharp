using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyInvasionWorldPersistence
{
    public static bool TryRestore(long worldSession, string modId,
        BunnyInvasionController controller)
    {
        ArgumentNullException.ThrowIfNull(controller);
        if (worldSession <= 0 || string.IsNullOrWhiteSpace(modId)) return false;
        if (Native.Read(worldSession, modId, controller.Definition.Id,
            out int phase, out int progress, out int cooldown) != 1)
            return false;
        if (phase < 0 || phase > 2) return false;
        var snapshot = new BunnyInvasionSnapshot(
            0, 0, (BunnyInvasionPhase)phase, progress, 0, 0, cooldown);
        return controller.Restore(true, snapshot);
    }

    public static bool Save(long worldSession, string modId,
        BunnyInvasionController controller)
    {
        ArgumentNullException.ThrowIfNull(controller);
        if (worldSession <= 0 || string.IsNullOrWhiteSpace(modId)) return false;
        BunnyInvasionSnapshot snapshot = controller.Snapshot();
        return Native.Write(worldSession, modId, controller.Definition.Id,
            (int)snapshot.Phase, snapshot.Progress, snapshot.Cooldown) == 1;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_state_read",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Read(long worldSession,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string invasionId,
            out int phase, out int progress, out int cooldown);

        [DllImport("bunny", EntryPoint = "bunny_invasion_state_write",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Write(long worldSession,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string invasionId,
            int phase, int progress, int cooldown);
    }
}
