using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinionBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Minion";
        Texture = "Content/Buffs/ExampleMinionBuff.png";
        Description = "The example minion will fight for you";
        MinionProjectile = nameof(ExampleMinion);
        NoSave = true;
        NoTimeDisplay = true;
    }
}
