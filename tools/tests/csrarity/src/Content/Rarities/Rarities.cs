using System;
using Microsoft.Xna.Framework.Graphics;
using Terraria.ModLoader;

namespace CsRarity.Content.Rarities;

public class Plain : ModRarity
{
    public static int StaticDefaults;

    public override Color RarityColor => new(10, 200, 30);

    public override void SetStaticDefaults() => StaticDefaults++;

    public override int GetPrefixedRarity(int offset, float valueMult) => offset > 0 ? ModContent.RarityType<Shiny>() : Type;
}

public class Shiny : ModRarity
{
    public static int StaticDefaults;

    public override Color RarityColor => new(230, 60, 200);

    public override void SetStaticDefaults() => StaticDefaults++;

    public override int GetPrefixedRarity(int offset, float valueMult) => offset < 0 ? ModContent.RarityType<Plain>() : Type;
}

public class Wild : ModRarity
{
    public override int GetPrefixedRarity(int offset, float valueMult) => 999;
}

public class Broken : ModRarity
{
    public override Color RarityColor => throw new InvalidOperationException("no color");

    public override int GetPrefixedRarity(int offset, float valueMult) => throw new InvalidOperationException("no rarity");
}
