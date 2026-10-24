using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

public static class BunnyPlayerHurtDispatcher
{
    private sealed class Context
    {
        public string Id = "";
        public string Path = "";
        public BunnyModAssemblyLease? Lease;
        public BunnyPlayer[] Players = Array.Empty<BunnyPlayer>();
        public BunnyHookRegistry[] Hooks = Array.Empty<BunnyHookRegistry>();
        public string LastError = "";
    }

    private static readonly List<Context> Contexts = new();
    private static string RegistrationError = "";

    // O worker MonoVM serializa estas chamadas. O ultimo erro fica disponivel
    // mesmo depois de outro callback funcionar; nao ha IO no caminho de sucesso.
    public static string GetLastError(int slot) =>
        slot >= 0 && slot < Contexts.Count ? Contexts[slot].LastError : RegistrationError;

    public static string GetHookSnapshot()
    {
        long before = 0, checks = 0, dodges = 0, after = 0, unhooks = 0, cancels = 0, faults = 0;
        int registered = 0, equipped = 0;
        int incoming = 0, outgoing = 0;
        string lastPlayer = "";
        string lastFault = "";
        bool hasDamage = false;
        foreach (var context in Contexts)
        {
            for (int i = 0; i < context.Hooks.Length; i++)
            {
                BunnyHookRegistry hook = context.Hooks[i];
                registered += hook.Count;
                if (hook.LastEquipped) equipped += hook.Count;
                before += hook.BeforeCalls;
                checks += hook.DodgeChecks;
                dodges += hook.SuccessfulDodges;
                after += hook.AfterCalls;
                unhooks += hook.UnhookCalls;
                cancels += hook.CancelledByHook;
                faults += hook.FaultedHooks;
                if (hook.LastHookError.Length > 0)
                    lastFault = hook.LastHookError;
                if (hook.HasLastDamage)
                {
                    incoming = hook.LastDamageBefore;
                    outgoing = hook.LastDamageAfter;
                    lastPlayer = context.Players[i].GetType().Name;
                    hasDamage = true;
                }
            }
        }
        string last = hasDamage
            ? incoming + " -> " + outgoing + " (" + lastPlayer + ")"
            : "aguardando golpe";
        return "Hooks C#  ativos " + equipped + "/" + registered +
               "  |  Before " + before + "  |  Dodge " + checks +
               " (esquivas " + dodges + ")" +
               "\nAfter " + after + "  |  Unhook " + unhooks +
               "  |  Cancel " + cancels + "  |  Falhas " + faults + "  |  dano ref " + last +
               (lastFault.Length == 0 ? "" : "\nÚltima falha: " + lastFault);
    }

    private static string Describe(string mod, string type, string hook, Exception error)
    {
        if (error is TargetInvocationException wrapped && wrapped.InnerException is not null)
            error = wrapped.InnerException;
        string value = "BUNNY_CSHARP_CALLBACK_ERROR mod=" + mod + " class=" + type +
            " hook=" + hook + " exception=" + error.GetType().Name + " message=" + error.Message +
            "\n" + (error.StackTrace ?? "(pilha indisponivel; esta build nao emite PDB)");
        return value.Length <= 2048 ? value : value.Substring(0, 2048) + " [truncado]";
    }

