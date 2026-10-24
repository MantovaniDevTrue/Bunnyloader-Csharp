using System;
using System.Runtime.InteropServices;
using System.Text;

namespace BunnyLoader.Source;

internal static class BunnyWorldPersistence
{
    internal static int Read(long session, string modId, string system,
        out BunnyWorldData tag, out string encoded)
    {
        tag = new BunnyWorldData();
        encoded = "";
        if (session <= 0 || string.IsNullOrWhiteSpace(modId) ||
            string.IsNullOrWhiteSpace(system)) return -2;
        var buffer = new byte[16385];
        int size;
        try { size = Native.Read(session, modId, system, buffer, buffer.Length); }
        catch (DllNotFoundException) { return -2; }
        catch (EntryPointNotFoundException) { return -2; }
        if (size < 0) return size;
        if (size == 0)
        {
            encoded = tag.Encode();
            return 0;
        }
        if (size >= buffer.Length) return -1;
        encoded = Encoding.ASCII.GetString(buffer, 0, size);
        if (!BunnyWorldData.TryDecode(encoded, out tag))
        {
            encoded = "";
            return -1;
        }
        return 1;
    }

    internal static bool Write(long session, string modId, string system,
        string encoded)
    {
        try { return Native.Write(session, modId, system, encoded) == 1; }
        catch (DllNotFoundException) { return false; }
        catch (EntryPointNotFoundException) { return false; }
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_world_system_state_read",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Read(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string system,
            [Out] byte[] output, int capacity);

        [DllImport("bunny", EntryPoint = "bunny_world_system_state_write",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Write(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string system,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string payload);
    }
}
