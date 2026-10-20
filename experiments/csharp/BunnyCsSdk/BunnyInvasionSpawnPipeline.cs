using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

public sealed class BunnyInvasionSpawnPipeline
{
    private struct Pending
    {
        public int Ticket;
        public string NpcClass;
        public int Slot;
        public long Identity;
        public long IssuedTick;
    }

    private readonly Dictionary<int, Pending> pending = new();
    private readonly Dictionary<int, int> slots = new();
    private readonly List<int> expired = new();
    private long tick;
    private readonly int timeoutTicks;

    public BunnyInvasionController Controller { get; }
    public int PendingCount => pending.Count;
    public int ConfirmedCount { get; private set; }
    public int FailedCount { get; private set; }
    public int ExpiredCount { get; private set; }
    public int RejectedCount { get; private set; }

    public BunnyInvasionSpawnPipeline(BunnyInvasionController controller, int timeoutTicks = 600)
    {
        Controller = controller ?? throw new ArgumentNullException(nameof(controller));
        if (timeoutTicks < 1 || timeoutTicks > 36000)
            throw new ArgumentOutOfRangeException(nameof(timeoutTicks));
        this.timeoutTicks = timeoutTicks;
    }

    public void Tick(bool authoritative, bool playersAvailable)
    {
        if (!authoritative || !playersAvailable) return;
        Controller.Tick(true, true);
        if (pending.Count == 0) return;
        if (tick < long.MaxValue) tick++;
        expired.Clear();
        foreach (var entry in pending)
        {
            if (Controller.Phase != BunnyInvasionPhase.Running ||
                tick - entry.Value.IssuedTick >= timeoutTicks)
                expired.Add(entry.Key);
        }
        foreach (int ticket in expired)
        {
            if (!pending.TryGetValue(ticket, out Pending value)) continue;
            if (value.Slot >= 0) slots.Remove(value.Slot);
            pending.Remove(ticket);
            Controller.CancelSpawn(true, ticket);
            ExpiredCount++;
        }
    }

    public bool TryReserve(bool authoritative, int weightedRoll, out BunnyInvasionSpawn spawn)
    {
        spawn = default;
        if (!authoritative ||
            !Controller.TryReserveSpawn(true, weightedRoll, out spawn)) return false;
        pending.Add(spawn.Ticket, new Pending {
            Ticket = spawn.Ticket, NpcClass = spawn.NpcClass,
            Slot = -1, Identity = 0, IssuedTick = tick
        });
        return true;
    }

    public bool ReportCreated(bool authoritative, int ticket,
        string npcClass, int slot, long nativeIdentity)
    {
        if (!authoritative || Controller.Phase != BunnyInvasionPhase.Running ||
            slot < 0 || slot >= 512 || nativeIdentity == 0 ||
            !pending.TryGetValue(ticket, out Pending claim) ||
            claim.NpcClass != npcClass || claim.Slot != -1 ||
            slots.ContainsKey(slot))
        {
            RejectedCount++;
            return false;
        }

        claim.Slot = slot;
        claim.Identity = nativeIdentity;
        pending[ticket] = claim;
        slots.Add(slot, ticket);
        return true;
    }

    public bool ConfirmFirstTick(bool authoritative, string npcClass,
        int slot, int generation, long nativeIdentity)
    {
        if (!authoritative || generation <= 0 || nativeIdentity == 0 ||
            !slots.TryGetValue(slot, out int ticket) ||
            !pending.TryGetValue(ticket, out Pending claim) ||
            claim.Slot != slot || claim.Identity != nativeIdentity ||
            claim.NpcClass != npcClass)
        {
            RejectedCount++;
            return false;
        }

        slots.Remove(slot);
        pending.Remove(ticket);
        if (!Controller.CommitSpawn(true, ticket, npcClass, slot, generation))
        {
            Controller.CancelSpawn(true, ticket);
            FailedCount++;
            return false;
        }
        ConfirmedCount++;
        return true;
    }

    public bool ReportFailed(bool authoritative, int ticket)
    {
        if (!authoritative || !pending.TryGetValue(ticket, out Pending claim))
            return false;
        if (claim.Slot >= 0) slots.Remove(claim.Slot);
        pending.Remove(ticket);
        if (!Controller.CancelSpawn(true, ticket)) return false;
        FailedCount++;
        return true;
    }

    public void ResetWorld()
    {
        pending.Clear();
        slots.Clear();
        expired.Clear();
        tick = 0;
        Controller.ResetWorld();
    }
}
