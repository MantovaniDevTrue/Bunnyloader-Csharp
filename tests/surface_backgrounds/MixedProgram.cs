using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

static class Program
{
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]private delegate int Callback(int slot,long ticket);
 private static readonly Callback Handler=(slot,ticket)=>BunnyHooks.Dispatch(slot,ticket,0);
 [DllImport("bunny",EntryPoint="bunny_test_surface_initialize")]private static extern int Initialize();
 [DllImport("bunny",EntryPoint="bunny_test_surface_callback")]private static extern void CallbackWorker(Callback callback);
 [DllImport("bunny",EntryPoint="bunny_test_surface_pick")]private static extern int Pick(int vanilla);
 [DllImport("bunny",EntryPoint="bunny_test_surface_scene")]private static extern void Scene(int slot,int menu);
 [DllImport("bunny",EntryPoint="bunny_test_surface_layer")]private static extern int Layer(int layer);
 [DllImport("bunny",EntryPoint="bunny_test_surface_js_start")]private static extern int StartJs();
 [DllImport("bunny",EntryPoint="bunny_test_surface_js_priority")]private static extern void JsPriority(int priority);
 [DllImport("bunny",EntryPoint="bunny_test_surface_js_second")]private static extern int SecondJs();
 [DllImport("bunny",EntryPoint="bunny_test_surface_js_stop")]private static extern void StopJs();
 [DllImport("bunny",EntryPoint="bunny_test_surface_cleanup")]private static extern void Cleanup();
 private static void Check(bool value,string message){if(!value)throw new Exception(message);}
 private static void SetId(BunnyHookModule style,string uid)=>typeof(BunnyHookModule).GetProperty(nameof(BunnyHookModule.ModId))!.GetSetMethod(true)!.Invoke(style,new object[]{uid});
 public static void Main()
 {
  Check(Initialize()==1,"six native hooks");CallbackWorker(Handler);
  try {
   using var scope=new BunnyHookScope();var cs=new ExampleSurfaceBackgroundStyle();SetId(cs,"csharp-surface-uid");cs.Load(scope);
   int js=StartJs();Check(js==cs.Slot+1,"C# then QuickJS register in one native registry");
   Check(Pick(0)==js,"real JS scene provider chooses its style");Scene(js,0);Check(Layer(0)==10,"original JS callback to native draw");
   BunnySurfaceBackgrounds.Activate<ExampleSurfaceBackgroundStyle>(cs.ModId,BunnyBackgroundPriority.BossHigh);
   Check(Pick(4)==cs.Slot,"managed style wins independently of install order");Scene(cs.Slot,0);Check(Layer(0)==10 && Layer(2)==15,"C# callback on worker with JS active");
   using var secondScope=new BunnyHookScope();var second=new ExampleSurfaceBackgroundStyle();SetId(second,"second-csharp-surface-uid");second.Load(secondScope);
   Check(second.Slot==js+1 && SecondJs()==second.Slot+1,"C#/JS/C#/JS interleaving preserves lookup gaps");
   JsPriority(8);Check(Pick(4)==second.Slot+1,"JS weight wins against same managed priority");
   cs.Unload();scope.Dispose();BunnySurfaceBackgrounds.Activate<ExampleSurfaceBackgroundStyle>(second.ModId,BunnyBackgroundPriority.BossHigh,1);
   Check(Pick(4)==second.Slot,"surviving managed scope rebinds with JS provider untouched");
   Scene(second.Slot,0);Check(Layer(0)==10,"real C# after first module unload");
   StopJs();Check(Pick(4)==second.Slot && Layer(2)==15,"unloading QuickJS preserves managed native service");
   second.Unload();secondScope.Dispose();Check(Pick(0)==0,"last provider unload preserves original picker");
   using var againScope=new BunnyHookScope();var again=new ExampleSurfaceBackgroundStyle();SetId(again,"csharp-surface-uid");again.Load(againScope);
   int againJs=StartJs();again.Unload();Check(Pick(0)==againJs,"C# unload also preserves live QuickJS, opposite order");StopJs();
  }finally{Cleanup();}
  Console.WriteLine("SURFACE_REAL_CSHARP_WORKER_AND_ORIGINAL_QUICKJS_ONE_PROCESS_INTERLEAVED_IDS_PRIORITY_DRAW_AND_BOTH_UNLOAD_ORDERS=PASS");
 }
}
