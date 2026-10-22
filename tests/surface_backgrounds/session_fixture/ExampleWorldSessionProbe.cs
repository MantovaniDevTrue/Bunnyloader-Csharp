using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

namespace ExampleModCSharp.Systems;

// The real module host loads this together with the actual ExampleMod style.
// Its static counter must reset in a fresh world assembly while menu animation
// keeps the original style instance and its own assembly context alive.
public sealed class ExampleWorldSessionProbe:BunnyHookModule
{
 private static int loads;
 private int generation;
 public override void Load(BunnyHookScope hooks)
 {
  generation=++loads;
  hooks.HookSystemStage("EntitiesEnd",(original,self)=>original.Invoke());
  BunnySurfaceBackgrounds.Set(ModId,typeof(ExampleSurfaceBackgroundStyle).FullName!,BunnyBackgroundPriority.BossHigh,.5,true,true);
 }
 public override string DiagnosticStatus()=>"Loads="+generation;
}
