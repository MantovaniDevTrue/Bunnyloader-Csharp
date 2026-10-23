using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Text;

static class Suite
{
    static void Equal<T>(T expected,T actual)
    { if(!EqualityComparer<T>.Default.Equals(expected,actual))throw new Exception($"Expected {expected}, got {actual}"); }
    static string LocaleValue(string wire) => Encoding.UTF8.GetString(Convert.FromHexString(wire.Split('\n')[2].Split('\t')[3]));
    [MethodImpl(MethodImplOptions.NoInlining)]
    static WeakReference Exercise(string file,string token,string owner,int version)
    {
        Equal("READY|1",BunnyRarityRuntime.Dispatch(owner,token,"",0,2,"SharedRarity:12"));
        Equal("v"+version,LocaleValue(BunnyLocalizationRuntime.Dispatch(owner,token,"en-US")));
        // The quest sees both previous hooks' writes only if all three share one assembly.
        Equal("1",BunnyRecipeRuntime.Dispatch(owner,token,"SharedQuestFish",0,4,"0,950"));
        Equal($"COLOR|{version}|2|0|255",BunnyRarityRuntime.Dispatch(owner,token,"SharedRarity",0,0,"255,255,255"));
        Equal("RARITY|12",BunnyRarityRuntime.Dispatch(owner,token,"SharedRarity",0,1,"1.2,11"));
        var mods=(IDictionary)typeof(BunnyRecipeLoader).GetField("mods",BindingFlags.Static|BindingFlags.NonPublic)!.GetValue(null)!;
        object mod=mods[owner]!;
        var context=(AssemblyLoadContext)mod.GetType().GetField("Context")!.GetValue(mod)!;
        Equal(true,context.IsCollectible);
        return new WeakReference(context);
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Clear(string owner,string token)
    {
        Equal("CLEARED",BunnyRecipeRuntime.Dispatch(owner,token,"__unload__",0,6,""));
        Equal("CLEARED",BunnyRarityRuntime.Dispatch(owner,token,"",0,3,""));
        Equal(true,BunnyLocalizationRuntime.Unload(owner));
    }
    static void Compile(string root,string file,int version)
    {
        string source="""
using BunnyLoader.Source;
public static class SharedState { public static int Hooks; public const int Version = VERSION; }
public sealed class SharedRarity:BunnyRarity {
 public override void SetStaticDefaults(){SharedState.Hooks++;}
 public override BunnyColor RarityColor=>new((byte)SharedState.Version,(byte)SharedState.Hooks,0);
}
public sealed class SharedLocales:BunnyLocalizationModule {
 public override void RegisterTranslations(BunnyLocalizationCatalog catalog){}
 public override void OnLocalizationsLoaded(){
  SharedState.Hooks++;
  BunnyLocalizationRuntime.Register("Mods."+BunnyLocalizationRuntime.ActiveModId+".Shared.Value","en-US","v"+SharedState.Version);
 }
}
public sealed class SharedQuestFish:BunnyItem {
 public override void SetDefaults(){
  DisplayName="Shared Quest Fish";Texture="Content/Items/Quests/ExampleQuestFish";
  DefaultToQuestFish();QuestAvailableWhen=context=>SharedState.Hooks==2&&context.FishType==950;
 }
}
""".Replace("VERSION",version.ToString(System.Globalization.CultureInfo.InvariantCulture));
        File.WriteAllText(Path.Combine(root,"content","Shared.cs"),source);
        string runtime=Path.Combine(root,"runtime");Directory.CreateDirectory(runtime);
        string framework=Path.GetDirectoryName(typeof(object).Assembly.Location)!;
        foreach(string name in new[]{"System.Private.CoreLib","System.Runtime","netstandard","System.Collections","System.Linq","System.Console","System.Runtime.Extensions","System.ObjectModel","System.Collections.Concurrent","System.Threading","System.Threading.Tasks"})
            File.Copy(Path.Combine(framework,name+".dll"),Path.Combine(runtime,name+".dll"),true);
        File.Copy(typeof(BunnyItem).Assembly.Location,Path.Combine(runtime,"BunnySourceSdk.dll"),true);
        int compiled=Compiler.Compile(root,file,runtime,file+".txt");
        if(compiled!=1)throw new Exception(File.ReadAllText(file+".txt"));
    }
    static void Collected(WeakReference context)
    {
        for(int i=0;i<20&&context.IsAlive;i++){GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();}
        Equal(false,context.IsAlive);
    }
    public static void Main()
    {
        string root=Path.Combine(Path.GetTempPath(),"bunny-three-loaders-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root,"content"));
        string file=Path.Combine(root,"Shared.dll"),first=file+"|bunny-revision=1",second=file+"|bunny-revision=2";
        try {
            Compile(root,file,1);
            var old=Exercise(file,first,"one",1);
            var other=Exercise(file,first,"two",1);
            Clear("one",first);Collected(old);
            Compile(root,file,2); // Same physical path and class names; new image and revision.
            var fresh=Exercise(file,second,"one",2);
            Equal("COLOR|1|2|0|255",BunnyRarityRuntime.Dispatch("two",first,"SharedRarity",0,0,"255,255,255"));
            Equal("1",BunnyRecipeRuntime.Dispatch("two",first,"SharedQuestFish",0,4,"0,950"));
            Equal("ERROR|Stage=ResolveRarity Reason=OwnerNotBound",BunnyRarityRuntime.Dispatch("one",first,"SharedRarity",0,0,"255,255,255"));
            Clear("one",second);Clear("two",first);Collected(fresh);Collected(other);
            Console.WriteLine("THREE_LOADERS_REAL_ROSLYN_SHARED_STATE_SAME_PATH_RELOAD_OWNER_ISOLATION_COLLECTIBLE_PASS");
        } finally {Directory.Delete(root,true);}
    }
}
