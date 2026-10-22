using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

sealed class ExampleCustomSurface:BunnySurfaceBackgroundStyle
{
 public override bool PreDrawCloseBackground(in BunnySurfaceDrawContext context)
 {
  if(context.ScreenWidth!=2400 || context.VerticalAdjustment!= -11.375 || context.Alpha!=.5)
   throw new Exception("Native snapshot changed scalar width or lost fractional geometry");
  if(context.BaseColor.PackedRgba!=0x500a1428 || context.FadedColor.PackedRgba!=0x28050a14)
   throw new Exception("Native BGRA storage was not normalized to public RGBA, including fade alpha");
  context.Draw(10,new(31,41),new(1,2,3,4),new BunnyRectangle(2,3,64,32),.25f,new(3,4),1,3,.5f);
  return false;
 }
 public override int ChooseCloseTexture(ref double scale,ref double parallax,ref double a,ref double b)
  =>throw new Exception("ChooseClose must not run after PreDrawClose returned false");
}
sealed class ExampleRejectedSurface:BunnySurfaceBackgroundStyle
{
 public override int ChooseFarTexture()=>throw new InvalidOperationException("example-original-surface-exception");
}
static class Program
{
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]private delegate int Callback(int slot,long ticket);
 private static readonly Callback Handler=(slot,ticket)=>BunnyHooks.Dispatch(slot,ticket,0);
 [DllImport("bunny",EntryPoint="bunny_test_surface_initialize")]private static extern int Initialize();
 [DllImport("bunny",EntryPoint="bunny_test_surface_callback")]private static extern void CallbackWorker(Callback callback);
 [DllImport("bunny",EntryPoint="bunny_test_surface_pick")]private static extern int Pick(int vanilla);
 [DllImport("bunny",EntryPoint="bunny_test_surface_scene")]private static extern void Scene(int slot,int menu);
 [DllImport("bunny",EntryPoint="bunny_test_surface_fades")]private static extern float Fades(int slot,float speed);
 [DllImport("bunny",EntryPoint="bunny_test_surface_layer")]private static extern int Layer(int layer);
 [DllImport("bunny",EntryPoint="bunny_test_surface_draw_count")]private static extern int DrawCount();
 [DllImport("bunny",EntryPoint="bunny_test_surface_draw_value")]private static extern double Value(int index);
 [DllImport("bunny",EntryPoint="bunny_test_surface_reset")]private static extern void Reset();
 [DllImport("bunny",EntryPoint="bunny_test_surface_calls")]private static extern int Calls();
 [DllImport("bunny",EntryPoint="bunny_test_surface_deny_installs")]private static extern void DenyInstalls(int deny);
 [DllImport("bunny",EntryPoint="bunny_test_surface_has_log")]private static extern int HasLog([MarshalAs(UnmanagedType.LPUTF8Str)]string text);
 [DllImport("bunny",EntryPoint="bunny_test_surface_cleanup")]private static extern void Cleanup();
 private static void Check(bool value,string message){if(!value)throw new Exception(message);}
 private static void SetId(BunnyHookModule style,string uid)=>typeof(BunnyHookModule).GetProperty(nameof(BunnyHookModule.ModId))!.GetSetMethod(true)!.Invoke(style,new object[]{uid});
 public static void Main()
 {
  Check(Initialize()==1,"actual six native hooks");CallbackWorker(Handler);
  try {
   using var firstScope=new BunnyHookScope();using var secondScope=new BunnyHookScope();
   var first=new ExampleSurfaceBackgroundStyle();var second=new ExampleSurfaceBackgroundStyle();
   SetId(first,"first-surface-uid");SetId(second,"second-surface-uid");first.Load(firstScope);second.Load(secondScope);
   Check(BunnySurfaceBackgrounds.VanillaCount==19 && first.Slot==19 && second.Slot==20,"real native count, two UIDs, no fixed slot assumption");
   Check(BunnySurfaceBackgrounds.GetSlot<ExampleSurfaceBackgroundStyle>("first-surface-uid")==first.Slot && ReferenceEquals(BunnySurfaceBackgrounds.Get(first.Slot),first),"public lookup and original instance");
   BunnySurfaceBackgrounds.Activate<ExampleSurfaceBackgroundStyle>(first.ModId);
   Check(Pick(0)==first.Slot && Pick(3)==3,"native low priority arbitration");
   Scene(first.Slot,0);Check(Fades(first.Slot,0)==.4f,"original front first, full native fade array to real C# span");
   Scene(first.Slot,0);Check(Layer(0)==10 && DrawCount()==2,"actual Example far texture on native SpriteBatch");
   for(int i=1;i<=53;i++)Check(Layer(1)==11+(i/13)%4,"original 13-call animated middle frame "+i);
   Check(Layer(2)==15 && DrawCount()>0,"actual Example close default parallax and projection");
   BunnySurfaceBackgrounds.Activate<ExampleSurfaceBackgroundStyle>(second.ModId,BunnyBackgroundPriority.BossHigh);
   Check(Pick(4)==second.Slot,"second mod independently activates");
   DenyInstalls(1);first.Unload();firstScope.Dispose();Scene(second.Slot,0);Check(Layer(0)==10,"owner scope transfer works while native hook installation is forbidden during teardown");DenyInstalls(0);
   var custom=new ExampleCustomSurface();SetId(custom,"custom-surface-uid");using var customScope=new BunnyHookScope();custom.Load(customScope);
   Scene(custom.Slot,0);Check(Layer(2)==10 && Value(0)==31 && Value(1)==41 && Value(2)==.25 && Value(4)==3 && Value(5)==.5,"custom queued source rectangle, rotation, origin, effects and depth");
   custom.Unload();
   var rejected=new ExampleRejectedSurface();SetId(rejected,"rejected-surface-uid");using var rejectedScope=new BunnyHookScope();rejected.Load(rejectedScope);
   var stderr=Console.Error;using var errors=new StringWriter();
   try {Console.SetError(errors);Scene(rejected.Slot,0);Layer(0);}
   finally{Console.SetError(stderr);}
   Check(errors.ToString().Contains("Mod=rejected-surface-uid") && errors.ToString().Contains("Class=ExampleRejectedSurface") &&
    errors.ToString().Contains("Method=ChooseFarTexture") && errors.ToString().Contains("example-original-surface-exception"),"original managed exception with mod/class/hook context");
   Check(HasLog("example-original-surface-exception")==1 && HasLog("Method=ChooseFarTexture")==1,"native Bunny report preserves the managed exception, not only Console.Error or CallbackReplyMissing");
   rejected.Unload();Scene(second.Slot,0);Check(Layer(0)==10,"one failure leaves other real style functional");
   Reset();Check(Pick(0)==0,"scene deactivates at world boundary");
   BunnySurfaceBackgrounds.Set(second.ModId,second.GetType().FullName!,BunnyBackgroundPriority.BossHigh,.5,true,true);
   Scene(second.Slot,1);Check(Pick(7)==7 && Layer(0)==10,"menu activation is independent and preserves original preferred style");
   Reset();Check(Layer(0)==10,"world reset preserves explicit menu theme");
   second.Unload();Check(Calls()>60,"callbacks ran on the separate managed worker through the production mailbox");
   try {BunnySurfaceBackgrounds.GetSlot<ExampleSurfaceBackgroundStyle>("missing-surface-uid");throw new Exception("missing dependency accepted");}
   catch(InvalidOperationException error){Check(error.Message.Contains("StyleNotRegistered") && error.Message.Contains("missing-surface-uid"),"native dependency diagnostic");}
  }finally{Cleanup();}
  Console.WriteLine("SURFACE_REAL_CSHARP_WORKER_PINVOKE_MAILBOX_SIX_HOOKS_ORIGINAL_EXAMPLE_53_ANIMATION_CALLS_CUSTOM_DRAW_MENU_WORLD_AND_UNLOAD=PASS");
 }
}
