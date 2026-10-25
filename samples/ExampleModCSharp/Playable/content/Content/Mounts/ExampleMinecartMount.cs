using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Mounts;

public sealed class ExampleMinecartMount : BunnyMount
{
    public override void SetDefaults()
    {
        Texture="Content/Mounts/ExampleMinecartMount";
        Buff="ExampleMinecartBuff";
        SpawnDust="21";
        Kind=BunnyMountKind.Minecart;
        BalloonTexture="";
        BalloonCount=0;
        SpawnRingDustCount=0;
        AmbientDustChance=10;
        ReuseVanillaMinecartDelegates=true;
    }
}
