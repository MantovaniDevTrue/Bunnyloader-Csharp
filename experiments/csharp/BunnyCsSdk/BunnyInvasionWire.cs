using System;
using System.Globalization;

namespace BunnyLoader.Source;

public readonly record struct BunnyInvasionWireState(
    long WorldSession, string ModId, string InvasionId,
    int Epoch, int Revision, BunnyInvasionPhase Phase,
    int Progress, int Goal, int Active, int Pending)
{
    public string Encode() =>
        string.Concat("INV1|",
            WorldSession.ToString(CultureInfo.InvariantCulture), "|",
            ModId, "|", InvasionId, "|",
            Epoch.ToString(CultureInfo.InvariantCulture), "|",
            Revision.ToString(CultureInfo.InvariantCulture), "|",
            ((int)Phase).ToString(CultureInfo.InvariantCulture), "|",
            Progress.ToString(CultureInfo.InvariantCulture), "|",
            Goal.ToString(CultureInfo.InvariantCulture), "|",
            Active.ToString(CultureInfo.InvariantCulture), "|",
            Pending.ToString(CultureInfo.InvariantCulture));

    public static bool TryDecode(string? payload, out BunnyInvasionWireState state)
    {
        state = default;
        if (string.IsNullOrEmpty(payload) || payload.Length > 300 ||
            !payload.StartsWith("INV1|", StringComparison.Ordinal)) return false;
        string[] fields = payload.Split('|');
        if (fields.Length != 11 ||
            !long.TryParse(fields[1], NumberStyles.None, CultureInfo.InvariantCulture, out long session) ||
            !int.TryParse(fields[4], NumberStyles.None, CultureInfo.InvariantCulture, out int epoch) ||
            !int.TryParse(fields[5], NumberStyles.None, CultureInfo.InvariantCulture, out int revision) ||
            !int.TryParse(fields[6], NumberStyles.None, CultureInfo.InvariantCulture, out int phase) ||
            !int.TryParse(fields[7], NumberStyles.None, CultureInfo.InvariantCulture, out int progress) ||
            !int.TryParse(fields[8], NumberStyles.None, CultureInfo.InvariantCulture, out int goal) ||
            !int.TryParse(fields[9], NumberStyles.None, CultureInfo.InvariantCulture, out int active) ||
            !int.TryParse(fields[10], NumberStyles.None, CultureInfo.InvariantCulture, out int pending))
            return false;
        state = new(session, fields[2], fields[3], epoch, revision,
            (BunnyInvasionPhase)phase, progress, goal, active, pending);
        return state.IsValid();
    }

    public bool IsValid()
    {
        if (WorldSession <= 0 || !ValidName(ModId, 128) ||
            !ValidName(InvasionId, 64) || Epoch < 0 || Revision < 0 ||
            Goal < 1 || Goal > 1000000 ||
            Progress < 0 || Progress > Goal || Active < 0 || Active > 64 ||
            Pending < 0 || Pending > 64 || Active + Pending > 64 ||
            !Enum.IsDefined(Phase)) return false;
        if (Phase == BunnyInvasionPhase.Completed && Progress != Goal) return false;
        if (Phase == BunnyInvasionPhase.Running && Progress == Goal) return false;
        if (Phase == BunnyInvasionPhase.Inactive &&
            (Progress != 0 || Active != 0 || Pending != 0)) return false;
        return true;
    }

    public static bool ValidName(string? value, int limit)
    {
        if (string.IsNullOrEmpty(value) || value.Length > limit) return false;
        foreach (char ch in value)
        {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
        }
        return true;
    }

    public static BunnyInvasionWireState Capture(
        long worldSession, string modId, BunnyInvasionController controller)
    {
        BunnyInvasionSnapshot snapshot = controller.Snapshot();
        return new BunnyInvasionWireState(worldSession, modId,
            controller.Definition.Id, snapshot.Epoch, snapshot.Revision,
            snapshot.Phase, snapshot.Progress, controller.Definition.Goal,
            snapshot.Active, snapshot.Pending);
    }
}

public sealed class BunnyInvasionClientView
{
    private long session;
    private int epoch = -1;
    private int revision = -1;

    public BunnyInvasionWireState Current { get; private set; }
    public bool HasState { get; private set; }

    public bool ReceiveFromHost(BunnyInvasionWireState state,
        long currentWorldSession, string modId, string invasionId)
    {
        if (!state.IsValid() || currentWorldSession <= 0 ||
            state.WorldSession != currentWorldSession ||
            state.ModId != modId || state.InvasionId != invasionId) return false;
        if (session == state.WorldSession && HasState &&
            (state.Epoch < epoch ||
             (state.Epoch == epoch && state.Revision <= revision))) return false;
        session = state.WorldSession;
        epoch = state.Epoch;
        revision = state.Revision;
        Current = state;
        HasState = true;
        return true;
    }

    private long localWorldSession;
    private long remoteWorldSession;

    public bool ReceiveRemoteFromHost(BunnyInvasionWireState state,
        long localSession, string modId, string invasionId)
    {
        if (!state.IsValid() || localSession <= 0 ||
            state.ModId != modId || state.InvasionId != invasionId) return false;
        if (localWorldSession != localSession)
        {
            ResetWorld();
            localWorldSession = localSession;
        }
        if (HasState &&
            (remoteWorldSession != state.WorldSession ||
             state.Epoch < epoch ||
             (state.Epoch == epoch && state.Revision <= revision))) return false;
        remoteWorldSession = state.WorldSession;
        epoch = state.Epoch;
        revision = state.Revision;
        Current = state;
        HasState = true;
        return true;
    }

    public void ResetWorld()
    {
        session = 0;
        localWorldSession = 0;
        remoteWorldSession = 0;
        epoch = -1;
        revision = -1;
        Current = default;
        HasState = false;
    }
}
