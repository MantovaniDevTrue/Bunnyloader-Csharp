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
                     typeof(BunnyBuffPotion).IsAssignableFrom(type))).ToArray();
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
                mod.Items.Add(owner, item);
            }

            if (kind == 2) return "READY";
            var state = BunnyPrefixRuntime.ReadItem(snapshot);
            bool allow = item switch
            {
                BunnyItem basic => basic.AllowPrefix(state, index),
                BunnyWeapon weapon => weapon.AllowPrefix(state, index),
                BunnyAccessory accessory => accessory.AllowPrefix(state, index),
                BunnyBuffPotion potion => potion.AllowPrefix(state, index),
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
