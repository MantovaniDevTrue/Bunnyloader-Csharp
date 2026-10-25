using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class Blocky : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Blocky";
        Texture = "Content/Buffs/Blocky.png";
        Description = "Jumping power is increased";
        IsDebuff = true;
        NoSave = true;
        NoTimeDisplay = true;
        NurseCannotRemove = true;
    }
}
