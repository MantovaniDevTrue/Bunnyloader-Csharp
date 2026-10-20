using BunnyLoader.Source;

namespace ExampleModCSharp;

public static class ExampleInvasion
{
    public static readonly BunnyInvasionController Controller = new(
        new BunnyInvasionDefinition(
            "example-invasion",
            30,
            8,
            120,
            new BunnyInvasionEnemy(nameof(ExampleSlimeNPC), 3, 1),
            new BunnyInvasionEnemy(nameof(ExampleInvasionEliteSlimeNPC), 1, 3, 10)));

    public static readonly BunnyInvasionSpawnPipeline Pipeline = new(Controller);

    public static bool Start(bool serverAuthority) =>
        Controller.Start(serverAuthority);

    public static bool TrySpawn(bool serverAuthority, int randomRoll,
        out BunnyInvasionSpawn spawn) =>
        Pipeline.TryReserve(serverAuthority, randomRoll, out spawn);

    public static bool NativeSpawnCreated(bool serverAuthority, int epoch,
        int ticket, int npcSlot, long nativeIdentity) =>
        Pipeline.TryGetReservedClass(epoch, ticket, out string npcClass) &&
        Pipeline.ReportCreated(serverAuthority, epoch, ticket,
            npcClass, npcSlot, nativeIdentity);

    public static bool NativeSpawnFailed(bool serverAuthority, int epoch, int ticket) =>
        Pipeline.ReportFailed(serverAuthority, epoch, ticket);

    public static bool ConfirmFirstTick(bool serverAuthority,
        string npcClass, ref BunnyNpcAiState state) =>
        Pipeline.MatchesPendingNpc(npcClass, state.Slot, state.NativeIdentity) &&
        Pipeline.ConfirmFirstTick(serverAuthority, Controller.Snapshot().Epoch,
            npcClass, state.Slot, state.Generation, state.NativeIdentity);

    public static bool EnemyKilled(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportKill(serverAuthority, npcSlot, npcGeneration,
            nameof(ExampleSlimeNPC)) ||
        Controller.ReportKill(serverAuthority, npcSlot, npcGeneration,
            nameof(ExampleInvasionEliteSlimeNPC));

    public static bool EnemyDespawned(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportDespawn(serverAuthority, npcSlot, npcGeneration);

    public static void WorldExit() => Pipeline.ResetWorld();
}
