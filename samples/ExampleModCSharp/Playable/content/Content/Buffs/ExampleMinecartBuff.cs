using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinecartBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName="Example Minecart";
        Texture="Content/Buffs/ExampleMinecartBuff.png";
        NoSave=true;
        NoTimeDisplay=true;
    }
}
