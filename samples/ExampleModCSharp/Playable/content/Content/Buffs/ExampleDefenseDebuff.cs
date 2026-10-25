using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleDefenseDebuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Debuff";
        Texture = "Content/Buffs/ExampleDefenseDebuff.png";
        Description = "Reduces the player's defense by 25%";
        IsDebuff = true;
        DefenseMultiplierPercent = 75;
    }
}
