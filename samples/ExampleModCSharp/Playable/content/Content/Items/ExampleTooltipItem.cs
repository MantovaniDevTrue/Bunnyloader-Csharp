using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTooltipItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/ExampleTooltipItem.png";
        DisplayName = "Exemplo de Tooltip";
        Tooltip = "Acima, uma cor por letra. Esta linha, em dourado";
        Width = 20;
        Height = 20;
        MaxStack = 9999;
        Rare = 1;
        Value = BuyPrice(silver: 1);
    }

    public override void ModifyTooltips(List<BunnyTooltipLine> lines)
    {
        lines.Insert(1, new BunnyTooltipLine("BunnyLoader", "Bunny Loader"));
        foreach (var line in lines)
            if (line.Name == "Tooltip0")
                line.OverrideColor = "FFD75A";
    }

    public override bool PreDrawTooltipLine(BunnyDrawableTooltipLine line)
    {
        if (line.Name != "BunnyLoader") return true;
        line.RainbowWave = true;
        line.WaveAmplitude = 3f;
        line.WaveSpeed = 8f;
        line.LetterStep = 1.15f;
        line.HueStep = 0.04f;
        line.HueSpeed = 0.20f;
        return false;
    }
}
