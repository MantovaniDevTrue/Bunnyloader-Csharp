using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using BiomeConversionExample;

int checks=0;
void Check(bool condition,string label){if(!condition)throw new Exception(label+": "+BunnyContentHostDispatcher.LastError);++checks;}
string temp=Path.Combine(Path.GetTempPath(),"bunny-biome-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temp);
try {
    var original=new OriginalConversion();original.RegisterForTest();original.SetupContent();
    Terraria.ModLoader.BiomeConversionLoader.PostSetupContent();
    Check(original.Type==22&&original.Defaults==1&&original.PostSetup==1,"pinned upstream register/setup/postsetup executed");
    Check(Terraria.ModLoader.BiomeConversionLoader.GetBiomeConversion(21)==null&&
        ReferenceEquals(original,Terraria.ModLoader.BiomeConversionLoader.GetBiomeConversion(22)),"pinned upstream lookup semantics");
    // The actual Roslyn compiler must accept a conversion-only mod. No entity is invented.
    string runtime=Path.Combine(temp,"runtime");Directory.CreateDirectory(runtime);
    string framework=Path.GetDirectoryName(typeof(object).Assembly.Location)!;
    foreach(string name in new[]{"System.Private.CoreLib","System.Runtime","netstandard","System.Collections","System.Linq","System.Console","System.Runtime.Extensions","System.ObjectModel","System.Threading","System.Runtime.InteropServices"})
        File.Copy(Path.Combine(framework,name+".dll"),Path.Combine(runtime,name+".dll"));
    File.Copy(typeof(ModBiomeConversion).Assembly.Location,Path.Combine(runtime,"BunnySourceSdk.dll"));
    string root=Path.Combine(temp,"mod");Directory.CreateDirectory(Path.Combine(root,"content"));
    File.WriteAllText(Path.Combine(root,"content","main.cs"),"""
        using BunnyLoader.Source;
        public sealed class ConversionOnly : ModBiomeConversion {
            public override void PostSetupContent() { RegisterTileConversion(1,2); }
        }
        """);
    string dll=Path.Combine(temp,"ConversionOnly.dll"),diag=Path.Combine(temp,"diagnostics.txt");
    Check(Compiler.Compile(root,dll,runtime,diag)>0,"Roslyn compiles conversion-only mod");
    Check(CompiledAssemblyProbe.Verify(dll,diag)==1&&CompiledAssemblyProbe.ManagedContentTypeCount()==1,"verifier separates service from native entities");
    Check(BiomeConversionLoader.BiomeConversionCount==Terraria.ModLoader.BiomeConversionLoader.BiomeConversionCount-1,"vanilla count matches pinned upstream");
    Check(BiomeConversionLoader.GetBiomeConversion(-1)==null&&BiomeConversionLoader.GetBiomeConversion(21)==null,"vanilla and invalid lookups");
    Check(BunnyContentHostDispatcher.Dispatch(0,"conversion-only",dll)==1,"automatic conversion-only bootstrap and native publish");
    var conversion=BiomeConversionLoader.GetBiomeConversion(22)!;
    Native.Begin();
    Check(conversion.Convert(8,8,0),"real PInvoke queues native conversion");
    Check(Native.Pump()==1&&Native.Tile()==2,"production C++ engine applies managed rule");
    Check(conversion.Convert(8,8,0),"queue before unload");
    Check(BunnyContentHostDispatcher.Dispatch(1,"conversion-only","")==0,"scope unload revokes native owner");
    Check(Native.Pump()==0&&!conversion.Convert(8,8,0)&&BiomeConversionLoader.GetBiomeConversion(22)==null,"unload cancels pending work and tombstones ID");
    bool rejected=false;try{conversion.RegisterTileConversion(0,1);}catch(InvalidOperationException){rejected=true;}
    Check(rejected,"rules frozen outside PostSetupContent");
    string example=typeof(ExampleStoneConversion).Assembly.Location;
    Check(CompiledAssemblyProbe.Verify(example,diag)==3&&CompiledAssemblyProbe.ManagedContentTypeCount()==1,"mixed example verifies one conversion and two legacy entities");
    Check(BunnyContentHostDispatcher.Dispatch(0,"example",example)==1,"mixed example automatic bootstrap");
    var stone=ModContent.GetInstance<ExampleStoneConversion>()!;
    Check(stone!=null&&ReferenceEquals(stone,BiomeConversionLoader.GetBiomeConversion(stone.Type)),"singleton and conversion identity agree");
    Native.Begin();
    var projectile=new ConversionBolt();var state=new BunnyProjectileAiState{Age=12,X=128,Y=128};
    Check(!projectile.AI(ref state),"real example projectile preserves vanilla AI");
    Check(Native.Pump()>0&&Native.Wall()==1,"example projectile reaches native wall conversion");
    Native.RejectPublish();
    Check(BunnyContentHostDispatcher.Dispatch(0,"native-rejected",dll)==-1&&!BunnyContentLoader.IsLoaded("native-rejected"),"native publication failure rolls back managed scope");
    Check(Native.Count()==1&&BunnyContentLoader.IsLoaded("example"),"failed owner preserves another loaded conversion");
    File.WriteAllText(Path.Combine(root,"content","main.cs"),"""
        using BunnyLoader.Source;
        public sealed class BadRules : ModBiomeConversion {
            public override void PostSetupContent() { RegisterTileConversion(1,2); RegisterTileConversion(1,3); }
        }
        """);
    string bad=Path.Combine(temp,"BadRules.dll");
    Check(Compiler.Compile(root,bad,runtime,diag)>0,"Roslyn compiles post-setup failure fixture");
    Check(BunnyContentHostDispatcher.Dispatch(0,"bad-rules",bad)==-1&&!BunnyContentLoader.IsLoaded("bad-rules"),"PostSetupContent failure rolls back registration");
    Check(Native.Count()==1&&ReferenceEquals(stone,ModContent.GetInstance<ExampleStoneConversion>()),"failure preserves singleton of unrelated owner");
    if(args.Length>0) {
        string compiled=Path.Combine(temp,"Playable.dll");
        Check(Compiler.Compile(args[0],compiled,runtime,diag)>0,"production Roslyn compiles exact playable sources");
        Check(CompiledAssemblyProbe.Verify(compiled,diag)==3&&CompiledAssemblyProbe.ManagedContentTypeCount()==1,"playable wire includes two native entities and one conversion");
    }
    Check(!stone!.Convert(-1,0)&&!stone.Convert(0,0,9),"invalid brush refused");
    Check(BunnyContentHostDispatcher.Dispatch(2,"","")==0,"global unload");
    Check(Native.Count()==0,"global shutdown revokes every native conversion owner");
    Terraria.ModLoader.BiomeConversionLoader.Unload();
    Check(BiomeConversionLoader.BiomeConversionCount==Terraria.ModLoader.BiomeConversionLoader.BiomeConversionCount,"global reset matches pinned upstream");
    Console.WriteLine("BIOME_CONVERSION_MANAGED_ROSLYN_NATIVE_PINVOKE_PASS checks="+checks);
} finally { BunnyContentHostDispatcher.Dispatch(2,"","");Directory.Delete(temp,true); }
static class Native {
    [DllImport("bunny",EntryPoint="test_reject_publish")] internal static extern void RejectPublish();
    [DllImport("bunny",EntryPoint="test_count")] internal static extern int Count();
    [DllImport("bunny",EntryPoint="test_world_begin")] internal static extern void Begin();
    [DllImport("bunny",EntryPoint="test_pump")] internal static extern int Pump();
    [DllImport("bunny",EntryPoint="test_tile")] internal static extern int Tile();
    [DllImport("bunny",EntryPoint="test_wall")] internal static extern int Wall();
}
