using System.Runtime.InteropServices;
using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

sealed class ExampleRejectedUndergroundStyle:BunnyUndergroundBackgroundStyle
{
 public int Calls;
 public override void FillTextureArray(Span<int> textureSlots)
 {Calls++;throw new InvalidOperationException("example-managed-fill-failure");}
}

static class Program
{
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]private delegate int Callback(int slot,long ticket);
 private static readonly Callback Handler=(slot,ticket)=>BunnyHooks.Dispatch(slot,ticket,0);
 [DllImport("bunny",EntryPoint="bunny_test_underground_initialize")]private static extern int Initialize();
 [DllImport("bunny",EntryPoint="bunny_test_underground_callback")]private static extern void SetCallback(Callback callback);
 [DllImport("bunny",EntryPoint="bunny_test_underground_draw")]private static extern int Draw(int vanilla);
 [DllImport("bunny",EntryPoint="bunny_test_underground_patch")]private static extern int Patch(int slot,int previous);
 [DllImport("bunny",EntryPoint="bunny_test_underground_reset")]private static extern void Reset();
 private static void Check(bool value,string message){if(!value)throw new Exception(message);}
 public static void Main()
 {
  Check(Initialize()==1,"native contracts and three original hooks");SetCallback(Handler);
  var setId=typeof(BunnyHookModule).GetProperty(nameof(BunnyHookModule.ModId))!.GetSetMethod(true)!;
  var first=new ExampleUndergroundBackgroundStyle();var second=new ExampleUndergroundBackgroundStyle();
  setId.Invoke(first,new object[]{"first-uid"});setId.Invoke(second,new object[]{"second-uid"});
  using var firstScope=new BunnyHookScope();using var secondScope=new BunnyHookScope();
  first.Load(firstScope);second.Load(secondScope);
  Check(first.Slot>=22 && second.Slot>first.Slot,"separate stable logical IDs for same class in two mods");
  Check(BunnyUndergroundBackgrounds.GetSlot<ExampleUndergroundBackgroundStyle>("first-uid")==first.Slot,"lookup actual UID and full class");
  Check(ReferenceEquals(BunnyUndergroundBackgrounds.Get(first.Slot),first),"get registered style instance");
  BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>("first-uid",BunnyBackgroundPriority.BiomeLow);
  Check(Draw(3)==first.Slot && Draw(1)==1,"low priority only replaces ordinary cave");
  BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>("second-uid",BunnyBackgroundPriority.BiomeHigh);
  Check(Draw(1)==second.Slot && Patch(second.Slot,0)==5 && Patch(first.Slot,1)==5,"actual example Fill to native current/previous layer buffers");
  var rejected=new ExampleRejectedUndergroundStyle();setId.Invoke(rejected,new object[]{"rejected-uid"});
  using var rejectedScope=new BunnyHookScope();rejected.Load(rejectedScope);
  BunnyUndergroundBackgrounds.Activate<ExampleRejectedUndergroundStyle>("rejected-uid",BunnyBackgroundPriority.BossHigh);
  var stderr=Console.Error;using var errors=new StringWriter();
  try {
   Console.SetError(errors);
   Check(Draw(1)==rejected.Slot && rejected.Calls==1,"exception does not report a successful fill");
   var report=errors.ToString();
   Check(report.Contains("Mod=rejected-uid") && report.Contains("Class=ExampleRejectedUndergroundStyle") &&
    report.Contains("Method=FillTextureArray") && report.Contains("example-managed-fill-failure") &&
    report.Contains("Result=CallbackRejected"),"full original managed exception context");
  }finally{Console.SetError(stderr);}
  rejected.Unload();
  Check(Draw(1)==second.Slot,"failed style unload leaves independent working style active");
  first.Unload();firstScope.Dispose();
  Check(Draw(3)==second.Slot && Patch(second.Slot,0)==5,"first owner unload rebinds dispatcher to second scope");
  Reset();Check(Draw(3)==3,"world reset deactivates previous world selection");
  BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>("second-uid",BunnyBackgroundPriority.BiomeHigh);
  Check(Draw(9)==second.Slot,"explicit activation after world boundary");
  second.Unload();Check(Draw(3)==3,"last owner unload preserves vanilla picker");
  try {BunnyUndergroundBackgrounds.GetSlot<ExampleUndergroundBackgroundStyle>("missing-uid");throw new Exception("missing dependency accepted");}
  catch(InvalidOperationException error){Check(error.Message.Contains("StyleNotRegistered") && error.Message.Contains("missing-uid"),"full missing style diagnostic");}
  Console.WriteLine("EXAMPLE_UNDERGROUND_REAL_MANAGED_DISPATCH_PINVOKE_NATIVE_PICKER_FIVE_LAYERS_TWO_UIDS_AND_UNLOAD=PASS");
 }
}
