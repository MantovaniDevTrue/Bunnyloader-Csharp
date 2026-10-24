// Adapted from tModLoader 1.4.5 ModTypeLookup.cs, commit fcafebc0b792b90e110bac7c1fc67cac81cb60ed.
// Copyright 2019 tModLoader Team. MIT; see THIRD_PARTY_NOTICES.md.
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// tModLoader's two-level typed content lookup: each T owns an independent registry.
/// Registers canonical names plus LegacyName attributes. Bunny adds atomic alias validation,
/// per-mod removal and a shared lifecycle lock; retrieval semantics remain case-sensitive.
/// </summary>
public static class ModTypeLookup<T> where T : class, IModType
{
    private static readonly Dictionary<string, T> dict = new(StringComparer.Ordinal);
    private static readonly Dictionary<string, Dictionary<string, T>> tieredDict = new(StringComparer.Ordinal);

    public static void Register(T instance)
    {
        ArgumentNullException.ThrowIfNull(instance);
        string[] names = new[] { instance.Name }.Concat(instance.GetType()
            .GetCustomAttributes<LegacyNameAttribute>().SelectMany(a => a.Names)).ToArray();
        RegisterNames(instance, names);
    }

    public static void RegisterLegacyNames(T instance, params string[] legacyNames)
    {
        ArgumentNullException.ThrowIfNull(instance);
        ArgumentNullException.ThrowIfNull(legacyNames);
        RegisterNames(instance, (string[])legacyNames.Clone());
    }

    private static void RegisterNames(T instance, string[] names)
    {
        lock (ContentCatalog.Gate)
        {
            if (instance.Mod == null || !ContentCatalog.Contains(instance))
                throw new InvalidOperationException("Register content through BunnyContentMod.AddContent first.");
            string modName = instance.Mod.Name;
            var keys = new HashSet<string>(StringComparer.Ordinal);
            foreach (string name in names)
            {
                string fullName = ContentCatalog.Key(modName, name);
                if (!keys.Add(fullName) || dict.ContainsKey(fullName))
                    throw new InvalidOperationException("Duplicate " + typeof(T).Name + " name: " + fullName);
            }
            if (!tieredDict.TryGetValue(modName, out var subDictionary))
                tieredDict[modName] = subDictionary = new(StringComparer.Ordinal);
            foreach (string name in names)
            {
                string fullName = modName + "/" + name;
                dict.Add(fullName, instance);
                subDictionary.Add(name, instance);
            }
            // Snapshot the key array; user callbacks may mutate Name after registration.
            ContentCatalog.OnRemove(instance, () => {
                foreach (string name in names) { dict.Remove(modName + "/" + name); subDictionary.Remove(name); }
                if (subDictionary.Count == 0) tieredDict.Remove(modName);
            });
        }
    }

    public static T Get(string fullName)
    {
        ContentCatalog.ValidateKey(fullName);
        lock (ContentCatalog.Gate) return dict[fullName];
    }
    public static T Get(string modName, string contentName)
    {
        ContentCatalog.Key(modName, contentName);
        lock (ContentCatalog.Gate) return tieredDict[modName][contentName];
    }
    public static bool TryGetValue(string fullName, [NotNullWhen(true)] out T? value)
    {
        ContentCatalog.ValidateKey(fullName);
        lock (ContentCatalog.Gate) return dict.TryGetValue(fullName, out value);
    }
    public static bool TryGetValue(string modName, string contentName, [NotNullWhen(true)] out T? value)
    {
        ContentCatalog.Key(modName, contentName);
        lock (ContentCatalog.Gate)
        {
            value = null;
            return tieredDict.TryGetValue(modName, out var subDictionary) && subDictionary.TryGetValue(contentName, out value);
        }
    }
}
