using System.Runtime.InteropServices;
using BunnyLoader.Source;

static class Program
{
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]private delegate int Callback(int slot,long ticket);
 private static readonly Callback Handler=(slot,ticket)=>BunnyHooks.Dispatch(slot,ticket,0);
 [DllImport("bunny",EntryPoint="bunny_test_surface_initialize")]private static extern int Initialize();
 [DllImport("bunny",EntryPoint="bunny_test_surface_callback")]private static extern void CallbackWorker(Callback callback);
 [DllImport("bunny",EntryPoint="bunny_test_surface_scene")]private static extern void Scene(int slot,int menu);
 [DllImport("bunny",EntryPoint="bunny_test_surface_layer")]private static extern int Layer(int layer);
 [DllImport("bunny",EntryPoint="bunny_test_surface_world_stage")]private static extern int WorldStage();
 [DllImport("bunny",EntryPoint="bunny_test_surface_pause_world")]private static extern void PauseNativeWorld();
 [DllImport("bunny",EntryPoint="bunny_test_surface_cleanup")]private static extern void Cleanup();
 private static void Check(bool value,string message){if(!value)throw new Exception(message);}
 public static void Main(string[] args)
 {
  Check(Initialize()==1,"six original native hooks");CallbackWorker(Handler);
  string uid="example-session-uid",path=Path.GetFullPath(args[0]);
  try {
   Check(BunnyUniversalHookDispatcher.Register(uid,path)==2,"actual collectible mod host discovers background and gameplay modules");
   int slot=BunnySurfaceBackgrounds.GetSlot(uid,"ExampleModCSharp.Biomes.ExampleSurfaceBackgroundStyle");
   var original=BunnySurfaceBackgrounds.Get(slot);Check(original is not null && WorldStage()==1,"session style and gameplay hooks loaded");
   Scene(slot,0);for(int i=1;i<=13;i++)Check(Layer(1)==11+(i/13)%4,"actual original animation before world exit");
   PauseNativeWorld();Check(BunnyUniversalHookDispatcher.PauseWorld()==1,"only menu style survives world pause");
   Check(WorldStage()==0 && !BunnyUniversalHookDispatcher.GetStatusSnapshot(uid).Contains("ExampleWorldSessionProbe"),"world scope and modules actually unloaded");
   Check(ReferenceEquals(original,BunnySurfaceBackgrounds.Get(slot)),"menu keeps the exact style instance, not a reconstruction");
   Scene(slot,1);Check(Layer(0)==10 && Layer(1)==12 && Layer(2)==15,"all actual C# callbacks remain live in menu and frame was not reset");
   Check(BunnyUniversalHookDispatcher.PauseWorld()==1 && Layer(1)==12,"repeated pause is idempotent");
   for(int world=0;world<3;world++) {
    Check(BunnyUniversalHookDispatcher.Register(uid,path)==2,"world re-entry reconstructs gameplay modules");
    Check(WorldStage()==1 && BunnyUniversalHookDispatcher.GetStatusSnapshot(uid).Contains("ExampleWorldSessionProbe=Loads=1"),"world assembly/static state resets as before");
    Check(ReferenceEquals(original,BunnySurfaceBackgrounds.Get(slot)),"re-entry does not duplicate/reload render style");
    Scene(slot,0);Check(Layer(0)==10,"same original C# style after re-entry");
    PauseNativeWorld();Check(BunnyUniversalHookDispatcher.PauseWorld()==1,"pause after repeated worlds");
    Scene(slot,1);Check(Layer(2)==15,"menu callbacks still functional after repeated exits");
   }
   Check(BunnyUniversalHookDispatcher.Unload(uid),"explicit mod unload also closes render scope");
   Check(BunnySurfaceBackgrounds.Get(slot) is null && WorldStage()==0,"no live style or gameplay hooks after true mod unload");
  }finally{BunnyUniversalHookDispatcher.UnloadAll();Cleanup();}
  Console.WriteLine("SURFACE_ACTUAL_MOD_HOST_WORLD_PAUSE_MENU_CALLBACKS_REENTRY_FRESH_GAMEPLAY_STATICS_PERSISTENT_ORIGINAL_ANIMATION_AND_FINAL_UNLOAD=PASS");
 }
}
