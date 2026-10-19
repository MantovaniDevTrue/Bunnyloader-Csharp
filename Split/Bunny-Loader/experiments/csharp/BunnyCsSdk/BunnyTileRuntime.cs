using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace BunnyLoader.Source;

public static class BunnyTileRuntime {
    private sealed class Mod {
        public string Path = "";
        public Assembly Assembly = null!;
        public readonly Dictionary<string, BunnyTile> Tiles = new(StringComparer.Ordinal);
    }
    private static readonly Dictionary<string, Mod> Mods = new(StringComparer.Ordinal);
    private static readonly string[] Names = {
        nameof(BunnyTile.CanKillTile), nameof(BunnyTile.CanDrop),
        nameof(BunnyTile.HitWire), nameof(BunnyTile.RandomUpdate),
        nameof(BunnyTile.RightClick), nameof(BunnyTile.PreDraw),
        nameof(BunnyTile.ModifyLight), nameof(BunnyTile.PlaceInWorld),
        nameof(BunnyTile.Slope), nameof(BunnyTile.MouseOver),
        nameof(BunnyTile.MouseOverFar), nameof(BunnyTile.NearbyEffects),
        nameof(BunnyTile.AnimateTile)
    };
    public static string Dispatch(string modId, string assemblyPath, string owner, int index, int kind, string snapshot) {
        try {
            if (string.IsNullOrEmpty(modId) || string.IsNullOrEmpty(owner) ||
                index < 0 || index >= Names.Length || kind < 0 || kind > 2 ||
                snapshot.Length > 512 || (kind == 2 && snapshot.Length != 0))
                return "ERROR|Stage=ValidateTileCallback Reason=InvalidTransport";
            if (!Mods.TryGetValue(modId, out var mod) || mod.Path != assemblyPath) {
                mod = new Mod { Path = assemblyPath, Assembly = Assembly.Load(File.ReadAllBytes(assemblyPath)) };
                Mods[modId] = mod;
            }
            if (!mod.Tiles.TryGetValue(owner, out var tile)) {
                Type[] matches = mod.Assembly.GetTypes().Where(t =>
                    t.Name == owner && !t.IsAbstract && !t.ContainsGenericParameters &&
                    typeof(BunnyTile).IsAssignableFrom(t)).ToArray();
                if (matches.Length != 1) return "ERROR|Stage=BindTile Reason=AmbiguousOrMissingOwner";
                tile = (BunnyTile)Activator.CreateInstance(matches[0])!;
                var declaration = matches[0].GetCustomAttribute<BunnyContentAttribute>(false);
                if (declaration != null) {
                    matches[0].GetProperty("DisplayName")!.SetValue(tile, declaration.DisplayName);
                    matches[0].GetProperty("Texture")!.SetValue(tile, declaration.Texture);
                }
                tile.SetStaticDefaults();
                foreach (string name in Names) {
                    var method = matches[0].GetMethod(name, BindingFlags.Public | BindingFlags.Instance)!;
                    if (method.DeclaringType != typeof(BunnyTile))
                        RuntimeHelpers.PrepareMethod(method.MethodHandle);
                }
                mod.Tiles.Add(owner, tile);
            }
            if (kind == 2) return "READY";
            if ((tile.TileCallbackMask & (1 << index)) == 0) return "DEFAULT";
            string[] fields = snapshot.Split(',');
            if (fields.Length != 2) return "ERROR|Stage=DecodeTileSnapshot Reason=ExpectedCoordinates";
            int x = int.Parse(fields[0], NumberStyles.Integer, CultureInfo.InvariantCulture);
            int y = int.Parse(fields[1], NumberStyles.Integer, CultureInfo.InvariantCulture);
            if (x < 0 || y < 0 || x > 100000 || y > 100000)
                return "ERROR|Stage=DecodeTileSnapshot Reason=CoordinatesOutOfBounds";
            switch (index) {
                case 0: return tile.CanKillTile(x, y) ? "1" : "0";
                case 1: return tile.CanDrop(x, y) ? "1" : "0";
                case 2: tile.HitWire(x, y); return "1";
                case 3: tile.RandomUpdate(x, y); return "1";
                case 4: return tile.RightClick(x, y) ? "1" : "0";
                case 5: return tile.PreDraw(x, y) ? "1" : "0";
                case 7: tile.PlaceInWorld(x, y); return "1";
                case 8: return tile.Slope(x, y) ? "1" : "0";
                case 9: tile.MouseOver(x, y); return "1";
                case 10: tile.MouseOverFar(x, y); return "1";
                case 11: tile.NearbyEffects(x, y, false); return "1";
                case 12:
                    int frame = x, counter = y;
                    tile.AnimateTile(ref frame, ref counter);
                    if (frame < 0 || frame > 127 || counter < 0 || counter > 600)
                        return "ERROR|Stage=ValidateTileAnimation Reason=FrameOrCounterOutOfBounds";
                    return frame.ToString(CultureInfo.InvariantCulture) + "," +
                        counter.ToString(CultureInfo.InvariantCulture);
                default: return "ERROR|Stage=TileCallback Reason=ContextRequired";
            }
        } catch (Exception error) {
            return "ERROR|[CSharp][Tile] Mod=" + modId + " Class=" + owner +
                " Callback=" + (index >= 0 && index < Names.Length ? Names[index] : "<invalid>") +
                " Reason=" + error.GetType().Name + " Message=" + error.Message;
        }
    }
}
