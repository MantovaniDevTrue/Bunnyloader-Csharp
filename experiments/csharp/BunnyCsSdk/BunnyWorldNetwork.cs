using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

namespace BunnyLoader.Source;

public readonly record struct BunnyWorldNetState(
    long HostSession, string ModId, string SystemName, long Revision, byte[] Data)
{
    private static int DecimalLength(long value)
    {
        int count = 1;
        while (value >= 10)
        {
            value /= 10;
            count++;
        }
        return count;
    }

    public bool IsValid() =>
        HostSession > 0 && Revision > 0 &&
        BunnyInvasionWireState.ValidName(ModId, 128) &&
        BunnyInvasionWireState.ValidName(SystemName, 128) &&
        Data != null && Data.Length is > 0 and <= BunnyNetWriter.MaxBytes &&
        8 + DecimalLength(HostSession) + ModId.Length + SystemName.Length +
            DecimalLength(Revision) + Data.Length * 2 <= 300;

    public string Encode() => string.Concat("WS1|",
        HostSession.ToString(CultureInfo.InvariantCulture), "|",
        ModId, "|", SystemName, "|",
        Revision.ToString(CultureInfo.InvariantCulture), "|",
        Convert.ToHexString(Data));

    public static bool TryDecode(string? input, out BunnyWorldNetState state)
    {
        state = default;
        if (string.IsNullOrEmpty(input) || input.Length > 300 ||
            !input.StartsWith("WS1|", StringComparison.Ordinal)) return false;
        string[] fields = input.Split('|');
        if (fields.Length != 6 ||
            !long.TryParse(fields[1], NumberStyles.None, CultureInfo.InvariantCulture, out long session) ||
            !long.TryParse(fields[4], NumberStyles.None, CultureInfo.InvariantCulture, out long revision) ||
            !BunnyInvasionWireState.ValidName(fields[2], 128) ||
            !BunnyInvasionWireState.ValidName(fields[3], 128) ||
            fields[5].Length is < 2 or > 192 || fields[5].Length % 2 != 0)
            return false;
        byte[] bytes;
        try { bytes = Convert.FromHexString(fields[5]); }
        catch (FormatException) { return false; }
        state = new(session, fields[2], fields[3], revision, bytes);
        return state.IsValid();
    }
}

public sealed class BunnyWorldNetClientView
{
    private long localSession;
    private long remoteSession;
    private long revision;
    public bool HasState { get; private set; }

    public bool Accept(BunnyWorldNetState state, long localWorldSession,
        string modId, string systemName)
    {
        if (!state.IsValid() || localWorldSession <= 0 ||
            state.ModId != modId || state.SystemName != systemName) return false;
        if (localSession != localWorldSession)
        {
            Reset();
            localSession = localWorldSession;
        }
        if (HasState && (remoteSession != state.HostSession ||
                         state.Revision <= revision)) return false;
        remoteSession = state.HostSession;
        revision = state.Revision;
        HasState = true;
        return true;
    }

    public void Reset()
    {
        localSession = 0;
        remoteSession = 0;
        revision = 0;
        HasState = false;
    }
}

internal static class BunnyWorldNetwork
{
    internal static bool Publish(long session, BunnyWorldNetState snapshot)
    {
        if (session <= 0 || !snapshot.IsValid() ||
            snapshot.HostSession != session) return false;
        try { return Native.Publish(session, snapshot.Encode()) == 1; }
        catch (DllNotFoundException) { return false; }
        catch (EntryPointNotFoundException) { return false; }
    }

    internal static bool TryPoll(long session, string modId,
        out BunnyWorldNetState state)
    {
        state = default;
        if (session <= 0 || !BunnyInvasionWireState.ValidName(modId, 128))
            return false;
        var buffer = new byte[301];
        int size;
        try { size = Native.Poll(session, modId, buffer, buffer.Length); }
        catch (DllNotFoundException) { return false; }
        catch (EntryPointNotFoundException) { return false; }
        if (size <= 0 || size > 300) return false;
        return BunnyWorldNetState.TryDecode(
            Encoding.ASCII.GetString(buffer, 0, size), out state) &&
            state.ModId == modId;
    }

    private static class Native
    {
        [DllImport("bunny", EntryPoint = "bunny_world_system_net_publish",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Publish(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string encoded);

        [DllImport("bunny", EntryPoint = "bunny_world_system_net_poll",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Poll(long session,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
            [Out] byte[] output, int capacity);
    }
}
