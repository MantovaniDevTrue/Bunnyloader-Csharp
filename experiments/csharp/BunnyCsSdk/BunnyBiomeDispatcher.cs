using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text;

namespace BunnyLoader.Source;

public static class BunnyBiomeDispatcher
{
    private const int MaxMods = 16;
    private const int MaxBiomes = 64;

    private sealed class Entry
    {
        public string Name { get; }
        public BunnyBiome Biome { get; }
        public bool Disabled { get; set; }

        public Entry(string name, BunnyBiome biome)
        {
            Name = name;
            Biome = biome;
        }
    }

    private static readonly Dictionary<string, List<Entry>> Mods =
        new(StringComparer.Ordinal);

    public static string LastError { get; private set; } = "";
    public static int ModCount => Mods.Count;

    public static bool Register(string modId, Assembly assembly)
    {
        LastError = "";
        if (!ValidModId(modId) || assembly is null ||
            Mods.ContainsKey(modId) || Mods.Count >= MaxMods)
            return Fail("Invalid or duplicate mod ID");
        try
        {
            Type[] types = assembly.GetTypes()
                .Where(t => t.IsClass && !t.IsAbstract &&
                    !t.ContainsGenericParameters &&
                    typeof(BunnyBiome).IsAssignableFrom(t))
                .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
            if (types.Length == 0 || types.Length > MaxBiomes)
                return Fail("No supported biome classes or limit exceeded");

            var prepared = new List<Entry>(types.Length);
            var used = new HashSet<string>(StringComparer.Ordinal);
            foreach (Type type in types)
            {
                if (!BunnyBiomeSnapshot.ValidIdentifier(type.Name) ||
                    !used.Add(type.Name) ||
                    type.GetConstructor(Type.EmptyTypes) is null ||
                    Activator.CreateInstance(type) is not BunnyBiome biome)
                    return Fail("Invalid BunnyBiome class: " + type.FullName);
                biome.SetStaticDefaults();
                biome.Validate();
                prepared.Add(new Entry(type.Name, biome));
            }
            Mods.Add(modId, prepared);
            return true;
        }
        catch (Exception ex)
        {
            return Fail(ex.GetType().Name + ": " + ex.Message);
        }
    }

    public static bool RegisterFromPath(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(assemblyPath) ||
            assemblyPath.Length > 2048)
            return Fail("Invalid assembly path");
        try { return Register(modId, Assembly.LoadFrom(assemblyPath)); }
        catch (Exception ex) { return Fail(ex.GetType().Name + ": " + ex.Message); }
    }

    public static string TilesWire(string modId)
    {
        if (!Mods.TryGetValue(modId, out List<Entry>? entries)) return "";
        var builder = new StringBuilder("BUNNY_BIOMES_V1\n");
        foreach (Entry entry in entries)
            builder.Append("BIOME\t").Append(entry.Name).Append('\t')
                .Append(string.Join(",", entry.Biome.CountedTiles))
                .Append('\n');
        return builder.ToString();
    }

    public static string EvaluateWire(string modId, bool gameMenu,
        bool playerActive, float positionX, int maxTilesX,
        bool skyHeight, bool overworldHeight, string tileCountsWire)
    {
        if (!Mods.TryGetValue(modId, out List<Entry>? entries) ||
            tileCountsWire is null || tileCountsWire.Length > 4096)
            return "";

        var counts = new Dictionary<string, int>(StringComparer.Ordinal);
        if (tileCountsWire.Length != 0)
        {
            foreach (string pair in tileCountsWire.Split(';'))
            {
                int separator = pair.IndexOf('=');
                if (separator <= 0 || separator == pair.Length - 1 ||
                    pair.IndexOf('=', separator + 1) != -1)
                    return "";
                string name = pair.Substring(0, separator);
                if (!BunnyBiomeSnapshot.ValidIdentifier(name) ||
                    !int.TryParse(pair.AsSpan(separator + 1),
                        NumberStyles.None, CultureInfo.InvariantCulture,
                        out int count) ||
                    count < 0 || count > 1000000 ||
                    !counts.TryAdd(name, count) || counts.Count > 32)
                    return "";
            }
        }

        try
        {
            BunnyBiomeSnapshot snapshot = new(
                gameMenu, playerActive, skyHeight, overworldHeight,
                positionX, maxTilesX, counts);
            var response = new StringBuilder("BUNNY_BIOMES_V1\n");
            foreach (Entry entry in entries)
            {
                if (entry.Disabled) continue;
                if (entry.Biome.CountedTiles.Any(name =>
                    !counts.ContainsKey(name)))
                    continue;
                try
                {
                    if (snapshot.PlayerActive &&
                        entry.Biome.IsBiomeActive(snapshot))
                        response.Append("BIOME\t")
                            .Append(entry.Name).Append('\n');
                }
                catch (Exception ex)
                {
                    entry.Disabled = true;
                    LastError = entry.Name + ".IsBiomeActive: " +
                        ex.GetType().Name + ": " + ex.Message;
                }
            }
            return response.ToString();
        }
        catch (Exception ex)
        {
            LastError = ex.GetType().Name + ": " + ex.Message;
            return "";
        }
    }

    public static bool Unregister(string modId) => Mods.Remove(modId);

    public static void Clear()
    {
        Mods.Clear();
        LastError = "";
    }

    private static bool ValidModId(string? modId) =>
        !string.IsNullOrEmpty(modId) && modId.Length <= 72 &&
        modId.All(c => char.IsAsciiLetterOrDigit(c) ||
            c == '_' || c == '-');

    private static bool Fail(string message)
    {
        LastError = message;
        return false;
    }
}
