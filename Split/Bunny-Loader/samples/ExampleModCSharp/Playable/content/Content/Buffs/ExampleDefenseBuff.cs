using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleDefenseBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense";
        Texture = "Content/Buffs/ExampleDefenseBuff.png";
        DefenseBonus = 10;
    }
}
