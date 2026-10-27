using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace BunnyLoader.Source;

public static class BunnyItemPrefixRuntime
{
    private sealed class Mod
    {
        public string Path = "";
        public Assembly Assembly = null!;
        public readonly Dictionary<string, object> Items = new(StringComparer.Ordinal);
    }

    private static readonly Dictionary<string, Mod> mods = new(StringComparer.Ordinal);

    public static string Dispatch(string modId, string assemblyPath, string owner,
                                  int index, int kind, string snapshot)
    {
        try
        {
            if (modId.Length == 0 || modId.Length > 128 ||
                owner.Length == 0 || owner.Length > 64 ||
                assemblyPath.Length == 0 || assemblyPath.Length > 4096 ||
                !((kind == 2 && index == 0 && snapshot.Length == 0) ||
                  (kind == 0 && index >= 1 && index <= 255 &&
                   snapshot.Length > 0 && snapshot.Length <= 512) ||
                  (kind == 1 && index == 0 &&
                   snapshot.Length > 0 && snapshot.Length <= 512) ||
                  (kind == 3 && index == 0 &&
                   snapshot.Length > 0 && snapshot.Length <= 512) ||
                  (kind == 4 && (index == 1 || index == 2) &&
                   snapshot.Length > 0 && snapshot.Length <= 512) ||
                  (kind == 5 && index >= 1 && index <= 255 &&
                   snapshot.Length > 0 && snapshot.Length <= 512)))
                throw new ArgumentException("Invalid item prefix callback transport");

            if (!mods.TryGetValue(modId, out var mod) || mod.Path != assemblyPath)
            {
                mod = new Mod
                {
                    Path = assemblyPath,
                    Assembly = Assembly.Load(File.ReadAllBytes(assemblyPath))
                };
                mods[modId] = mod;
            }

            if (!mod.Items.TryGetValue(owner, out object? item))
            {
                var matches = mod.Assembly.GetTypes().Where(type =>
                    type.Name == owner && type.IsClass && !type.IsAbstract &&
                    !type.ContainsGenericParameters &&
                    (typeof(BunnyItem).IsAssignableFrom(type) ||
                     typeof(BunnyWeapon).IsAssignableFrom(type) ||
                     typeof(BunnyAccessory).IsAssignableFrom(type) ||
                     typeof(BunnyBuffPotion).IsAssignableFrom(type) ||
                     typeof(BunnyGlobalItem).IsAssignableFrom(type))).ToArray();
                if (matches.Length != 1)
                    throw new InvalidOperationException("Item prefix owner must resolve to one class");
                var type = matches[0];
                item = Activator.CreateInstance(type)
                    ?? throw new InvalidOperationException("Item prefix instance unavailable");
                switch (item)
                {
                    case BunnyItem basic: basic.SetDefaults(); break;
                    case BunnyWeapon weapon: weapon.SetDefaults(); break;
                    case BunnyAccessory accessory: accessory.SetDefaults(); break;
                    case BunnyBuffPotion potion: potion.SetDefaults(); break;
                }
                var method = type.GetMethod("AllowPrefix",
                    new[] { typeof(BunnyPrefixItemSnapshot), typeof(int) });
                if (method == null)
                    throw new MissingMethodException(type.FullName, "AllowPrefix");
                RuntimeHelpers.PrepareMethod(method.MethodHandle);
                var snapshotType=typeof(BunnyPrefixItemSnapshot);
                var dynamicMethods=new (string Name, Type[] Parameters)[] {
                    ("ChoosePrefix", new[] { snapshotType }),
                    ("DecidePrefixChance", new[] { snapshotType, typeof(int) }),
                    ("ApplyPrefix", new[] {
                        snapshotType, typeof(int), typeof(BunnyPrefixApplyStats).MakeByRefType()
                    })
                };
                foreach (var entry in dynamicMethods)
                {
                    MethodInfo? callback=type.GetMethod(entry.Name,entry.Parameters);
                    if (callback == null)
                        throw new MissingMethodException(type.FullName,entry.Name);
                    RuntimeHelpers.PrepareMethod(callback.MethodHandle);
                }
                if (item is BunnyGlobalItem)
                {
                    MethodInfo applies=type.GetMethod("AppliesTo",new[] { snapshotType })
                        ?? throw new MissingMethodException(type.FullName,"AppliesTo");
                    RuntimeHelpers.PrepareMethod(applies.MethodHandle);
                }
                mod.Items.Add(owner, item);
            }

            if (kind == 2) return "READY";
            if (kind == 1)
            {
                int separator = snapshot.IndexOf('|');
                if (separator <= 0 || separator == snapshot.Length - 1)
                    throw new FormatException("Missing batch separator");
                string hex = snapshot[(separator + 1)..];
                if (hex.Length < 2 || hex.Length > 256 || (hex.Length & 1) != 0)
                    throw new FormatException("Invalid batch candidate length");
                var batchState = BunnyPrefixRuntime.ReadItem(snapshot[..separator]);
                Span<bool> seen = stackalloc bool[256];
                seen.Clear();
                if (item is BunnyGlobalItem global && !global.AppliesTo(batchState))
                    return new string('1', hex.Length / 2);
                char[] results = new char[hex.Length / 2];
                for (int i = 0; i < results.Length; i++)
                {
                    int candidate = int.Parse(hex.AsSpan(i * 2, 2),
                        NumberStyles.HexNumber, CultureInfo.InvariantCulture);
                    if (candidate == 0 || seen[candidate])
                        throw new FormatException("Duplicate or zero prefix");
                    seen[candidate] = true;
                    bool allowed = item switch
                    {
                        BunnyItem basic => basic.AllowPrefix(batchState, candidate),
                        BunnyWeapon weapon => weapon.AllowPrefix(batchState, candidate),
                        BunnyAccessory accessory => accessory.AllowPrefix(batchState, candidate),
                        BunnyBuffPotion potion => potion.AllowPrefix(batchState, candidate),
                        BunnyGlobalItem globalItem => globalItem.AllowPrefix(batchState, candidate),
                        _ => throw new InvalidOperationException("Invalid item prefix owner")
                    };
                    results[i] = allowed ? '1' : '0';
                }
                return new string(results);
            }
            var state = BunnyPrefixRuntime.ReadItem(snapshot);
            if (item is BunnyGlobalItem globalItemInstance && !globalItemInstance.AppliesTo(state))
                return kind switch {
                    0 => "1", 3 => "-1", 4 => "N", 5 => "0,0",
                    _ => throw new InvalidOperationException("Invalid global prefix operation")
                };
            if (kind == 3)
            {
                int chosen = item switch
                {
                    BunnyItem basic => basic.ChoosePrefix(state),
                    BunnyWeapon weapon => weapon.ChoosePrefix(state),
                    BunnyAccessory accessory => accessory.ChoosePrefix(state),
                    BunnyBuffPotion potion => potion.ChoosePrefix(state),
                    BunnyGlobalItem globalItem => globalItem.ChoosePrefix(state),
                    _ => throw new InvalidOperationException("Invalid item prefix owner")
                };
                if (chosen != -1 && (chosen < 1 || chosen > 255))
                    throw new ArgumentOutOfRangeException(nameof(chosen));
                return chosen.ToString(CultureInfo.InvariantCulture);
            }
            if (kind == 4)
            {
                bool? chance = item switch
                {
                    BunnyItem basic => basic.DecidePrefixChance(state, -index),
                    BunnyWeapon weapon => weapon.DecidePrefixChance(state, -index),
                    BunnyAccessory accessory => accessory.DecidePrefixChance(state, -index),
                    BunnyBuffPotion potion => potion.DecidePrefixChance(state, -index),
                    BunnyGlobalItem globalItem => globalItem.DecidePrefixChance(state, -index),
                    _ => throw new InvalidOperationException("Invalid item prefix owner")
                };
                return chance.HasValue ? (chance.Value ? "1" : "0") : "N";
            }
            if (kind == 5)
            {
                BunnyPrefixApplyStats effect = new();
                switch (item)
                {
                    case BunnyItem basic: basic.ApplyPrefix(state, index, ref effect); break;
                    case BunnyWeapon weapon: weapon.ApplyPrefix(state, index, ref effect); break;
                    case BunnyAccessory accessory: accessory.ApplyPrefix(state, index, ref effect); break;
                    case BunnyBuffPotion potion: potion.ApplyPrefix(state, index, ref effect); break;
                    case BunnyGlobalItem globalItem: globalItem.ApplyPrefix(state, index, ref effect); break;
                    default: throw new InvalidOperationException("Invalid item prefix owner");
                }
                if (effect.DamageBonus < 0 || effect.DamageBonus > 1000 ||
                    effect.ValueBonus < 0 || effect.ValueBonus > 100000)
                    throw new InvalidOperationException("Dynamic item prefix effects out of range");
                return effect.DamageBonus.ToString(CultureInfo.InvariantCulture) + "," +
                    effect.ValueBonus.ToString(CultureInfo.InvariantCulture);
            }
            bool allow = item switch
            {
                BunnyItem basic => basic.AllowPrefix(state, index),
                BunnyWeapon weapon => weapon.AllowPrefix(state, index),
                BunnyAccessory accessory => accessory.AllowPrefix(state, index),
                BunnyBuffPotion potion => potion.AllowPrefix(state, index),
                BunnyGlobalItem globalItem => globalItem.AllowPrefix(state, index),
                _ => throw new InvalidOperationException("Invalid item prefix owner")
            };
            return allow ? "1" : "0";
        }
        catch (Exception error)
        {
            return "ERROR|[CSharp][ERROR][ItemPrefixCallback] Mod=" + modId +
                " Class=" + owner + " Kind=" + kind +
                " Reason=ManagedException Result=Rejected Exception=" + error;
        }
    }
}
