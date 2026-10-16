using BunnyLoader.Source;

public sealed class ExampleNativeAccess : BunnyHookModule
{
    private readonly BunnyValue[] snapshot=new BunnyValue[3];
    private BunnyNativeBatch batch=null!;
    private BunnyNativeMember buffIndex=null!;
    public static int LastLife { get; private set; }
    public static BunnyVector2 LastPosition { get; private set; }
    public static bool LastDayTime { get; private set; }
    public static int LastOnFireIndex { get; private set; }

    public override void Load(BunnyHookScope hooks)
    {
        BunnyNativeScope access=hooks.Native;
        BunnyNativeType player=access.Type("Terraria.Player");
        BunnyNativeType main=access.Type("Terraria.Main");
        var life=player.Field("statLife",BunnyValueKind.Int32);
        var position=player.Field("position",BunnyValueKind.Vector2);
        var day=main.Field("dayTime",BunnyValueKind.Boolean,isStatic:true);
        batch=access.Batch(life,position,day);
        buffIndex=player.Method("FindBuffIndex",BunnyValueKind.Int32,false,"System.Int32");
        hooks.HookVoid("Terraria.Player","ResetEffects",(original,self)=>
        {
            original.Invoke();
            BunnyNativeObject target=BunnyNativeObject.FromHook(self);
            BunnyValue result=batch.ReadAndInvoke(buffIndex,snapshot,target,BunnyValue.FromInt32(24));
            LastLife=snapshot[0].AsInt32();
            LastPosition=snapshot[1].AsVector2();
            LastDayTime=snapshot[2].AsBoolean();
            LastOnFireIndex=result.AsInt32();
        });
    }
}
