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
        public readonly Dictionary<string, BunnyTileDrawContext> DrawContexts = new(StringComparer.Ordinal);
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
        nameof(BunnyTile.AnimateIndividualTile), nameof(BunnyTile.SetSpriteEffects),
        nameof(BunnyTile.PostDraw), nameof(BunnyTile.DrawEffects),
        nameof(BunnyTile.SpecialDraw), nameof(BunnyTile.EmitParticles),
        nameof(BunnyTile.HasSmartInteract), nameof(BunnyTile.TileFrame),
        nameof(BunnyTile.KillTile), nameof(BunnyTile.KillMultiTile),
        nameof(BunnyTile.NumDust), nameof(BunnyTile.CreateDust),
        nameof(BunnyTile.KillSound), nameof(BunnyTile.GetItemDrops)
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
                if ((mod.Masks[owner] & ((7 << 13) | (1 << 5) | (15 << 16))) == 0) return "DEFAULT";
            } else if ((mod.Masks[owner] & (1 << index)) == 0) return "DEFAULT";
            string[] fields = snapshot.Split(',');
            int expected = index == 11 || index == 25 || index == 26 ? 3 : index == 13 ? 12 :
                index == 21 || index == 23 || index == 24 ? 4 : index == 22 ? 5 : 2;
            if (fields.Length != (index == 20 ? 6 : expected) && !(index == 13 && (fields.Length == 11 || fields.Length == 13)))
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
                int sourceFrameX = frameX, sourceFrameY = frameY;
                int tileType = fields.Length >= 12
                    ? int.Parse(fields[11], NumberStyles.Integer, CultureInfo.InvariantCulture) : -1;
                int animationFrame = fields.Length == 13
                    ? int.Parse(fields[12], NumberStyles.Integer, CultureInfo.InvariantCulture) : -1;
                if (animationFrame < -1 || animationFrame > 127)
                    return "ERROR|Stage=DecodeTileDraw Reason=InvalidAnimationFrame";
                if (tileType != -1 && (tileType < 754 || tileType > 65535))
                    return "ERROR|Stage=DecodeTileDraw Reason=InvalidTileType";
                if ((mod.Masks[owner] & (1 << 13)) != 0)
                    tile.SetDrawPositions(x, y, ref width, ref top, ref height, ref frameX, ref frameY);
                if ((mod.Masks[owner] & (1 << 14)) != 0) {
                    if (fields.Length >= 12) {
                        tile.AnimateIndividualTile(tileType, x, y, ref addFrameX, ref addFrameY);
                    } else {
                        tile.AnimateIndividualTile(x, y, ref addFrameX, ref addFrameY);
                    }
                }
                if ((mod.Masks[owner] & (1 << 15)) != 0)
                    tile.SetSpriteEffects(x, y, ref effects);
                bool drawDefault = true;
                bool hasPreDraw = (mod.Masks[owner] & (1 << 5)) != 0 && fields[10] == "0";
                bool hasPostDraw = (mod.Masks[owner] & (1 << 16)) != 0 && fields[10] == "0";
                bool hasDrawEffects = (mod.Masks[owner] & (1 << 17)) != 0 && fields[10] == "0";
                bool hasSpecialDraw = (mod.Masks[owner] & (1 << 18)) != 0 && fields[10] == "0";
                bool hasParticles = (mod.Masks[owner] & (1 << 19)) != 0 && fields[10] == "0";
                BunnyTileDrawContext? context = null;
                if (hasPreDraw || hasPostDraw || hasDrawEffects || hasSpecialDraw || hasParticles) {
                    if (tileType >= 754) {
                        if (!mod.DrawContexts.TryGetValue(owner, out context)) {
                            context = new BunnyTileDrawContext(tileType, x, y);
                            mod.DrawContexts.Add(owner, context);
                        }
                        context.Reset(tileType, x, y, animationFrame);
                        if (hasPreDraw) drawDefault = tile.PreDraw(x, y, context);
                        if (hasDrawEffects) tile.DrawEffects(x, y, sourceFrameX, sourceFrameY, context);
                        if (hasParticles) tile.EmitParticles(x, y, sourceFrameX, sourceFrameY, context);
                        if (hasPostDraw) tile.PostDraw(x, y, context);
                        if (hasSpecialDraw && context.SpecialRequested) {
                            context.BeginSpecialDraw();
                            tile.SpecialDraw(x, y, context);
                        }
                    } else {
                        if (hasPostDraw || hasDrawEffects || hasSpecialDraw || hasParticles)
                            return "ERROR|Stage=DecodeTileDraw Reason=VisualCallbacksRequireTileType";
                        if (hasPreDraw) drawDefault = tile.PreDraw(x, y);
                    }
                }
                if (frameX < 0 || frameX > 32767 || frameY < 0 || frameY > 32767 ||
                    width < 1 || width > 512 || height < 1 || height > 512 ||
                    top < -128 || top > 128 || addFrameX < -8192 || addFrameX > 8192 ||
                    addFrameY < -8192 || addFrameY > 8192 ||
                    frameX + addFrameX < 0 || frameY + addFrameY < 0 ||
                    effects < 0 || effects > 3)
                    return "ERROR|Stage=ValidateTileDraw Reason=InvalidDrawData";
                string drawWire = string.Join("|", "DRAW",
                    frameX.ToString(CultureInfo.InvariantCulture),
                    frameY.ToString(CultureInfo.InvariantCulture),
                    width.ToString(CultureInfo.InvariantCulture),
                    height.ToString(CultureInfo.InvariantCulture),
                    top.ToString(CultureInfo.InvariantCulture),
                    addFrameX.ToString(CultureInfo.InvariantCulture),
                    addFrameY.ToString(CultureInfo.InvariantCulture),
                    effects.ToString(CultureInfo.InvariantCulture)) +
                    (hasPreDraw ? (drawDefault ? "|1" : "|0") : "");
                if (context != null && context.Count > 0)
                    drawWire += "\n" + context.Encode();
                if (context != null && context.ParticleCount > 0)
                    drawWire += "\n" + context.EncodeParticles();
                return drawWire;
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
                case 27:
                    var drops=tile.GetItemDrops(x,y);
                    if(drops==null)return "DEFAULT";
                    if(drops.Length>2)return "ERROR|Stage=ValidateTileDrops Reason=TooMany";
                    var wire="DROPS|"+drops.Length.ToString(CultureInfo.InvariantCulture);
                    for(int i=0;i<drops.Length;i++) {
                        var d=drops[i];
                        if(d.ItemType<1||d.ItemType>65535||d.Stack<1||d.Stack>9999)
                            return "ERROR|Stage=ValidateTileDrops Reason=InvalidEntry";
                        wire+="|"+d.ItemType.ToString(CultureInfo.InvariantCulture)+","+
                              d.Stack.ToString(CultureInfo.InvariantCulture);
                    }
                    return wire;
                case 26:
                    if(fields[2]!="0"&&fields[2]!="1")
                        return "ERROR|Stage=DecodeTileKillSound Reason=InvalidFail";
                    return tile.KillSound(x,y,fields[2]=="1") ? "1" : "0";
                case 24:
                    if(fields[2]!="0"&&fields[2]!="1")
                        return "ERROR|Stage=DecodeTileDustCount Reason=InvalidFail";
                    int dustCount=int.Parse(fields[3],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    if(dustCount<0||dustCount>20)
                        return "ERROR|Stage=DecodeTileDustCount Reason=CountOutOfBounds";
                    tile.NumDust(x,y,fields[2]=="1",ref dustCount);
                    if(dustCount<0||dustCount>20)
                        return "ERROR|Stage=ValidateTileDustCount Reason=CountOutOfBounds";
                    return "DUST|"+dustCount.ToString(CultureInfo.InvariantCulture);
                case 25:
                    int dustType=int.Parse(fields[2],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    if(dustType< -1||dustType>1999)
                        return "ERROR|Stage=DecodeTileDust Reason=InvalidDustType";
                    bool create=tile.CreateDust(x,y,ref dustType);
                    if(dustType< -1||dustType>1999)
                        return "ERROR|Stage=ValidateTileDust Reason=InvalidDustType";
                    return "CREATE|"+(create?"1":"0")+"|"+dustType.ToString(CultureInfo.InvariantCulture);
                case 22:
                    if ((fields[2] != "0" && fields[2] != "1") ||
                        (fields[3] != "0" && fields[3] != "1") ||
                        (fields[4] != "0" && fields[4] != "1"))
                        return "ERROR|Stage=DecodeTileKill Reason=InvalidBooleans";
                    bool fail=fields[2]=="1",effectOnly=fields[3]=="1",noItem=fields[4]=="1";
                    tile.KillTile(x,y,ref fail,ref effectOnly,ref noItem);
                    return "KILL|"+(fail?"1":"0")+"|"+(effectOnly?"1":"0")+"|"+(noItem?"1":"0");
                case 23:
                    int frameX=int.Parse(fields[2],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    int frameY=int.Parse(fields[3],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    if(frameX<0||frameY<0||frameX>32767||frameY>32767)
                        return "ERROR|Stage=DecodeTileMultiKill Reason=InvalidFrames";
                    tile.KillMultiTile(x,y,frameX,frameY);
                    return "1";
                case 21:
                    if ((fields[2] != "0" && fields[2] != "1") ||
                        (fields[3] != "0" && fields[3] != "1"))
                        return "ERROR|Stage=DecodeTileFrame Reason=InvalidBooleans";
                    bool resetFrame=fields[2]=="1",noBreak=fields[3]=="1";
                    bool runOriginal=tile.TileFrame(x,y,ref resetFrame,ref noBreak);
                    return "FRAME|"+(runOriginal?"1":"0")+"|"+(resetFrame?"1":"0")+"|"+(noBreak?"1":"0");
                case 20:
                    int lx=int.Parse(fields[2],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    int ly=int.Parse(fields[3],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    int hx=int.Parse(fields[4],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    int hy=int.Parse(fields[5],NumberStyles.Integer,CultureInfo.InvariantCulture);
                    return tile.HasSmartInteract(x,y,new BunnyTileSmartInteractSettings(lx,ly,hx,hy)) ? "1" : "0";
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
