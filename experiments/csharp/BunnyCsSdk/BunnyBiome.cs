using System;
using System.Collections.Generic;
using System.Linq;

namespace BunnyLoader.Source;

public sealed class BunnyBiomeSnapshot
{
    private readonly IReadOnlyDictionary<string, int> tileCounts;

    public bool GameMenu { get; }
    public bool PlayerActive { get; }
    public bool ZoneSkyHeight { get; }
    public bool ZoneOverworldHeight { get; }
    public float PlayerPositionX { get; }
    public int MaxTilesX { get; }

    public BunnyBiomeSnapshot(bool gameMenu, bool playerActive,
        bool zoneSkyHeight, bool zoneOverworldHeight,
        float playerPositionX, int maxTilesX,
        IReadOnlyDictionary<string, int> tileCounts)
    {
        if (tileCounts is null || tileCounts.Count > 32 ||
            !float.IsFinite(playerPositionX) ||
            maxTilesX < 0 || maxTilesX > 100000)
            throw new ArgumentException("Invalid native biome snapshot");

        var copy = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (var entry in tileCounts)
        {
            if (!ValidIdentifier(entry.Key) || entry.Value < 0 ||
                entry.Value > 1000000 || !copy.TryAdd(entry.Key, entry.Value))
                throw new ArgumentException("Invalid tile counter");
        }

        GameMenu = gameMenu;
        PlayerActive = playerActive && !gameMenu;
        ZoneSkyHeight = zoneSkyHeight;
        ZoneOverworldHeight = zoneOverworldHeight;
        PlayerPositionX = playerPositionX;
        MaxTilesX = maxTilesX;
        this.tileCounts = copy;
    }

    public int TileCount(string tileName) =>
        tileName != null && tileCounts.TryGetValue(tileName, out int count)
            ? count : -1;

    public bool IsCenterSurface =>
        PlayerActive && MaxTilesX > 0 &&
        (ZoneSkyHeight || ZoneOverworldHeight) &&
        Math.Abs(Math.Floor((double)PlayerPositionX / 16d) -
            (double)MaxTilesX / 2d) < (double)MaxTilesX / 6d;

    internal static bool ValidIdentifier(string? name) =>
        !string.IsNullOrEmpty(name) && name.Length <= 72 &&
        name.All(c => char.IsAsciiLetterOrDigit(c) || c == '_');
}

public abstract class BunnyBiome
{
    public virtual IReadOnlyList<string> CountedTiles => Array.Empty<string>();

    public virtual void SetStaticDefaults() { }

    public abstract bool IsBiomeActive(BunnyBiomeSnapshot snapshot);

    public virtual void OnEnter(BunnyBiomeSnapshot snapshot) { }

    public virtual void OnInBiome(BunnyBiomeSnapshot snapshot) { }

    public virtual void OnLeave(BunnyBiomeSnapshot snapshot) { }

    public void Validate()
    {
        if (CountedTiles is null || CountedTiles.Count > 32 ||
            CountedTiles.Any(tile => !BunnyBiomeSnapshot.ValidIdentifier(tile)) ||
            CountedTiles.Distinct(StringComparer.Ordinal).Count() != CountedTiles.Count)
            throw new ArgumentException("Invalid or repeated BunnyBiome tile names");
    }
}
