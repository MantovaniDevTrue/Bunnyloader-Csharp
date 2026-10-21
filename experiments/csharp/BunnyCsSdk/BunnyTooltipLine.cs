using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

public sealed class BunnyTooltipLine
{
    public string Name { get; set; }
    public string Text { get; set; }
    public bool Visible { get; set; } = true;
    public string? OverrideColor { get; set; }

    public BunnyTooltipLine(string name, string text)
    {
        Name = name;
        Text = text;
    }
}

public sealed class BunnyDrawableTooltipLine
{
    public string Name { get; }
    public bool DrawDefault { get; set; } = true;
    public bool RainbowWave { get; set; }
    public float WaveAmplitude { get; set; } = 3f;
    public float WaveSpeed { get; set; } = 8f;
    public float LetterStep { get; set; } = 1.15f;
    public float HueStep { get; set; } = 0.04f;
    public float HueSpeed { get; set; } = 0.20f;

    public BunnyDrawableTooltipLine(string name) => Name = name;
}

public sealed class BunnyTooltipPlan
{
    public List<BunnyTooltipLine> Lines { get; } = new();
    public Dictionary<string, BunnyDrawableTooltipLine> Draw { get; } =
        new(StringComparer.Ordinal);
}
