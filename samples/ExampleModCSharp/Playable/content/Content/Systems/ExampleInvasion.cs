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
            new BunnyInvasionEnemy(nameof(ExampleSlimeNPC), 1, 1)));

    public static bool Start(bool serverAuthority) =>
        Controller.Start(serverAuthority);

    public static bool TrySpawn(bool serverAuthority, int randomRoll,
        out BunnyInvasionSpawn spawn) =>
        Controller.TryReserveSpawn(serverAuthority, randomRoll, out spawn);

    public static bool ConfirmSpawn(bool serverAuthority, int ticket,
        int npcSlot, int npcGeneration) =>
        Controller.CommitSpawn(serverAuthority, ticket,
            nameof(ExampleSlimeNPC), npcSlot, npcGeneration);

    public static bool SpawnFailed(bool serverAuthority, int ticket) =>
        Controller.CancelSpawn(serverAuthority, ticket);

    public static bool EnemyKilled(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportKill(serverAuthority, npcSlot, npcGeneration,
            nameof(ExampleSlimeNPC));

    public static bool EnemyDespawned(bool serverAuthority, int npcSlot, int npcGeneration) =>
        Controller.ReportDespawn(serverAuthority, npcSlot, npcGeneration);

    public static void WorldExit() => Controller.ResetWorld();
}
