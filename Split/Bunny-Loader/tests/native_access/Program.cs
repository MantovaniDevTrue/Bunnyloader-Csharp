using System.Runtime.InteropServices;
using BunnyLoader.Source;

static class Program
{
    [DllImport("bunny")]static extern int test_slot();
    [DllImport("bunny")]static extern int test_calls();
    [DllImport("bunny")]static extern int test_closes();
    [DllImport("bunny")]static extern void test_batch_fail(int value);
    static void Check(bool value) { if(!value)throw new Exception("Native access assertion"); }
    static void Throws<T>(Action action) where T:Exception { try { action(); }catch(T){return;}throw new Exception("Expected "+typeof(T).Name); }
    static void Main()
    {
        Check(Marshal.SizeOf<BunnyValue>()==24&&Marshal.SizeOf<BunnyVector2>()==8&&Marshal.SizeOf<BunnyRectangle>()==16&&Marshal.SizeOf<BunnyColor>()==4);
        Check(BunnyValue.FromInt32(-123).AsInt32()==-123);
        Check(BunnyValue.FromInt64(long.MinValue).AsInt64()==long.MinValue);
        Check(BunnyValue.FromUInt32(uint.MaxValue).AsUInt32()==uint.MaxValue);
        Check(BunnyValue.FromUInt64(ulong.MaxValue).AsUInt64()==ulong.MaxValue);
        Check(BunnyValue.FromSingle(-1.25f).AsSingle()==-1.25f);
        Check(BunnyValue.FromDouble(double.Epsilon).AsDouble()==double.Epsilon);
        Check(BunnyValue.FromBoolean(true).AsBoolean());
        var color=BunnyValue.FromColor(new BunnyColor(1,2,3,4)).AsColor();Check(color.R==1&&color.A==4);
        var vector=BunnyValue.FromVector2(new BunnyVector2(-4.5f,9.25f)).AsVector2();Check(vector.X==-4.5f&&vector.Y==9.25f);
        var rectangle=BunnyValue.FromRectangle(new BunnyRectangle(-5,8,40,60)).AsRectangle();Check(rectangle.X==-5&&rectangle.Width==40&&rectangle.Contains(0,9));
        Check(new BunnyRectangle(int.MaxValue-2,0,10,10).Contains(int.MaxValue,1));
        Throws<InvalidOperationException>(()=>BunnyValue.FromInt32(1).AsSingle());
        using var hooks=new BunnyHookScope();
        BunnyNativeScope scope=hooks.Native;
        Check(ReferenceEquals(scope,hooks.Native));
        BunnyNativeType type=scope.Type("Terraria.Player");
        var life=type.Field("statLife",BunnyValueKind.Int32,writable:true);
        var position=type.Field("position",BunnyValueKind.Vector2);
        var day=type.Field("dayTime",BunnyValueKind.Boolean,isStatic:true);
        var property=type.Property("Life",BunnyValueKind.Int32);
        var method=type.Method("Probe",BunnyValueKind.Int32,false,"System.Int32");
        BunnyNativeBatch batch=scope.Batch(life,position,day);
        BunnyValue[] destination=new BunnyValue[3];
        Throws<BunnyAccessException>(()=>type.Field("Missing",BunnyValueKind.Int32));
        Throws<ArgumentException>(()=>scope.Type("System.Console"));
        Throws<ArgumentException>(()=>scope.Batch(new BunnyNativeMember[17]));
        Throws<BunnyAccessException>(()=>life.Read());
        BunnyNativeObject escaped=default;
        int dispatched=0;
        var hook=hooks.HookVoid("Terraria.Player","ResetEffects",(original,self)=>
        {
            original.Invoke();
            var target=BunnyNativeObject.FromHook(self);escaped=target;
            Check(target.IsCurrent);
            Check(life.Read(target).AsInt32()==100);
            life.Write(BunnyValue.FromInt32(125),target);
            Check(property.Read(target).AsInt32()==125);
            Check(method.Invoke(target,BunnyValue.FromInt32(9)).AsInt32()==10);
            Throws<BunnyAccessException>(()=>position.Write(BunnyValue.FromVector2(default),target));
            int before=test_calls();
            var combined=batch.ReadAndInvoke(method,destination,target,BunnyValue.FromInt32(9));
            Check(test_calls()==before+1&&combined.AsInt32()==10);
            Check(destination[0].AsInt32()==125&&destination[1].AsVector2().Y==8&&destination[2].AsBoolean());
            destination[0]=BunnyValue.FromInt32(987);test_batch_fail(1);
            Throws<BunnyAccessException>(()=>batch.ReadAndInvoke(method,destination,target,BunnyValue.FromInt32(9)));
            Check(destination[0].AsInt32()==987);test_batch_fail(0);
            life.Write(BunnyValue.FromInt32(100),target);
            dispatched++;
        });
        int slot=test_slot();Check(BunnyHooks.Dispatch(slot,1,123)==1&&dispatched==1);
        Check(!escaped.IsCurrent);Throws<BunnyAccessException>(()=>life.Read(escaped));
        hook.Dispose();
        hooks.HookVoid("Terraria.Player","ResetEffects",(_,self)=> {
            var target=BunnyNativeObject.FromHook(self);
            Check(batch.ReadAndInvoke(method,destination,target,BunnyValue.FromInt32(9)).AsInt32()==10);
        });
        slot=test_slot();
        for(int i=2;i<12;++i)Check(BunnyHooks.Dispatch(slot,i,123)==1);
        long allocatedCalls=GC.GetAllocatedBytesForCurrentThread();
        for(int i=12;i<10012;++i)Check(BunnyHooks.Dispatch(slot,i,123)==1);
        Check(GC.GetAllocatedBytesForCurrentThread()==allocatedCalls);
        long allocations=GC.GetAllocatedBytesForCurrentThread();
        for(int i=0;i<10000;++i) { var v=BunnyValue.FromVector2(vector).AsVector2();Check(v.X==vector.X); }
        Check(GC.GetAllocatedBytesForCurrentThread()==allocations);
        var typed=hooks.HookInt32("Terraria.Player","Probe",(original,self,value)=> {
            var target=BunnyNativeObject.FromHook(self);escaped=target;
            Check(target.IsCurrent);
            int combined=batch.ReadAndInvoke(method,destination,target,BunnyValue.FromInt32(value)).AsInt32();
            Check(destination[0].AsInt32()==100);
            return original.Invoke()+combined;
        });
        BunnyHooks.InvokeInt32(test_slot(),10013,123,9,out int ok,out int returned);
        Check(ok==1&&returned==52&&!escaped.IsCurrent);
        Throws<BunnyAccessException>(()=>life.Read(escaped));
        int closes=test_closes();hooks.Dispose();Check(scope.IsDisposed&&test_closes()==closes+1);
        Throws<ObjectDisposedException>(()=>life.Read(escaped));
        Check(BunnyNativeScope.GetStatus().Contains("lotes="));
        Console.WriteLine("C# native access: typed values, scoped access, fused batch+method, atomic publish and expired self PASS");
    }
}
