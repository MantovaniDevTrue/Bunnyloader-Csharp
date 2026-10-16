using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public static class Exports
{
    private static readonly IProjectileBehavior Behavior = new HomingProjectile();
    private static readonly IProjectileBehavior PredictiveBehavior = new PredictiveHomingProjectile();
    private static int _homingType = -1;
    private static int _predictiveType = -1;
    private static readonly BunnyProjectileRegistry Registry = new();
    private static readonly BunnyNpcRegistry NpcRegistry = new();

    static Exports()
    {
        Registry.Register("NativeWaveBolt", new WaveSeekingProjectile());
        NpcRegistry.Register("NativePhaseSlime", new PhaseSlimeBehavior());
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_abi_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int AbiVersion()
    {
        return BunnyApi.NativeModAbi;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_host_api_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int HostApiVersion()
    {
        return BunnyApi.HostApiVersion;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_initialize", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int Initialize(int hostApiVersion)
    {
        return hostApiVersion >= BunnyApi.HostApiVersion ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_bind_projectile", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int BindProjectile(int type, byte* name)
    {
        if (name == null || type < 0) return 0;
        string? projectileName = Marshal.PtrToStringUTF8((nint)name);
        if (projectileName == "NativeHomingBolt")
        {
            _homingType = type;
            return 1;
        }
        if (projectileName == "NativePredictiveBolt")
        {
            _predictiveType = type;
            return 1;
        }
        return projectileName != null && Registry.Bind(type, projectileName) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_bind_npc", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int BindNpc(int type, byte* name)
    {
        if (type < 0 || name == null) return 0;
        string? npcName = Marshal.PtrToStringUTF8((nint)name);
        return npcName != null && NpcRegistry.Bind(type, npcName) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_npc_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int TickNpc(NpcState* state, float playerX, float playerY)
    {
        if (state == null || state->Generation <= 0) return 0;
        NpcContext context = new(state, playerX, playerY);
        return NpcRegistry.Tick(ref context) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_npc_combat_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int NpcCombatTick(NpcState* state, float playerX, float playerY, NpcCombatCommand* output)
    {
        if (state == null || output == null || state->Generation <= 0) return 0;
        NpcContext context = new(state, playerX, playerY);
        bool fired = NpcRegistry.TryAttack(ref context, out NpcCombatCommand attack);
        *output = fired ? attack : default;
        return fired ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_npc_release", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void ReleaseNpc(int type, int slot, int generation)
    {
        NpcRegistry.Release(type, slot, generation);
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_projectile_release", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void ReleaseProjectile(int type, int slot, int owner, int generation)
    {
        Registry.Release(type, slot, owner, generation);
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_projectile_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int ProjectileTick(
        ProjectileState* state,
        NpcSample* npcs,
        int npcCount,
        float playerX,
        float playerY)
    {
        if (state == null || npcs == null || npcCount <= 0) return 0;
        if (npcCount > 4096) npcCount = 4096;

        ProjectileContext context = new(state, npcs, npcCount, playerX, playerY);
        if (Registry.Tick(ref context)) return 1;
        if (state->Type >= 1136 && state->Type != _homingType &&
            state->Type != _predictiveType) return 0;
        if (state->Type == _predictiveType) {
            return PredictiveBehavior.Update(ref context) ? 1 : 0;
        }
        if (state->Type == _homingType || state->Type < 1136) {
            return Behavior.Update(ref context) ? 1 : 0;
        }
        return 0;
    }
}
