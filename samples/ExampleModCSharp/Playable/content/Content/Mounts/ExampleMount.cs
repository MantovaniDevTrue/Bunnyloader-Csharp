using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Mounts;

public sealed class ExampleMount : BunnyMount
{
    public override void SetDefaults()
    {
        Texture="Content/Mounts/ExampleMount";
        Buff="ExampleMountBuff";
        SpawnDust="GemDiamond";
        Kind=BunnyMountKind.Ride;
        JumpHeight=5;
        Acceleration=0.19f;
        JumpSpeed=4;
        BlockExtraJumps=false;
        ConstantJump=true;
        HeightBoost=20;
        FallDamage=0.5f;
        RunSpeed=11;
        DashSpeed=8;
        FlightTimeMax=0;
        FatigueMax=0;
        TotalFrames=4;
        PlayerYOffset=20;
        XOffset=13;
        YOffset=-12;
        PlayerHeadOffset=22;
        BodyFrame=3;
        Standing=new(4,12,0);
        Running=new(4,12,0);
        Flying=new(0,0,0);
        InAir=new(1,12,0);
        Idle=new(4,12,0);
        Swimming=new(1,12,0);
        IdleFrameLoop=true;
        BalloonTexture="Content/Items/Armor/SimpleAccessory_Balloon";
        BalloonCount=3;
        SpawnRingDustCount=16;
        SpawnRingRadius=80;
        RunDustThreshold=4;
    }
}
