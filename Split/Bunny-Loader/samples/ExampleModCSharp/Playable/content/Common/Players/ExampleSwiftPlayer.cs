using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSwiftPlayer : BunnyPlayer
{
    public int LastHurtDamage { get; private set; }

    public override void SetDefaults()
    {
        Accessory = nameof(ExampleSwiftAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            MoveSpeedPercent = 8;
    }

    public override void RegisterHooks(BunnyHookRegistry hooks)
    {
        hooks.HookBeforeHurt(ReduceHurtDamage);
        hooks.HookDodge(CheckDodge);
        hooks.HookAfterHurt(info => LastHurtDamage = info.Damage);
    }

    private static void ReduceHurtDamage(ref int damage, ref int direction, ref bool cancel)
    {
        damage = Math.Max(1, damage - 2);
    }

    private static void CheckDodge(int damage, out bool dodge)
    {
        dodge = damage >= 250;
    }
}
