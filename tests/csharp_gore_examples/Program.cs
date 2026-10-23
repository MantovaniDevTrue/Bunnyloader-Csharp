using System;
using BunnyLoader.Source;
using ExampleModCSharp;

internal static class Program
{
    private static void Main()
    {
        // A host test has no Terraria Gore ABI. An optional visual must report
        // rejection without aborting the NPC death callback and invasion score.
        var npc = new ExampleSlimeNPC();
        npc.SetDefaults();
        var state = new BunnyNpcAiState();
        int before = ExampleSlimeNPC.KillEvents;
        npc.OnKill(ref state);
        if (ExampleSlimeNPC.KillEvents != before + 1 || BunnyGore.LastHostError.Length == 0)
            throw new InvalidOperationException("Optional gore interrupted OnKill or hid host absence");
        if (BunnyGore.TryConfigure("host-test", "Missing", new BunnyGoreSettings()))
            throw new InvalidOperationException("Host without Gore ABI accepted configuration");
        if (BunnyGore.TrySpawn("host-test", "Missing", float.NaN, 0, 0, 0))
            throw new InvalidOperationException("Invalid position accepted");
        bool rejected = false;
        try { BunnyGore.TrySpawn("host-test", "../escape", 0, 0, 0, 0); }
        catch (ArgumentException) { rejected = true; }
        if (!rejected) throw new InvalidOperationException("Invalid identity accepted");
        Console.WriteLine("GORE_OPTIONAL_HOST_CALLBACK_ISOLATION_PASS");
    }
}
