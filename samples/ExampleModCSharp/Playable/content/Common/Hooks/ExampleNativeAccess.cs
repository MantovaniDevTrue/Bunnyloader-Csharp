using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleNativeAccess : BunnyHookModule
{
    public static readonly bool EnableAtLoad = false;
    private readonly BunnyValue[] snapshot = new BunnyValue[3];
    private BunnyNativeBatch batch = null!;
    private BunnyNativeMember findBuffIndex = null!;

    public static bool PairMethodPresent { get; private set; }
    public static string PairMethodError { get; private set; } = "";
    public static long ResetCalls { get; private set; }
    public static long Samples { get; private set; }
    public static int LastLife { get; private set; }
    public static BunnyVector2 LastPosition { get; private set; }
    public static bool LastDayTime { get; private set; }
    public static int LastOnFireIndex { get; private set; }
    public static string LastError { get; private set; } = "";

    public override void Load(BunnyHookScope hooks)
    {
        if (!EnableAtLoad) return;
        try
        {
            BunnyNativeScope access = hooks.Native;
            BunnyNativeType player = access.Type("Terraria.Player");
            BunnyNativeType main = access.Type("Terraria.Main");
            var life = player.Field("statLife", BunnyValueKind.Int32);
            var position = player.Field("position", BunnyValueKind.Vector2);
            var dayTime = main.Field("dayTime", BunnyValueKind.Boolean, isStatic: true);
            batch = access.Batch(life, position, dayTime);
            findBuffIndex = player.Method("FindBuffIndex", BunnyValueKind.Int32, false, "System.Int32");
            try
            {
                access.Type("Terraria.Utilities.UnifiedRandom").Method("Next", BunnyValueKind.Int32, false, "System.Int32", "System.Int32");
                PairMethodPresent = true;
            }
            catch (Exception error)
            {
                PairMethodError = error.GetType().Name + ": " + error.Message;
            }
            hooks.HookVoid("Terraria.Player", "ResetEffects", (original, self) =>
            {
                original.Invoke();
                long calls = ++ResetCalls;
                if ((calls & 63L) != 0)
                    return;
                BunnyNativeObject target = BunnyNativeObject.FromHook(self);
                BunnyValue buffIndex = batch.ReadAndInvoke(findBuffIndex, snapshot, target, BunnyValue.FromInt32(24));
                LastLife = snapshot[0].AsInt32();
                LastPosition = snapshot[1].AsVector2();
                LastDayTime = snapshot[2].AsBoolean();
                LastOnFireIndex = buffIndex.AsInt32();
                Samples++;
            });
        }
        catch (Exception error)
        {
            LastError = error.GetType().Name + ": " + error.Message;
        }
    }
}
