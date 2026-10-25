using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMountBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "ExampleMount";
        Texture = "Content/Buffs/ExampleMountBuff.png";
        Description = "Leather seats, 4 cup holders";
        NoSave = true;
        NoTimeDisplay = true;
    }
}
