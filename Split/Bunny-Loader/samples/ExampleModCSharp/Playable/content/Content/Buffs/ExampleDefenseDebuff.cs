using BunnyLoader.Source;
namespace ExampleModCSharp;

// Original: ExampleDefenseDebuff.js + ExamplePlayer.PostUpdateEquips.
// A multiplicação acontece depois dos equipamentos, não antes da armadura.
public sealed class ExampleDefenseDebuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Debuff";
        Texture = "Content/Buffs/ExampleDefenseDebuff.png";
        Description = "Reduces total defense by 25%";
        IsDebuff = true;
        DefenseMultiplierPercent = 75;
    }
}
