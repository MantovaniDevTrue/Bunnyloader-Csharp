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
        return 0;
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
        if (state->Type == _predictiveType) {
            return PredictiveBehavior.Update(ref context) ? 1 : 0;
        }
        if (state->Type == _homingType || state->Type < 1136) {
            return Behavior.Update(ref context) ? 1 : 0;
        }
        return 0;
    }
}
