using System;
using System.IO;
using BunnyLoader.Source;

internal static class Program
{
    private const string ModA = "00000000-0000-4000-8000-000000000001";
    private const string ModB = "00000000-0000-4000-8000-000000000002";

    private static void Require(bool condition, string name)
    {
        if (!condition) throw new InvalidOperationException("FAILED: " + name);
        Console.WriteLine("PASS " + name);
    }

    private static void Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "--emit-fixture")
        {
            // Interoperabilidade real: fixture gerada pelo SDK C#, lida pelo POSIX C++.
            var fixture = new BunnyWorldDocument();
            var tag = new BunnyWorldTag();
            tag.SetInt32("count", 42);
            fixture.Replace("00000000-0000-4000-8000-000000000001",
                            "Example.Progression", tag);
            File.WriteAllBytes(args[1], fixture.Encode());
            return;
        }
        if (args.Length != 0) throw new ArgumentException("Unsupported test argument");
        var document = new BunnyWorldDocument();
        var boss = new BunnyWorldTag();
        boss.SetBoolean("downedExampleBoss", true);
        boss.SetInt32("bossKills", 12);
        boss.SetString("playerNote", "vitória 🎮");
        document.Replace(ModA, "ExampleMod.DownedBossSystem", boss);

        var other = new BunnyWorldTag();
        other.SetBoolean("otherBoss", true);
        document.Replace(ModB, "AnotherMod.Progression", other);

        var first = document.Encode();
        Require(BunnyWorldDocument.TryDecode(first, out var loaded), "roundtrip accepted");
        var restored = loaded.Read(ModA, "ExampleMod.DownedBossSystem");
        Require(restored.GetBoolean("downedExampleBoss"), "boolean restored");
        Require(restored.GetInt32("bossKills") == 12, "int32 restored");
        Require(restored.GetString("playerNote") == "vitória 🎮", "utf8 restored");
        Require(!restored.GetBoolean("missing") && restored.GetInt32("missing") == 0, "absent fields");
        Require(Convert.ToHexString(first) == Convert.ToHexString(loaded.Encode()), "deterministic encoding");

        boss.SetBoolean("downedExampleBoss", false);
        Require(document.Read(ModA, "ExampleMod.DownedBossSystem").GetBoolean("downedExampleBoss"),
            "defensive input copy");
        restored.Clear();
        Require(loaded.Read(ModA, "ExampleMod.DownedBossSystem").Count == 3,
            "defensive output copy");

        loaded.Replace(ModA, "ExampleMod.DownedBossSystem", new BunnyWorldTag());
        Require(loaded.SystemCount == 1, "empty system removed");
        Require(loaded.Read(ModB, "AnotherMod.Progression").GetBoolean("otherBoss"),
            "absent mod preserved");

        var corrupted = (byte[])first.Clone();
        corrupted[corrupted.Length - 5] ^= 0x3F;
        Require(!BunnyWorldDocument.TryDecode(corrupted, out var rejected) &&
                rejected.SystemCount == 0, "checksum rejects corruption");
        Require(!BunnyWorldDocument.TryDecode(first.AsSpan(0, first.Length - 1), out _),
            "truncated payload rejected");
        var trailing = new byte[first.Length + 1];
        Array.Copy(first, trailing, first.Length);
        Require(!BunnyWorldDocument.TryDecode(trailing, out _), "trailing bytes rejected");
        var invalidText = new BunnyWorldTag();
        var oversized = new string('a', BunnyWorldDocument.MaxTextBytes + 1);
        bool bounded = false;
        try { invalidText.SetString("tooBig", oversized); }
        catch (ArgumentOutOfRangeException) { bounded = true; }
        Require(bounded, "string bound enforced");

        var newWorld = new BunnyWorldDocument();
        Require(!newWorld.Read(ModA, "ExampleMod.DownedBossSystem").GetBoolean("downedExampleBoss"),
            "fresh world state is empty");
        newWorld.Replace(ModA, "ExampleMod.DownedBossSystem", boss);
        newWorld.Clear();
        Require(newWorld.SystemCount == 0, "world transition clears state");

        // Este teste de fonte não confirma hooks IL2CPP ou escrita no Android.
        DispatcherTests.Run();
        Console.WriteLine("WORLD_CODEC_SOURCE_TESTS_PASS");
    }
}
