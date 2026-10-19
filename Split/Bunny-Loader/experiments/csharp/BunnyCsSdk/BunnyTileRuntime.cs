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
        public readonly Dictionary<string, int> Masks = new(StringComparer.Ordinal);
    }
    private static readonly Dictionary<string, Mod> Mods = new(StringComparer.Ordinal);
    private static readonly string[] Names = {
        nameof(BunnyTile.CanKillTile), nameof(BunnyTile.CanDrop),
        nameof(BunnyTile.HitWire), nameof(BunnyTile.RandomUpdate),
        nameof(BunnyTile.RightClick), nameof(BunnyTile.PreDraw),
        nameof(BunnyTile.ModifyLight), nameof(BunnyTile.PlaceInWorld),
        nameof(BunnyTile.Slope), nameof(BunnyTile.MouseOver),
        nameof(BunnyTile.MouseOverFar), nameof(BunnyTile.NearbyEffects),
        nameof(BunnyTile.AnimateTile), nameof(BunnyTile.SetDrawPositions),
        nameof(BunnyTile.AnimateIndividualTile), nameof(BunnyTile.SetSpriteEffects)
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
                foreach (var method in matches[0].GetMethods(BindingFlags.Public | BindingFlags.Instance)) {
                    if (Array.IndexOf(Names, method.Name) >= 0 &&
                        method.DeclaringType != typeof(BunnyTile))
                        RuntimeHelpers.PrepareMethod(method.MethodHandle);
                }
                mod.Tiles.Add(owner, tile);
                mod.Masks.Add(owner, tile.TileCallbackMask);
            }
            if (kind == 2) return "READY";
            if (index == 13) {
                if ((mod.Masks[owner] & ((7 << 13) | (1 << 5))) == 0) return "DEFAULT";
            } else if ((mod.Masks[owner] & (1 << index)) == 0) return "DEFAULT";
            string[] fields = snapshot.Split(',');
            int expected = index == 11 ? 3 : index == 13 ? 12 : 2;
            if (fields.Length != expected && !(index == 13 && fields.Length == 11))
                return "ERROR|Stage=DecodeTileSnapshot Reason=ExpectedCoordinates";
            int x = int.Parse(fields[0], NumberStyles.Integer, CultureInfo.InvariantCulture);
            int y = int.Parse(fields[1], NumberStyles.Integer, CultureInfo.InvariantCulture);
            if (x < 0 || y < 0 || x > 100000 || y > 100000)
                return "ERROR|Stage=DecodeTileSnapshot Reason=CoordinatesOutOfBounds";
            if (index == 13) {
                int frameX = int.Parse(fields[2], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int frameY = int.Parse(fields[3], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int width = int.Parse(fields[4], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int height = int.Parse(fields[5], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int top = int.Parse(fields[6], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int addFrameX = int.Parse(fields[7], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int addFrameY = int.Parse(fields[8], NumberStyles.Integer, CultureInfo.InvariantCulture);
                int effects = int.Parse(fields[9], NumberStyles.Integer, CultureInfo.InvariantCulture);
                if (fields[10] != "0" && fields[10] != "1")
                    return "ERROR|Stage=DecodeTileDraw Reason=InvalidSpecialPass";
                if ((mod.Masks[owner] & (1 << 13)) != 0)
                    tile.SetDrawPositions(x, y, ref width, ref top, ref height, ref frameX, ref frameY);
                if ((mod.Masks[owner] & (1 << 14)) != 0) {
                    if (fields.Length == 12) {
                        int tileType = int.Parse(fields[11], NumberStyles.Integer, CultureInfo.InvariantCulture);
                        if (tileType < 754 || tileType > 65535)
                            return "ERROR|Stage=DecodeTileDraw Reason=InvalidTileType";
                        tile.AnimateIndividualTile(tileType, x, y, ref addFrameX, ref addFrameY);
                    } else {
                        tile.AnimateIndividualTile(x, y, ref addFrameX, ref addFrameY);
                    }
                }
                if ((mod.Masks[owner] & (1 << 15)) != 0)
                    tile.SetSpriteEffects(x, y, ref effects);
                bool drawDefault = true;
                bool hasPreDraw = (mod.Masks[owner] & (1 << 5)) != 0 && fields[10] == "0";
                if (hasPreDraw) drawDefault = tile.PreDraw(x, y);
                if (frameX < 0 || frameX > 32767 || frameY < 0 || frameY > 32767 ||
                    width < 1 || width > 512 || height < 1 || height > 512 ||
                    top < -128 || top > 128 || addFrameX < -8192 || addFrameX > 8192 ||
                    addFrameY < -8192 || addFrameY > 8192 ||
                    frameX + addFrameX < 0 || frameY + addFrameY < 0 ||
                    effects < 0 || effects > 3)
                    return "ERROR|Stage=ValidateTileDraw Reason=InvalidDrawData";
                return string.Join("|", "DRAW",
                    frameX.ToString(CultureInfo.InvariantCulture),
                    frameY.ToString(CultureInfo.InvariantCulture),
                    width.ToString(CultureInfo.InvariantCulture),
                    height.ToString(CultureInfo.InvariantCulture),
                    top.ToString(CultureInfo.InvariantCulture),
                    addFrameX.ToString(CultureInfo.InvariantCulture),
                    addFrameY.ToString(CultureInfo.InvariantCulture),
                    effects.ToString(CultureInfo.InvariantCulture)) +
                    (hasPreDraw ? (drawDefault ? "|1" : "|0") : "");
            }
            switch (index) {
                case 0: return tile.CanKillTile(x, y) ? "1" : "0";
                case 1: return tile.CanDrop(x, y) ? "1" : "0";
                case 2: tile.HitWire(x, y); return "1";
                case 3: tile.RandomUpdate(x, y); return "1";
                case 4: return tile.RightClick(x, y) ? "1" : "0";
                case 5: return tile.PreDraw(x, y) ? "1" : "0";
                case 6:
                    float red=0f,green=0f,blue=0f;
                    tile.ModifyLight(x,y,ref red,ref green,ref blue);
                    if(!float.IsFinite(red)||!float.IsFinite(green)||!float.IsFinite(blue)||
                       red<0f||green<0f||blue<0f||red>4f||green>4f||blue>4f)
                        return "ERROR|Stage=ValidateTileLight Reason=NonFiniteOrOutsideRange";
                    return "LIGHT|"+((int)MathF.Round(red*1000f)).ToString(CultureInfo.InvariantCulture)+"|"+
                        ((int)MathF.Round(green*1000f)).ToString(CultureInfo.InvariantCulture)+"|"+
                        ((int)MathF.Round(blue*1000f)).ToString(CultureInfo.InvariantCulture);
                case 7: tile.PlaceInWorld(x, y); return "1";
                case 8: return tile.Slope(x, y) ? "1" : "0";
                case 9: tile.MouseOver(x, y); return "1";
                case 10: tile.MouseOverFar(x, y); return "1";
                case 11:
                    if(fields[2]!="0"&&fields[2]!="1")
                        return "ERROR|Stage=DecodeTileSnapshot Reason=InvalidNearbyCloser";
                    tile.NearbyEffects(x,y,fields[2]=="1"); return "1";
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
            string detail=error.ToString();
            if(detail.Length>3000)detail=detail.Substring(0,3000);
            return "ERROR|[CSharp][Tile] Mod=" + modId + " Class=" + owner + " Assembly=" + assemblyPath +
                " Callback=" + (index >= 0 && index < Names.Length ? Names[index] : "<invalid>") +
                " Stage=Invoke Result=CallbackRejected Reason=" + error.GetType().Name + " Exception=" + detail;
        }
    }
}
