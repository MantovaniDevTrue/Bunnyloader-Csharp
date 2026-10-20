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
        Pipeline.ReportCreated(serverAuthority, epoch, ticket,
            nameof(ExampleSlimeNPC), npcSlot, nativeIdentity);

    public static bool NativeSpawnFailed(bool serverAuthority, int epoch, int ticket) =>
        Pipeline.ReportFailed(serverAuthority, epoch, ticket);

    public static bool ConfirmFirstTick(bool serverAuthority, ref BunnyNpcAiState state) =>
        Pipeline.PendingCount != 0 &&
        Pipeline.ConfirmFirstTick(serverAuthority, Controller.Snapshot().Epoch,
            nameof(ExampleSlimeNPC), state.Slot, state.Generation, state.NativeIdentity);

    public static bool EnemyKilled(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportKill(serverAuthority, npcSlot, npcGeneration,
            nameof(ExampleSlimeNPC));

    public static bool EnemyDespawned(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportDespawn(serverAuthority, npcSlot, npcGeneration);

    public static void WorldExit() => Pipeline.ResetWorld();
}
