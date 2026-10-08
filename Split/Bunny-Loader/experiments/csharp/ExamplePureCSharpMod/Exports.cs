using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using BunnyLoader.CSharp;

namespace ExamplePureCSharpMod;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeContentSpec
{
    public int Kind;
    public byte* Name;
    public byte* Texture;
    public byte* DisplayName;
    public int Width;
    public int Height;
    public int Damage;
    public int UseTime;
    public float ShootSpeed;
    public int ShootType;
    public int AiStyle;
    public int Penetrate;
    public int TimeLeft;
    public int Flags;
}

public sealed class NativePureSlimeAI : BunnyNpcBehavior
{
    public override bool AI(ref NpcContext context, ref NpcMemory memory)
    {
        float acceleration = context.LifeRatio <= 0.5f ? 0.24f : 0.13f;
        float maxSpeed = context.LifeRatio <= 0.5f ? 5.5f : 3.2f;
        return context.MoveTowards(context.PlayerX, context.PlayerY, acceleration, maxSpeed);
    }
}

public sealed class NativeBoltAI : BunnyProjectileBehavior
{
    public override bool AI(ref ProjectileContext context, ref ProjectileMemory memory)
    {
        if (!context.TryFindClosestHostileInRange(800f, out NpcView target)) return false;
        return context.SteerTowards(target.X, target.Y, 8f, 15f, 0.12f);
    }
}

public static unsafe class Exports
{
    private static readonly BunnyProjectileRegistry Registry = new();
    private static readonly BunnyNpcRegistry Npcs = new();

    static Exports()
    {
        Registry.Register("NativePureBolt", new NativeBoltAI());
        Npcs.Register("NativePureSlime", new NativePureSlimeAI());
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_abi_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int AbiVersion() => BunnyApi.NativeModAbi;

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_host_api_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int HostApiVersion() => BunnyApi.HostApiVersion;

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_initialize", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int Initialize(int hostVersion) => hostVersion >= 1 ? 1 : 0;

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_register_native_content", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int RegisterContent(nint context,
        delegate* unmanaged[Cdecl]<nint, NativeContentSpec*, int> register)
    {
        if (register == null) return 0;
        int projectileId;
        fixed (byte* name = "NativePureBolt\0"u8)
        fixed (byte* texture = "Content/Projectiles/NativePureBolt.png\0"u8)
        fixed (byte* display = "Native Pure Bolt\0"u8)
        {
            NativeContentSpec projectile = new()
            {
                Kind = 1,
                Name = name,
                Texture = texture,
                DisplayName = display,
                Width = 8,
                Height = 8,
                AiStyle = 1,
                Penetrate = 1,
                TimeLeft = 420
            };
            projectileId = register(context, &projectile);
            if (projectileId < 0) return 0;
        }

        fixed (byte* name = "NativePureWand\0"u8)
        fixed (byte* texture = "Content/Items/Weapons/NativePureWand.png\0"u8)
        fixed (byte* display = "Native Pure Wand\0"u8)
        {
            NativeContentSpec item = new()
            {
                Kind = 2,
                Name = name,
                Texture = texture,
                DisplayName = display,
                Damage = 22,
                UseTime = 18,
                ShootSpeed = 13f,
                ShootType = projectileId
            };
            if (register(context, &item) < 0) return 0;
        }
        fixed (byte* name = "NativePureSlime\0"u8)
        fixed (byte* texture = "Content/NPCs/NativePureSlime.png\0"u8)
        fixed (byte* display = "Native Pure Slime\0"u8)
        {
            NativeContentSpec npc = new()
            {
                Kind = 3,
                Name = name,
                Texture = texture,
                DisplayName = display,
                Width = 32,
                Height = 24,
                Damage = 14,
                UseTime = 5,
                ShootType = 90,
                AiStyle = 0,
                Penetrate = 1,
                TimeLeft = 2
            };
            if (register(context, &npc) < 0) return 0;
        }

        int buffType;
        fixed (byte* name = "NativePureDefense\0"u8)
        fixed (byte* texture = "Content/Buffs/NativePureDefense.png\0"u8)
        fixed (byte* display = "Native Pure Defense\0"u8)
        {
            NativeContentSpec buff = new()
            {
                Kind = 5,
                Name = name,
                Texture = texture,
                DisplayName = display,
                Damage = 8
            };
            buffType = register(context, &buff);
            if (buffType < 0) return 0;
        }

        fixed (byte* name = "NativePureAmulet\0"u8)
        fixed (byte* texture = "Content/Items/Accessories/NativePureAmulet.png\0"u8)
        fixed (byte* display = "Native Pure Amulet\0"u8)
        {
            NativeContentSpec accessory = new()
            {
                Kind = 4,
                Name = name,
                Texture = texture,
                DisplayName = display,
                Damage = 4
            };
            if (register(context, &accessory) < 0) return 0;
        }

        fixed (byte* name = "NativePurePotion\0"u8)
        fixed (byte* texture = "Content/Items/Consumables/NativePurePotion.png\0"u8)
        fixed (byte* display = "Native Pure Potion\0"u8)
        {
            NativeContentSpec potion = new()
            {
                Kind = 6,
                Name = name,
                Texture = texture,
                DisplayName = display,
                ShootType = buffType,
                TimeLeft = 3600
            };
            if (register(context, &potion) < 0) return 0;
        }
        return 1;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_bind_npc", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int BindNpc(int type, byte* name)
    {
        string? npcName = name != null ? Marshal.PtrToStringUTF8((nint)name) : null;
        return npcName is not null && Npcs.Bind(type, npcName) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_npc_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int TickNpc(NpcState* state, float playerX, float playerY)
    {
        if (state == null || state->Generation < 1) return 0;
        NpcContext context = new(state, playerX, playerY);
        return Npcs.Tick(ref context) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_npc_release", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void ReleaseNpc(int type, int slot, int generation)
    {
        Npcs.Release(type, slot, generation);
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_bind_projectile", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int BindProjectile(int type, byte* name)
    {
        string? projectileName = name != null ? Marshal.PtrToStringUTF8((nint)name) : null;
        return projectileName is not null && Registry.Bind(type, projectileName) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_projectile_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int TickProjectile(ProjectileState* state, NpcSample* npcs, int count, float playerX, float playerY)
    {
        if (state == null || npcs == null || count <= 0 || count > 4096) return 0;
        ProjectileContext context = new(state, npcs, count, playerX, playerY);
        return Registry.Tick(ref context) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunny_mod_projectile_release", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void ReleaseProjectile(int type, int slot, int owner, int generation)
    {
        Registry.Release(type, slot, owner, generation);
    }
}
