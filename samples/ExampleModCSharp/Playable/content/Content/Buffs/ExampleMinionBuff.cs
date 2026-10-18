using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinionBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Minion";
        Texture = "Content/Buffs/ExampleMinionBuff.png";
        Description = "Um minion C# procura e ataca inimigos próximos";
        MinionProjectile = nameof(ExampleMinion);
        NoSave = true;
        NoTimeDisplay = true;
    }
}
