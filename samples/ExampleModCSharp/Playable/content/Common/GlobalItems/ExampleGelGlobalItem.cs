// ExampleMod C# | Common/GlobalItems/ExampleGelGlobalItem.cs
// Porte do GelGlobalItem do ExampleMod do tModLoader: linha extra no tooltip do Gel.
using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGelGlobalItem : BunnyGlobalItem
{
    private const int Gel = 23; // ItemID.Gel

    public override bool AppliesToEntity(BunnyVanillaItem item) => item.Type == Gel;

    public override void ModifyTooltips(BunnyVanillaItem item, List<BunnyTooltipLine> tooltips)
    {
        tooltips.Add(new BunnyTooltipLine("ExampleGel", "Linha adicionada por um GlobalItem C#"));
    }
}
