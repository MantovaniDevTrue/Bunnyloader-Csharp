using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

static class Program
{
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]private delegate int Callback(int slot,long ticket);
 private static readonly Callback Handler=(slot,ticket)=>BunnyHooks.Dispatch(slot,ticket,0);
 [DllImport("bunny",EntryPoint="bunny_test_underground_initialize")]private static extern int Initialize();
 [DllImport("bunny",EntryPoint="bunny_test_underground_callback")]private static extern void SetCallback(Callback callback);
 [DllImport("bunny",EntryPoint="bunny_test_underground_draw")]private static extern int Draw(int vanilla);
 [DllImport("bunny",EntryPoint="bunny_test_underground_patch")]private static extern int Patch(int slot,int previous);
 [DllImport("bunny",EntryPoint="bunny_test_underground_js_start")]private static extern int StartJs();
 [DllImport("bunny",EntryPoint="bunny_test_underground_js_priority")]private static extern void JsPriority(int priority);
 [DllImport("bunny",EntryPoint="bunny_test_underground_js_stop")]private static extern void StopJs();
 private static void Check(bool value,string message){if(!value)throw new Exception(message);}
 public static void Main()
 {
  Check(Initialize()==1,"native IL2CPP contracts");SetCallback(Handler);
  var setId=typeof(BunnyHookModule).GetProperty(nameof(BunnyHookModule.ModId))!.GetSetMethod(true)!;
  var cs=new ExampleUndergroundBackgroundStyle();setId.Invoke(cs,new object[]{"csharp-mixed-uid"});
  using var scope=new BunnyHookScope();cs.Load(scope);
  int js=StartJs();Check(js>cs.Slot,"actual QuickJS registration after actual C# module, shared IDs");
  Check(Draw(3)==js && Patch(js,0)==5,"original JS example fills native arrays in same process as managed C#");
  BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>("csharp-mixed-uid",BunnyBackgroundPriority.BossHigh);
  Check(Draw(3)==cs.Slot && Patch(js,1)==5,"C# wins while JS remains previous style");
  JsPriority(8);Check(Draw(3)==js && Patch(cs.Slot,1)==5,"JS weight wins while C# remains previous style");
  cs.Unload();Check(Draw(3)==js,"unloading last C# scope preserves actual QuickJS provider");scope.Dispose();
  using var reloadedScope=new BunnyHookScope();var reloaded=new ExampleUndergroundBackgroundStyle();
  setId.Invoke(reloaded,new object[]{"csharp-mixed-uid"});reloaded.Load(reloadedScope);
  BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>("csharp-mixed-uid",BunnyBackgroundPriority.BossHigh);
  StopJs();Check(Draw(3)==reloaded.Slot && Patch(reloaded.Slot,0)==5,"JS unload preserves reloaded real C# dispatcher");
  reloaded.Unload();Check(Draw(3)==3,"both providers unloaded preserve original picker");
  Console.WriteLine("REAL_CSHARP_AND_ORIGINAL_JS_QUICKJS_IN_ONE_PROCESS_NATIVE_BACKGROUNDS_PRIORITY_TRANSITION_AND_BOTH_UNLOAD_ORDERS=PASS");
 }
}
