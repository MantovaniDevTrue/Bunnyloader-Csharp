using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;

static class Program
{
    static void Check(bool ok,string text){if(!ok)throw new InvalidOperationException(text+" "+BunnyGore.LastError);}
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_initialize")] static extern int Initialize([MarshalAs(UnmanagedType.LPUTF8Str)] string folder);
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_ready")] static extern void Ready();
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_boundary")] static extern void Boundary();
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_pump")] static extern void Pump();
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_spawns")] static extern int Spawns();
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_last_type")] static extern int LastType();
    [DllImport("bunny",EntryPoint="bunny_gore_fixture_last_x")] static extern float LastX();

    static void Main(string[] args)
    {
        Check(Initialize(args[0])==3,"Actual ExampleMod Gore PNGs were not registered for two independent UIDs");
        Check(new BunnyGoreSettings().DisappearSpeed==1 && new BunnyGoreSettings().DisappearSpeedAlpha==1,
            "Explicit parameterless settings must retain original native defaults");
        int first=BunnyGore.GetType("independent-a","ExampleDroplet");
        int second=BunnyGore.GetType("independent-b","ExampleDroplet");
        Check(first>0 && second>first && BunnyGore.GetTypeAny("ExampleDroplet")==first,"Per-mod lookup or first-owned fallback failed");
        Check(!BunnyGore.TrySpawn("independent-a","ExampleDroplet",10,20,0,0) && BunnyGore.LastError.Contains("Reason=WorldNotReady"),
            "Inactive-world rejection did not explain the failure");
        Ready();
        Check(BunnyGore.TryConfigure("independent-a","ExampleDroplet",new BunnyGoreSettings(disappearSpeed:300,disappearSpeedAlpha:0.5f)),
            "Representable Int32/Single rates were artificially limited to bytes");
        Check(!BunnyGore.TryConfigure("independent-a","ExampleDroplet",new BunnyGoreSettings(disappearSpeed:0.5f)) &&
            BunnyGore.LastError.Contains("Reason=NativeRepresentationMismatch"),"Int32 rate silently truncated a fraction");
        Check(!BunnyGore.TrySpawn("missing","ExampleDroplet",10,20,0,0) && BunnyGore.LastError.Contains("Mod=missing"),"Missing dependency lacks ownership context");

        string assembly=Assembly.GetExecutingAssembly().Location;
        int a=BunnyEntitySourceDispatcher.Register("independent-a",assembly);
        int b=BunnyEntitySourceDispatcher.Register("independent-b",assembly);
        Check(a>=0 && b>=0,"Example modules did not register");
        Check(BunnyEntitySourceDispatcher.Bind(a,0,200,"ExampleGoreSlime") &&
              BunnyEntitySourceDispatcher.Bind(b,0,201,"ExampleGoreSlime"),"Example NPC binding failed");
        BunnyEntitySourceDispatcher.TickNpc(a,200,0,1,45,45,200,300,0,0,200,300,12345);
        Check(BunnyEntitySourceDispatcher.KillNpc(a,200,0,1),"Actual ExampleGoreSlime.OnKill failed");
        Check(Spawns()==0,"Mono callback invoked Terraria immediately instead of queueing");
        Pump();Check(Spawns()==2 && LastX()==208 && LastType()==BunnyGore.GetType("independent-a","ExampleBoss_GoreBack"),
            "NPC kill lost its native snapshot, defaults or ownership");
        BunnyEntitySourceDispatcher.TickNpc(b,201,0,1,45,45,300,300,0,0,300,300,56789);
        Check(BunnyEntitySourceDispatcher.KillNpc(b,201,0,1),"Second mod's actual example failed");
        Pump();Check(Spawns()==4 && LastX()==308 && LastType()==BunnyGore.GetType("independent-b","ExampleBoss_GoreBack"),
            "Second mod used another package's effect or position");
        BunnyEntitySourceDispatcher.TickNpc(a,200,1,1,45,45,400,300,0,0,400,300,67890);
        Check(BunnyEntitySourceDispatcher.HitNpc(a,200,1,1,50,true),"Lethal-hit example callback failed");
        Pump();Check(Spawns()==6 && LastX()==408,"Lethal hit discarded the final cached NPC position");
        Check(BunnyGore.TrySpawn("independent-a","ExampleDroplet",1,2,0,0),"Valid effect was not queued");
        Boundary();Ready();Pump();Check(Spawns()==6,"A stale world queue spawned in the next world");
        bool invalid=false;try{BunnyGore.GetType("independent-a\0other","ExampleDroplet");}catch(ArgumentException){invalid=true;}
        Check(invalid,"Embedded NUL changed native ownership identity");
        Console.WriteLine("ACTUAL_EXAMPLE_GORE_CALLBACKS_CSHARP_PINVOKE_PRODUCTION_SERVICE_MULTIMOD_UID_AND_WORLD_BOUNDARY=PASS");
    }
}
