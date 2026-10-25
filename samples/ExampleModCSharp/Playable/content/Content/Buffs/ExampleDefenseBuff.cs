using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleDefenseBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense";
        Texture = "Content/Buffs/ExampleDefenseBuff.png";
        DefenseBonus = 10;
        Description = "Increases the player's defense by {0}";
    }
}
