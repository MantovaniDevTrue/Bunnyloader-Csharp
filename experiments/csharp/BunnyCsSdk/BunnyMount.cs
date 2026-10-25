using System;
using System.Globalization;
using System.Linq;

namespace BunnyLoader.Source;

public enum BunnyMountKind
{
    Ride = 0,
    Minecart = 1
}

public readonly record struct BunnyMountFrames(int Count,int Delay,int Start)
{
    public void Validate()
    {
        if(Count<0||Count>64||Delay<0||Delay>600||Start<0||Start>63||
           (Count==0&&Delay!=0)||(Count>0&&Delay==0)||(Start+Count>64))
            throw new ArgumentOutOfRangeException(nameof(BunnyMountFrames));
    }
}

public abstract class BunnyMount
{
    public string Texture { get; set; } = "";
    public string Buff { get; set; } = "";
    public string SpawnDust { get; set; } = "GemDiamond";
    public BunnyMountKind Kind { get; set; }
    public int JumpHeight { get; set; } = 5;
    public float Acceleration { get; set; } = 0.19f;
    public float JumpSpeed { get; set; } = 4f;
    public bool BlockExtraJumps { get; set; }
    public bool ConstantJump { get; set; } = true;
    public int HeightBoost { get; set; } = 20;
    public float FallDamage { get; set; } = 0.5f;
    public float RunSpeed { get; set; } = 11f;
    public float DashSpeed { get; set; } = 8f;
    public int FlightTimeMax { get; set; }
    public int FatigueMax { get; set; }
    public int TotalFrames { get; set; } = 4;
    public int PlayerYOffset { get; set; } = 20;
    public int XOffset { get; set; } = 13;
    public int YOffset { get; set; } = -12;
    public int PlayerHeadOffset { get; set; } = 22;
    public int BodyFrame { get; set; } = 3;
    public BunnyMountFrames Standing { get; set; } = new(4,12,0);
    public BunnyMountFrames Running { get; set; } = new(4,12,0);
    public BunnyMountFrames Flying { get; set; }
    public BunnyMountFrames InAir { get; set; } = new(1,12,0);
    public BunnyMountFrames Idle { get; set; } = new(4,12,0);
    public BunnyMountFrames Swimming { get; set; } = new(1,12,0);
    public bool IdleFrameLoop { get; set; } = true;
    public string BalloonTexture { get; set; } = "";
    public int BalloonCount { get; set; }
    public int SpawnRingDustCount { get; set; }
    public int SpawnRingRadius { get; set; }
    public int RunDustThreshold { get; set; }
    public int AmbientDustChance { get; set; }
    public bool ReuseVanillaMinecartDelegates { get; set; }

    public abstract void SetDefaults();

    private static void Path(string value,bool optional)
    {
        if(optional&&value.Length==0)return;
        if(string.IsNullOrWhiteSpace(value)||value.Length>200||
           value.StartsWith("/",StringComparison.Ordinal)||
           value.EndsWith("/",StringComparison.Ordinal)||
           value.Contains("//",StringComparison.Ordinal)||
           value.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_'||c=='-'||c=='/')))
            throw new ArgumentException("Invalid mount texture path",nameof(value));
    }

    private static void Name(string value)
    {
        if(string.IsNullOrWhiteSpace(value)||value.Length>72||
           value.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_')))
            throw new ArgumentException("Invalid mount content name",nameof(value));
    }

    private static void Finite(float value,float minimum,float maximum)
    {
        if(!float.IsFinite(value)||value<minimum||value>maximum)
            throw new ArgumentOutOfRangeException(nameof(value));
    }

    public void Validate()
    {
        if(!Enum.IsDefined(typeof(BunnyMountKind),Kind))
            throw new ArgumentOutOfRangeException(nameof(Kind));
        Path(Texture,false);
        Path(BalloonTexture,true);
        Name(Buff);
        Name(SpawnDust);
        if(JumpHeight<0||JumpHeight>100||HeightBoost<0||HeightBoost>200||
           FlightTimeMax<0||FlightTimeMax>36000||FatigueMax<0||FatigueMax>36000||
           TotalFrames<1||TotalFrames>64||PlayerYOffset< -100||PlayerYOffset>100||
           XOffset< -100||XOffset>100||YOffset< -100||YOffset>100||
           PlayerHeadOffset< -100||PlayerHeadOffset>100||BodyFrame<0||BodyFrame>=TotalFrames||
           BalloonCount<0||BalloonCount>8||SpawnRingDustCount<0||SpawnRingDustCount>128||
           SpawnRingRadius<0||SpawnRingRadius>512||RunDustThreshold<0||RunDustThreshold>100||
           AmbientDustChance<0||AmbientDustChance>10000)
            throw new ArgumentOutOfRangeException(nameof(BunnyMount));
        Finite(Acceleration,0,100);
        Finite(JumpSpeed,0,100);
        Finite(FallDamage,0,100);
        Finite(RunSpeed,0,100);
        Finite(DashSpeed,0,100);
        Standing.Validate();Running.Validate();Flying.Validate();
        InAir.Validate();Idle.Validate();Swimming.Validate();
        foreach(var frame in new[]{Standing,Running,Flying,InAir,Idle,Swimming})
            if(frame.Start+frame.Count>TotalFrames)
                throw new ArgumentOutOfRangeException(nameof(TotalFrames));
        if((BalloonCount>0)!=(BalloonTexture.Length>0))
            throw new ArgumentException("Balloon count must match texture");
        if(Kind==BunnyMountKind.Minecart &&
           (BalloonCount!=0||SpawnRingDustCount!=0||!ReuseVanillaMinecartDelegates))
            throw new ArgumentException("Minecart must use vanilla native mount defaults");
        if(Kind==BunnyMountKind.Ride&&ReuseVanillaMinecartDelegates)
            throw new ArgumentException("Ride must not use minecart delegates");
    }

    public string NativeDefinition
    {
        get
        {
            Validate();
            string I(int value)=>value.ToString(CultureInfo.InvariantCulture);
            string F(float value)=>value.ToString("R",CultureInfo.InvariantCulture);
            string B(bool value)=>value?"1":"0";
            string Frame(BunnyMountFrames frame)=>I(frame.Count)+","+I(frame.Delay)+","+I(frame.Start);
            return string.Join("|",new[]{
                "M1",I((int)Kind),Texture,Buff,SpawnDust,I(JumpHeight),F(Acceleration),
                F(JumpSpeed),B(BlockExtraJumps),B(ConstantJump),I(HeightBoost),
                F(FallDamage),F(RunSpeed),F(DashSpeed),I(FlightTimeMax),I(FatigueMax),
                I(TotalFrames),I(PlayerYOffset),I(XOffset),I(YOffset),
                I(PlayerHeadOffset),I(BodyFrame),Frame(Standing),Frame(Running),
                Frame(Flying),Frame(InAir),Frame(Idle),Frame(Swimming),
                B(IdleFrameLoop),BalloonTexture.Length==0?"-":BalloonTexture,
                I(BalloonCount),I(SpawnRingDustCount),I(SpawnRingRadius),
                I(RunDustThreshold),I(AmbientDustChance),B(ReuseVanillaMinecartDelegates)
            });
        }
    }
}
