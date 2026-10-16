using BunnyLoader.Source;
using System.Reflection;

static void Check(bool value, string message)
{
    if (!value) throw new InvalidOperationException(message);
}

var registry = new BunnyHookRegistry();
int order = 0;
BunnyHookHandle first = registry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) =>
{
    Check(order++ == 0, "BeforeHurt first handler order");
    damage -= 4;
    direction = -1;
});
BunnyHookHandle second = registry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) =>
{
    Check(order++ == 1 && damage == 16, "BeforeHurt mutable ref pipeline");
    damage += 2;
    cancel = true;
});
int d = 20, dir = 1;
bool cancel = false;
registry.InvokeBefore(ref d, ref dir, ref cancel);
Check(d == 18 && dir == -1 && cancel && registry.Count == 2,
    "before ref mutation, direction and cancellation");
Check(registry.Unhook(second) && !registry.Unhook(second) && registry.Count == 1,
    "handle must unregister exactly once");
order = 0;
d = 10; dir = 1; cancel = false;
registry.InvokeBefore(ref d, ref dir, ref cancel);
Check(d == 6 && dir == -1 && !cancel, "unhooked handler must not run");
Check(registry.BeforeCalls == 3 && registry.HasLastDamage &&
      registry.LastDamageBefore == 10 && registry.LastDamageAfter == 6 &&
      registry.CancelledByHook == 1, "BeforeHurt live telemetry and last ref damage");
Check(registry.Unhook(first) && registry.Count == 0, "unhook first");
var query = registry.HookDodge((int damage, out bool dodge) => dodge = damage >= 50);
Check(!registry.InvokeDodge(49) && registry.InvokeDodge(50),
    "out parameter changed in native dispatcher contract");
Check(registry.DodgeChecks == 2 && registry.SuccessfulDodges == 1,
    "Dodge checks and successful out result telemetry");
Check(registry.Unhook(query), "unhook out handler");
int afterCount = 0;
var after = registry.HookAfterHurt(info => afterCount += info.Damage);
registry.InvokeAfter(new BunnyHurtInfo(9, 11, 1, 72, false, false));
Check(afterCount == 9 && registry.Unhook(after), "after hook registration");
Check(registry.AfterCalls == 1 && registry.UnhookCalls == 4,
    "AfterHurt and successful Unhook telemetry");
registry.Clear();
Check(registry.Count == 0, "clear registry");
for (int i = 0; i < 24; i++)
    registry.HookDodge((int damage, out bool dodge) => dodge = false);
bool limitReached = false;
try { registry.HookDodge((int damage, out bool dodge) => dodge = false); }
catch (InvalidOperationException) { limitReached = true; }
Check(limitReached && registry.Count == 24, "registration cap");
registry.Clear();

var mutableRegistry = new BunnyHookRegistry();
BunnyHookHandle selfRemoving = default;
selfRemoving = mutableRegistry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) =>
{
    damage += 1;
    Check(mutableRegistry.Unhook(selfRemoving), "self unhook during invocation");
});
mutableRegistry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) => damage += 2);
int mutableDamage = 5, mutableDirection = 1;
bool mutableCancel = false;
mutableRegistry.InvokeBefore(ref mutableDamage, ref mutableDirection, ref mutableCancel);
Check(mutableDamage == 8 && mutableRegistry.Count == 1 && mutableRegistry.BeforeCalls == 2,
    "snapshot traversal must survive mutation");
mutableDamage = 5;
mutableRegistry.InvokeBefore(ref mutableDamage, ref mutableDirection, ref mutableCancel);
Check(mutableDamage == 7 && mutableRegistry.BeforeCalls == 3,
    "removed hook must not reappear on next call");

var isolatedRegistry = new BunnyHookRegistry();
isolatedRegistry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) =>
{
    damage = 0;
    direction = -1;
    cancel = true;
    throw new InvalidOperationException("expected fault");
});
isolatedRegistry.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) => damage -= 2);
int safeDamage = 10, safeDirection = 1;
bool safeCancel = false;
isolatedRegistry.InvokeBefore(ref safeDamage, ref safeDirection, ref safeCancel);
Check(safeDamage == 8 && safeDirection == 1 && !safeCancel &&
      isolatedRegistry.BeforeCalls == 1 && isolatedRegistry.FaultedHooks == 1 &&
      isolatedRegistry.Count == 1 && isolatedRegistry.LastHookError.Contains("expected fault"),
    "faulty BeforeHurt cannot leak partial ref writes or stop healthy handlers");
