using System;
using System.Runtime.InteropServices;
using System.Text;

namespace BunnyLoader.Source;

public static class BunnyInvasionNativeNetwork
{
    public static bool Publish(long worldSession, string modId,
        BunnyInvasionController controller)
    {
        if (worldSession <= 0 || controller == null ||
            !BunnyInvasionWireState.ValidName(modId, 128)) return false;
        BunnyInvasionWireState message = BunnyInvasionWireState.Capture(
            worldSession, modId, controller);
        return message.IsValid() && Native.Publish(worldSession, message.Encode()) == 1;
    }

    public static bool TryPoll(long worldSession, string modId,
        out BunnyInvasionWireState state)
    {
        state = default;
        if (worldSession <= 0 || !BunnyInvasionWireState.ValidName(modId, 128))
            return false;
        var buffer = new byte[304];
        int size = Native.Poll(worldSession, modId, buffer, buffer.Length);
        if (size < 1 || size > 300) return false;
        string data = Encoding.ASCII.GetString(buffer, 0, size);
        return BunnyInvasionWireState.TryDecode(data, out state) &&
            state.ModId == modId;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_invasion_net_publish",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Publish(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string payload);

        [DllImport("bunny", EntryPoint = "bunny_invasion_net_poll",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Poll(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string mod,
            [Out] byte[] buffer, int capacity);
    }
}
