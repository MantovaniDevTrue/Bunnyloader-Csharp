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
        public BunnyPlayer[] Players = Array.Empty<BunnyPlayer>();
    }

    private static readonly List<Context> Contexts = new();

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
        try
        {
            var assembly = Assembly.LoadFrom(assemblyPath);
            var types = assembly.GetTypes()
                .Where(type => type.IsClass && !type.IsAbstract &&
                    type.BaseType?.FullName == "BunnyLoader.Source.BunnyPlayer")
                .OrderBy(type => type.Name, StringComparer.Ordinal)
                .ToArray();
            if (types.Length == 0 || types.Length > 16)
                return -1;
            var players = new BunnyPlayer[types.Length];
            for (int i = 0; i < types.Length; i++)
            {
                players[i] = (BunnyPlayer)(Activator.CreateInstance(types[i])
                    ?? throw new InvalidOperationException("BunnyPlayer instance unavailable"));
                players[i].SetDefaults();
            }
            Contexts.Add(new Context { Id = modId, Path = assemblyPath, Players = players });
            return Contexts.Count - 1;
        }
        catch
        {
            return -1;
        }
    }

    public static long Modify(int slot, int damage, int direction, int flags,
                              int life, int maxLife, int equippedMask)
    {
        if (slot < 0 || slot >= Contexts.Count || damage < 0 || damage > 1000000)
            return -1;
        try
        {
            var modifiers = new BunnyHurtModifiers
            {
                Damage = damage,
                HitDirection = direction,
                PvP = (flags & 1) != 0,
                Quiet = (flags & 2) != 0,
                Crit = (flags & 4) != 0,
                Dodgeable = (flags & 8) != 0,
                CurrentLife = life,
                MaxLife = maxLife
            };
            var players = Contexts[slot].Players;
            for (int i = 0; i < players.Length; i++)
            {
                var player = players[i];
                bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                    (equippedMask & (1 << i)) != 0;
                player.ResetEffects();
                player.UpdateEquips(equipped);
                player.ModifyHurt(equipped, modifiers);
            }
            bool dodged = false;
            if (!modifiers.Cancelled && modifiers.Dodgeable)
            {
                for (int i = 0; i < players.Length; i++)
                {
                    var player = players[i];
                    bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                        (equippedMask & (1 << i)) != 0;
                    if (!player.FreeDodge(equipped, modifiers))
                        continue;
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
            return d | ((long)dir << 20) | ((long)outputFlags << 22);
        }
        catch
        {
            return -1;
        }
    }

    public static int After(int slot, int damage, int sourceDamage,
                            int direction, int flags, int lifeRemaining, int equippedMask)
    {
        if (slot < 0 || slot >= Contexts.Count || damage <= 0)
            return 0;
        try
        {
            var info = new BunnyHurtInfo(damage, sourceDamage, direction,
                lifeRemaining, (flags & 1) != 0, (flags & 4) != 0);
            var players = Contexts[slot].Players;
            for (int i = 0; i < players.Length; i++)
            {
                var player = players[i];
                bool equipped = string.IsNullOrEmpty(player.Accessory) ||
                    (equippedMask & (1 << i)) != 0;
                player.OnHurt(equipped, info);
            }
            return players.Length;
        }
        catch
        {
            return -1;
        }
    }
}