safeDamage = 10;
isolatedRegistry.InvokeBefore(ref safeDamage, ref safeDirection, ref safeCancel);
Check(safeDamage == 8 && isolatedRegistry.FaultedHooks == 1,
    "quarantined handler must not run again");
isolatedRegistry.HookDodge((int damage, out bool dodge) =>
{
    dodge = true;
    throw new InvalidOperationException("broken dodge");
});
isolatedRegistry.HookDodge((int damage, out bool dodge) => dodge = true);
Check(isolatedRegistry.InvokeDodge(20) && isolatedRegistry.DodgeChecks == 1 &&
      isolatedRegistry.FaultedHooks == 2, "dodge failure isolation");
isolatedRegistry.HookAfterHurt(info => throw new InvalidOperationException("broken after"));
int afterSafe = 0;
isolatedRegistry.HookAfterHurt(info => afterSafe++);
isolatedRegistry.InvokeAfter(new BunnyHurtInfo(9, 11, 1, 72, false, false));
Check(afterSafe == 1 && isolatedRegistry.AfterCalls == 1 &&
      isolatedRegistry.FaultedHooks == 3, "after hook failure isolation");
Console.WriteLine("BUNNY_CSHARP_HOOK_MUTATION_AND_ISOLATION=PASS");

string modId = Guid.NewGuid().ToString("D");
int slot = BunnyPlayerHurtDispatcher.Register(modId, Assembly.GetExecutingAssembly().Location);
Check(slot >= 0, "register source player: " + BunnyPlayerHurtDispatcher.GetLastError(-1));
long modified = BunnyPlayerHurtDispatcher.Modify(slot, 40, 1, 8, 100, 500, 0, -1, -1,
    -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0);
Check((modified & 0xfffff) == 35, "real dispatcher ref damage pipeline");
Check(((modified >> 20) & 3) == 0, "real dispatcher ref direction");
Check(((modified >> 22) & 16) == 0, "real dispatcher non-cancel");
long dodged = BunnyPlayerHurtDispatcher.Modify(slot, 17, 1, 8, 100, 500, 0, -1, -1,
    -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0);
Check((dodged & 0xfffff) == 12, "real dispatcher damage prior to dodge");
Check(((dodged >> 22) & 48) == 48, "out dodge must reach native packed result");
long cancelled = BunnyPlayerHurtDispatcher.Modify(slot, 6, 1, 8, 100, 500, 0, -1, -1,
    -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0);
Check(((cancelled >> 22) & 16) == 16, "ref cancellation must reach native packed result");
int delivered = BunnyPlayerHurtDispatcher.After(slot, 8, 13, 1, 0, 92, 0, -1, -1,
    -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0);
Check(delivered == 1 && HookFixturePlayer.AfterDamage == 8, "after hook from Mono dispatcher");
string live = BunnyPlayerHurtDispatcher.GetHookSnapshot();
Check(live.Contains("Before 3") && live.Contains("Dodge 2 (esquivas 1)") &&
      live.Contains("After 1") && live.Contains("Cancel 1") && live.Contains("Falhas 0") &&
      live.Contains("dano ref 6 -> 1") && live.Contains("HookFixturePlayer"),
    "Live snapshot must reflect actual managed delegate execution");
Check(string.IsNullOrEmpty(BunnyPlayerHurtDispatcher.GetLastError(slot)),
    "unexpected native hook managed diagnostic");
Console.WriteLine("BUNNY_CSHARP_HOOKS_REF_OUT_DELEGATE=PASS");
Console.WriteLine("BUNNY_CSHARP_PLAYER_HURT_NATIVE_BRIDGE=PASS");
Console.WriteLine("BUNNY_CSHARP_EDITOR_HOOK_COUNTERS=PASS");

public sealed class HookFixturePlayer : BunnyPlayer
{
    public static int AfterDamage;
    public override void SetDefaults() { }
    public override void UpdateEquips(bool accessoryEquipped) { }
    public override void RegisterHooks(BunnyHookRegistry hooks)
    {
        hooks.HookBeforeHurt((ref int damage, ref int direction, ref bool cancel) =>
        {
            damage = Math.Max(0, damage - 5);
            direction = -1;
            if (damage <= 2) cancel = true;
        });
        hooks.HookDodge((int damage, out bool dodge) => dodge = damage == 12);
        hooks.HookAfterHurt(info => AfterDamage = info.Damage);
    }
}
