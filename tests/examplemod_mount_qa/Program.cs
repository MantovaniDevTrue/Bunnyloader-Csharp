using BunnyLoader.Source;
using ExampleModMountQa;
using System.Globalization;

static void Check(bool condition,string name)
{
    if(!condition)throw new InvalidOperationException(name);
}

var ride=new ExampleMount();
ride.SetDefaults();
ride.Validate();
Check(ride.Kind==BunnyMountKind.Ride&&ride.Buff=="ExampleMountBuff","Ride reference");
Check(ride.JumpHeight==5&&ride.Acceleration==0.19f&&ride.JumpSpeed==4f,"Ride jump");
Check(!ride.BlockExtraJumps&&ride.ConstantJump&&ride.HeightBoost==20,"Ride jump policy");
Check(ride.FallDamage==0.5f&&ride.RunSpeed==11&&ride.DashSpeed==8,"Ride locomotion");
Check(ride.FlightTimeMax==0&&ride.FatigueMax==0,"Ride stamina");
Check(ride.TotalFrames==4&&ride.PlayerYOffset==20&&ride.XOffset==13&&ride.YOffset==-12,"Ride frames");
Check(ride.PlayerHeadOffset==22&&ride.BodyFrame==3&&ride.IdleFrameLoop,"Ride drawing offsets");
Check(ride.Standing==new BunnyMountFrames(4,12,0) &&
      ride.Running==new BunnyMountFrames(4,12,0) &&
      ride.Flying==new BunnyMountFrames(0,0,0) &&
      ride.InAir==new BunnyMountFrames(1,12,0) &&
      ride.Idle==new BunnyMountFrames(4,12,0) &&
      ride.Swimming==new BunnyMountFrames(1,12,0),"Ride frame timings");
Check(ride.BalloonCount==3&&ride.SpawnRingDustCount==16&&ride.SpawnRingRadius==80&&
      ride.RunDustThreshold==4,"Ride particles and balloons");
Check(ride.BalloonTexture=="Content/Items/Armor/SimpleAccessory_Balloon","Ride balloon sprite");
var rideWire=ride.NativeDefinition.Split('|');
Check(rideWire.Length==36&&rideWire[0]=="M1"&&rideWire[1]=="0","Ride wire version");
Check(rideWire[6]=="0.19"&&rideWire[11]=="0.5"&&rideWire[22]=="4,12,0","Ride wire fields");
Check(rideWire[24]=="0,0,0"&&rideWire[29]==ride.BalloonTexture,"Ride wire animation");
var cart=new ExampleMinecartMount();
cart.SetDefaults();
cart.Validate();
Check(cart.Kind==BunnyMountKind.Minecart&&cart.Buff=="ExampleMinecartBuff","Cart reference");
Check(cart.SpawnDust=="21"&&cart.AmbientDustChance==10,"Cart particles");
Check(cart.ReuseVanillaMinecartDelegates&&cart.BalloonCount==0,"Cart delegates");
var cartWire=cart.NativeDefinition.Split('|');
Check(cartWire.Length==36&&cartWire[1]=="1"&&cartWire[35]=="1","Cart native descriptor");
static void Fails(Action action,string label)
{
    try{action();}catch(ArgumentException){return;}
    throw new InvalidOperationException(label);
}
ride.Buff="../Blocky";
Fails(ride.Validate,"Buff path");
ride.Buff="ExampleMountBuff";
ride.BalloonTexture="../textures";
Fails(ride.Validate,"Balloon path");
ride.BalloonTexture="Content/Items/Armor/SimpleAccessory_Balloon";
ride.Standing=new(8,12,0);
Fails(ride.Validate,"Frame boundary");
ride.Standing=new(4,12,0);
ride.Acceleration=float.NaN;
Fails(ride.Validate,"Non-finite movement");
ride.Acceleration=0.19f;
ride.BalloonCount=9;
Fails(ride.Validate,"Balloon limit");
cart.BalloonCount=1;
Fails(cart.Validate,"Minecart cannot carry balloon declaration");
Console.WriteLine("EXAMPLEMOD_MOUNT_SOURCE_DESCRIPTOR_HOST_PASS no_android_mount_registration=true");