    public static int Register(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(modId) || string.IsNullOrWhiteSpace(assemblyPath))
            return -1;
        for (int i = 0; i < Contexts.Count; i++)
        {
            if (Contexts[i].Id == modId)
                return Contexts[i].Path == assemblyPath ? i : -1;
        }
        if (Contexts.Count >= 16)
            return -1;
        string currentType = "(assembly)";
        string currentHook = "SharedAssembly";
        BunnyModAssemblyLease? lease = null;
        try
        {
            lease = BunnyModAssemblyRegistry.Acquire(modId, assemblyPath);
            var assembly = lease.Assembly;
            var types = assembly.GetTypes()
                .Where(type => type.IsClass && !type.IsAbstract &&
                    !type.ContainsGenericParameters && typeof(BunnyPlayer).IsAssignableFrom(type))
                .OrderBy(type => type.Name, StringComparer.Ordinal)
                .ToArray();
            if (types.Length == 0 || types.Length > 16)
                return -1;
            var players = new BunnyPlayer[types.Length];
            var hookRegistries = new BunnyHookRegistry[types.Length];
            for (int i = 0; i < types.Length; i++)
            {
                currentType = types[i].FullName ?? types[i].Name;
                currentHook = ".ctor";
                players[i] = (BunnyPlayer)(Activator.CreateInstance(types[i])
                    ?? throw new InvalidOperationException("BunnyPlayer instance unavailable"));
                currentHook = "SetDefaults";
                players[i].SetDefaults();
                currentHook = "RegisterHooks";
                hookRegistries[i] = new BunnyHookRegistry();
                players[i].RegisterHooks(hookRegistries[i]);
            }
            Contexts.Add(new Context {
                Id = modId, Path = assemblyPath, Lease = lease,
                Players = players, Hooks = hookRegistries
            });
            lease = null;
            return Contexts.Count - 1;
        }
        catch (Exception error)
        {
            RegistrationError = Describe(modId, currentType, currentHook, error);
            return -1;
        }
        finally { lease?.Dispose(); }
    }

    public static long Modify(int slot, int damage, int direction, int flags,
                              int life, int maxLife, int equippedMask,
                              int npcIndex, int projectileIndex,
                              int npcType, int npcDamage, int npcLife,
                              int projectileType, int projectileDamage, int projectileOwner,
                              float attackerX, float attackerY,
                              float attackerVelocityX, float attackerVelocityY,
                              float attackerAngle, float attackerDistance)
    {
        if (slot < 0 || slot >= Contexts.Count || damage < 0 || damage > 1000000)
            return -1;
        string currentType = "(dispatcher)";
        string currentHook = "context";
        try
        {
            npcIndex = npcIndex >= 0 && npcIndex < 200 ? npcIndex : -1;
            projectileIndex = npcIndex < 0 && projectileIndex >= 0 && projectileIndex < 1000
                ? projectileIndex : -1;
            var modifiers = new BunnyHurtModifiers
            {
                Damage = damage,
                HitDirection = direction,
                PvP = (flags & 1) != 0,
                Quiet = (flags & 2) != 0,
                Crit = (flags & 4) != 0,
                Dodgeable = (flags & 8) != 0,
                CurrentLife = life,
                MaxLife = maxLife,
                NpcIndex = npcIndex,
                ProjectileIndex = projectileIndex,
                NpcType = npcType,
                NpcDamage = npcDamage,
                NpcLife = npcLife,
                ProjectileType = projectileType,
                ProjectileDamage = projectileDamage,
                ProjectileOwner = projectileOwner,
                AttackerX = attackerX,
                AttackerY = attackerY,
                AttackerVelocityX = attackerVelocityX,
                AttackerVelocityY = attackerVelocityY,
                AttackerAngle = attackerAngle,
                AttackerDistance = attackerDistance
            };
            var players = Contexts[slot].Players;
            var hooks = Contexts[slot].Hooks;
            for (int i = 0; i < players.Length; i++)
            {
                var player = players[i];
                currentType = player.GetType().FullName ?? player.GetType().Name;
                bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                    (equippedMask & (1 << i)) != 0;
                hooks[i].LastEquipped = equipped;
                currentHook = "ResetEffects";
                player.ResetEffects();
                currentHook = "UpdateEquips";
                player.UpdateEquips(equipped);
                currentHook = "ModifyHurt";
                player.ModifyHurt(equipped, modifiers);
                if (npcIndex >= 0)
                {
                    currentHook = "ModifyHitByNPC";
                    player.ModifyHitByNPC(equipped, npcIndex, modifiers);
                }
                else if (projectileIndex >= 0)
                {
                    currentHook = "ModifyHitByProjectile";
                    player.ModifyHitByProjectile(equipped, projectileIndex, modifiers);
                }
                if (equipped && hooks[i].Count > 0)
                {
                    currentHook = "HookBeforeHurt";
                    int currentDamage = modifiers.Damage;
                    int currentDirection = modifiers.HitDirection;
                    bool cancelled = modifiers.Cancelled;
                    hooks[i].InvokeBefore(ref currentDamage, ref currentDirection, ref cancelled);
                    modifiers.Damage = currentDamage;
                    modifiers.HitDirection = currentDirection;
                    if (cancelled) modifiers.Cancel();
                }
            }
            bool dodged = false;
            if (!modifiers.Cancelled && modifiers.Dodgeable)
            {
                for (int i = 0; i < players.Length; i++)
                {
                    var player = players[i];
                    currentType = player.GetType().FullName ?? player.GetType().Name;
                    bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                        (equippedMask & (1 << i)) != 0;
                    currentHook = "FreeDodge";
                    bool freeDodge = player.FreeDodge(equipped, modifiers);
                    if (!freeDodge && equipped && hooks[i].Count > 0)
                    {
                        currentHook = "HookDodge";
                        freeDodge = hooks[i].InvokeDodge(modifiers.Damage);
                    }
                    if (!freeDodge) continue;
                    modifiers.Cancel();
                    dodged = true;
                    break;
                }
            }
            int d = Math.Clamp(modifiers.Damage, 0, 1000000);
            int dir = Math.Clamp(modifiers.HitDirection, -1, 1) + 1;
            int outputFlags = (modifiers.PvP ? 1 : 0) |
                (modifiers.Quiet ? 2 : 0) |
                (modifiers.Crit ? 4 : 0) |
                (modifiers.Dodgeable ? 8 : 0) |
                (modifiers.Cancelled ? 16 : 0) |
                (dodged ? 32 : 0);
            return (long)(uint)d | ((long)dir << 20) | ((long)outputFlags << 22);
        }
        catch (Exception error)
        {
            Contexts[slot].LastError = Describe(Contexts[slot].Id, currentType, currentHook, error);
            return -1;
        }
    }

    public static int After(int slot, int damage, int sourceDamage,
                            int direction, int flags, int lifeRemaining, int equippedMask,
                            int npcIndex, int projectileIndex,
                            int npcType, int npcDamage, int npcLife,
                            int projectileType, int projectileDamage, int projectileOwner,
                            float attackerX, float attackerY,
                            float attackerVelocityX, float attackerVelocityY,
                            float attackerAngle, float attackerDistance)
    {
        if (slot < 0 || slot >= Contexts.Count || damage <= 0)
            return 0;
        string currentType = "(dispatcher)";
        string currentHook = "context";
        try
        {
            npcIndex = npcIndex >= 0 && npcIndex < 200 ? npcIndex : -1;
            projectileIndex = projectileIndex >= 0 && projectileIndex < 1000
                ? projectileIndex : -1;
            var info = new BunnyHurtInfo(damage, sourceDamage, direction,
                lifeRemaining, (flags & 1) != 0, (flags & 4) != 0,
                npcIndex, projectileIndex,
                npcType, npcDamage, npcLife,
                projectileType, projectileDamage, projectileOwner,
                attackerX, attackerY, attackerVelocityX, attackerVelocityY,
                attackerAngle, attackerDistance);
            var players = Contexts[slot].Players;
            var hooks = Contexts[slot].Hooks;
            for (int i = 0; i < players.Length; i++)
            {
                var player = players[i];
                currentType = player.GetType().FullName ?? player.GetType().Name;
                bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                    (equippedMask & (1 << i)) != 0;
                currentHook = "OnHurt";
                player.OnHurt(equipped, info);
                if (npcIndex >= 0)
                {
                    currentHook = "OnHitByNPC";
                    player.OnHitByNPC(equipped, npcIndex, info);
                }
                else if (projectileIndex >= 0)
                {
                    currentHook = "OnHitByProjectile";
                    player.OnHitByProjectile(equipped, projectileIndex, info);
                }
                if (equipped && hooks[i].Count > 0)
                {
                    currentHook = "HookAfterHurt";
                    hooks[i].InvokeAfter(info);
                }
            }
            return players.Length;
        }
        catch (Exception error)
        {
            Contexts[slot].LastError = Describe(Contexts[slot].Id, currentType, currentHook, error);
            return -1;
        }
    }
}

