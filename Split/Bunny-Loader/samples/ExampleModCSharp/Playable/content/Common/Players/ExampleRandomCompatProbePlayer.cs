using System;
using BunnyLoader.Source;
using Terraria;
using Terraria.Utilities;

namespace ExampleModCSharp;

public sealed class ExampleRandomCompatProbePlayer : BunnyPlayer
{
    public override void SetDefaults() { }

    public override void UpdateEquips(bool accessoryEquipped) { }

    public override string OnEnterWorld()
    {
        var seedA = new UnifiedRandom(20261022);
        var seedB = new UnifiedRandom(20261022);
        for (int i = 0; i < 32; i++)
        {
            if (seedA.Next() != seedB.Next())
                return "Bunny C# Random: FAIL seed";
        }

        seedA.SetSeed(71);
        seedB = new UnifiedRandom(71);
        if (seedA.NextDouble() != seedB.NextDouble())
            return "Bunny C# Random: FAIL SetSeed";

        var table = new WeightedRandom<int>(new UnifiedRandom(42));
        table.Add(100, 0);
        table.Add(200, 1);
        for (int i = 0; i < 32; i++)
        {
            if (table.Get() != 200)
                return "Bunny C# Random: FAIL WeightedRandom";
        }

        if (!seedA.NextBool(1) || seedA.NextBool(0, 5))
            return "Bunny C# Random: FAIL NextBool";

        var value = seedA.NextFloat(-3f, 7f);
        if (value < -3f || value > 7f)
            return "Bunny C# Random: FAIL NextFloat";

        var vector = seedA.NextVector2Unit();
        if (Math.Abs(vector.Length() - 1f) > 0.0001f)
            return "Bunny C# Random: FAIL Vector2";

        int selected = seedA.NextFromList(1, 2, 3);
        if (selected < 1 || selected > 3)
            return "Bunny C# Random: FAIL List";

        return "Bunny C# Random Compat: PASS (seed/weights/bool/float/vector/list)";
    }
}
