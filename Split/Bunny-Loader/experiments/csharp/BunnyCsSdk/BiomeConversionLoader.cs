// Adapted from tModLoader 1.4.5, fcafebc0b792b90e110bac7c1fc67cac81cb60ed.
// Copyright 2019 tModLoader Team. MIT; see THIRD_PARTY_NOTICES.md.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// A conversion identity, as in tModLoader. The original category has no per-tile hooks;
/// TileLoader/WallLoader own those hooks. Bunny additionally supports native replacement
/// tables, configured in PostSetupContent, without a Mono callback for each world tile.
/// </summary>
public abstract class ModBiomeConversion : ModType
{
    public int Type { get; internal set; }
    internal readonly Dictionary<int, int> Tiles = new(), Walls = new();
    internal bool Configuring, Published;
    protected sealed override void Register() => Type = BiomeConversionLoader.Register(this);
    public sealed override void SetupContent() => SetStaticDefaults();
    public virtual void PostSetupContent() { }

    /// <summary>Replace active tiles of sourceType. Only valid during PostSetupContent.</summary>
    public void RegisterTileConversion(int sourceType, int targetType) => Add(Tiles, sourceType, targetType);
    /// <summary>Replace existing walls of sourceType. Wall zero (no wall) cannot be a source.</summary>
    public void RegisterWallConversion(int sourceType, int targetType) => Add(Walls, sourceType, targetType);
    private void Add(Dictionary<int, int> table, int source, int target)
    {
        lock (ContentCatalog.Gate)
        {
            if (!Configuring) throw new InvalidOperationException("Register conversion rules in PostSetupContent.");
            if (source < (ReferenceEquals(table, Walls) ? 1 : 0) || source > ushort.MaxValue || target < 0 || target > ushort.MaxValue)
                throw new ArgumentOutOfRangeException(nameof(source));
            if (Tiles.Count + Walls.Count >= 4096) throw new InvalidOperationException("Conversion rule limit exceeded.");
            if (!table.TryAdd(source, target)) throw new InvalidOperationException("Duplicate conversion source: " + source);
        }
    }
    /// <summary>
    /// Queue a conversion on the game thread. Coordinates are TILE coordinates. True means
    /// admitted, not applied: world exit/unload cancels outstanding work. Single-player only.
    /// The bounded radius is 0..8; larger regions must be explicitly split by the caller.
    /// </summary>
    public bool Convert(int i, int j, int size = 4, bool tiles = true, bool walls = true)
    {
        lock (ContentCatalog.Gate)
        {
            if (!Published || Mod == null || !Mod.IsLoaded) return false;
            if (i < 0 || j < 0 || size < 0 || size > 8 || (!tiles && !walls)) return false;
            return BiomeConversionNative.Request(Mod.Name, Type, i, j, size, tiles ? 1 : 0, walls ? 1 : 0) == 1;
        }
    }
}

public static class BiomeConversionLoader
{
    // tModLoader 1.4.5 adds Count=22 to BiomeConversionID (vanilla conversions 0..21).
    // IDs here are sent to the native conversion dispatcher, never used as TileID/WallID.
    public const int VanillaCount = 22;
    private sealed class Storage : Loader<ModBiomeConversion>
    {
        public Storage() => Initialize(BiomeConversionLoader.VanillaCount);
    }
    private static Storage Conversions => LoaderManager.Get<Storage>();
    public static int BiomeConversionCount => Conversions.TotalCount;
    public static ModBiomeConversion? GetBiomeConversion(int type) => Conversions.Get(type);
    public static int Register(ModBiomeConversion conversion)
    {
        int type = Conversions.Register(conversion);
        ContentCatalog.OnRemove(conversion, () => {
            if (conversion.Published && conversion.Mod != null)
            {
                // All content of an owner unloads together. Revoke the native owner BEFORE any
                // stale queued request could change terrain. Other owners/IDs remain untouched.
                BiomeConversionNative.Publish(conversion.Mod.Name, "");
            }
            conversion.Published = false;
            conversion.Tiles.Clear(); conversion.Walls.Clear();
        });
        return type;
    }
    internal static void PostSetupContent(BunnyContentMod mod)
    {
        foreach (var conversion in mod.GetContent<ModBiomeConversion>())
        {
            conversion.Configuring = true;
            try { conversion.PostSetupContent(); }
            finally { conversion.Configuring = false; }
        }
    }
    internal static void Publish(BunnyContentMod mod)
    {
        var conversions = mod.GetContent<ModBiomeConversion>();
        if (conversions.Count == 0) return;
        var wire = new StringBuilder("BUNNY_BIOME_CONVERSION_V1\n");
        foreach (var conversion in conversions)
        {
            wire.Append("C|").Append(conversion.Type.ToString(CultureInfo.InvariantCulture)).Append('\n');
            foreach (var pair in conversion.Tiles.OrderBy(p => p.Key)) Rule('T', conversion.Type, pair);
            foreach (var pair in conversion.Walls.OrderBy(p => p.Key)) Rule('W', conversion.Type, pair);
        }
        void Rule(char kind, int id, KeyValuePair<int, int> pair) => wire.Append(kind).Append('|')
            .Append(id.ToString(CultureInfo.InvariantCulture)).Append('|')
            .Append(pair.Key.ToString(CultureInfo.InvariantCulture)).Append('|')
            .Append(pair.Value.ToString(CultureInfo.InvariantCulture)).Append('\n');
        if (BiomeConversionNative.Publish(mod.Name, wire.ToString()) != 1)
            throw new InvalidOperationException("Native biome conversion registry rejected " + mod.Name);
        foreach (var conversion in conversions) conversion.Published = true;
    }
}

internal static class BiomeConversionNative
{
    [DllImport("bunny", EntryPoint = "bunny_biome_conversion_publish", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Publish([MarshalAs(UnmanagedType.LPUTF8Str)] string owner,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string wire);
    [DllImport("bunny", EntryPoint = "bunny_biome_conversion_request", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Request([MarshalAs(UnmanagedType.LPUTF8Str)] string owner,
        int type, int i, int j, int size, int tiles, int walls);
}
