using System;

namespace BunnyLoader.Source;

public abstract class BunnyInvasionSystem
{
    public abstract BunnyInvasionSpawnPipeline Pipeline { get; }
    public string Id => Pipeline.Controller.Definition.Id;
    public virtual string RewardItemClass => "";
    public virtual int RewardStack => 0;

    public virtual void OnWorldExit() => Pipeline.ResetWorld();

    public virtual void Tick(bool authoritative, int frames, bool playerAvailable)
    {
        for (int i = 0; i < frames; i++)
            Pipeline.Tick(authoritative, playerAvailable);
    }

    public virtual bool TryReserve(bool authoritative, int roll, out BunnyInvasionSpawn spawn) =>
        Pipeline.TryReserve(authoritative, roll, out spawn);

    public virtual bool OnNativeCreated(bool authoritative, BunnyInvasionSpawn spawn,
        int slot, long identity) =>
        Pipeline.ReportCreated(authoritative, spawn.Epoch, spawn.Ticket,
            spawn.NpcClass, slot, identity);

    public virtual bool OnNativeFailed(bool authoritative, int epoch, int ticket) =>
        Pipeline.ReportFailed(authoritative, epoch, ticket);
}
