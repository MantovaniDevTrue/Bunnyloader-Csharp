using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public static class Exports
{
    private static readonly IProjectileBehavior Behavior = new HomingProjectile();

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
        return Behavior.Update(ref context) ? 1 : 0;
    }
}
