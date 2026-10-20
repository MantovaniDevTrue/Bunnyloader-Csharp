using System;
using System.Collections.Generic;
using System.Text;

namespace BunnyLoader.Source;

[Flags]
public enum BunnyTileFlags {
    None = 0,
    Table = 1,
    SolidTop = 2,
    NoAttach = 4,
    LavaDeath = 8,
    FrameImportant = 16,
    DisableSmartCursor = 32,
    HasOutlines = 64,
    IgnoredByNpcStepUp = 128,
    CountsAsTable = 256,
    /// <summary>Colisão sólida do terreno, calculada pelo Terraria.</summary>
    Solid = 512,
    /// <summary>O bloco bloqueia luz; não é a emissão de luz.</summary>
    BlockLight = 1024,
    /// <summary>Mesclagem dos frames com terra.</summary>
    MergeDirt = 2048,
    /// <summary>Habilita o processamento de iluminação; não define uma cor.</summary>
    Lighted = 4096,
    WaterDeath = 8192,
    /// <summary>Inclui o tile na verificação nativa de junção com vizinhos.</summary>
    ChecksForMerge = 16384,
    Cut = 32768,
    Campfire = 65536
}

public enum BunnyTileShape {
    Single = 0,
    Style2x1 = 1,
    Style3x2 = 2,
    Style1x3 = 3
}

public enum BunnyTileFrameToggle {
    None = 0,
    CampfireVertical = 1,
    LampHorizontal = 2
}

public readonly struct BunnyTileDrop {
    public int ItemType { get; }
    public int Stack { get; }
    public BunnyTileDrop(int itemType,int stack=1) {
        if(itemType<1||itemType>65535||stack<1||stack>9999)
            throw new ArgumentOutOfRangeException(nameof(itemType));
        ItemType=itemType;Stack=stack;
    }
}

public readonly struct BunnyTileSmartInteractSettings {
    public int LX { get; }
    public int LY { get; }
    public int HX { get; }
    public int HY { get; }
    public BunnyTileSmartInteractSettings(int lx,int ly,int hx,int hy) {
        if(hx<lx||hy<ly||hx-(long)lx>=64||hy-(long)ly>=64)
            throw new ArgumentOutOfRangeException(nameof(hx));
        LX=lx;LY=ly;HX=hx;HY=hy;
    }
}

public abstract class BunnyTile {
    public string Texture { get; set; } = "";
    public string DisplayName { get; set; } = "";
    public BunnyTileFlags Flags { get; set; }
    public BunnyTileShape Shape { get; set; }
    public BunnyTileFrameToggle FrameToggle { get; set; }
    public int CoordinateHeight { get; set; } = 18;
    public int AnimationFrameHeight { get; set; } = 0;
    public int AnimationFrames { get; set; } = 1;
    public int AnimationFrameTicks { get; set; } = 0;
    public int AdjacentTile { get; set; } = -1;
    public int[] AdjacentTiles { get; set; } = Array.Empty<int>();
    public bool AllowBreaking { get; set; } = true;
    public bool AllowDrops { get; set; } = true;
    public bool CacheDrawData { get; set; } = true;
    public int MinPick { get; set; }
    public float MineResist { get; set; } = 1f;
    private readonly SortedDictionary<int, string> _itemDrops = new();
    public void RegisterItemDrop(string itemName, params int[] styles) {
        if (string.IsNullOrEmpty(itemName) || itemName.Length > 72)
            throw new ArgumentException("Invalid item name", nameof(itemName));
        foreach (char c in itemName)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_'))
                throw new ArgumentException("Invalid item name", nameof(itemName));
        AddItemDropRules(itemName, styles);
    }
    public void RegisterItemDrop(int itemType, params int[] styles) {
        if (itemType < 1 || itemType > 65535)
            throw new ArgumentOutOfRangeException(nameof(itemType));
        AddItemDropRules("#" + itemType.ToString(System.Globalization.CultureInfo.InvariantCulture), styles);
    }
    private void AddItemDropRules(string item, int[] styles) {
        if (styles == null) throw new ArgumentNullException(nameof(styles));
        if (styles.Length == 0) {
            _itemDrops[-1] = item;
        } else {
            foreach (int style in styles) {
                if (style < 0 || style > 4095)
                    throw new ArgumentOutOfRangeException(nameof(styles));
                _itemDrops[style] = item;
            }
        }
        if (_itemDrops.Count > 16)
            throw new InvalidOperationException("Maximum 16 registered item drop styles");
    }
    public string ItemDropDefinitions {
        get {
            if (_itemDrops.Count == 0) return "-";
            var text = new StringBuilder();
            foreach (var entry in _itemDrops) {
                if (text.Length > 0) text.Append(';');
                text.Append(entry.Key).Append('~').Append(entry.Value);
            }
            return text.ToString();
        }
    }
    public string DustName { get; set; } = "";
    public int MapR { get; private set; } = 200;
    public int MapG { get; private set; } = 200;
    public int MapB { get; private set; } = 200;
    public string MapName { get; private set; } = "";
    public void AddMapEntry(int red, int green, int blue, string name) {
        if ((uint)red > 255 || (uint)green > 255 || (uint)blue > 255)
            throw new ArgumentOutOfRangeException(nameof(red));
        if (string.IsNullOrWhiteSpace(name) || name.Length > 80 ||
            name.IndexOfAny(new[] { '\t', '\n', '\r', '|' }) >= 0)
            throw new ArgumentException("Invalid tile map name", nameof(name));
        MapR = red;
        MapG = green;
        MapB = blue;
        MapName = name;
    }
    public abstract void SetStaticDefaults();
    public virtual int NumDust(bool fail) => fail ? 1 : 3;
    public virtual void NumDust(int x,int y,bool fail,ref int num) { }
    public virtual bool CreateDust(int x,int y,ref int type) => true;
    public virtual bool KillSound(int x,int y,bool fail) => true;
    public virtual BunnyTileDrop[]? GetItemDrops(int x,int y) => null;
    public virtual bool CanKillTile(int x, int y) => true;
    public virtual bool CanDrop(int x, int y) => true;
    public virtual void HitWire(int x, int y) { }
    public virtual void RandomUpdate(int x, int y) { }
    public virtual bool RightClick(int x, int y) => false;
    public virtual bool PreDraw(int x, int y) => true;
    public virtual bool PreDraw(int x, int y, BunnyTileDrawContext draw)
        => PreDraw(x, y);
    public virtual void PostDraw(int x, int y, BunnyTileDrawContext draw) { }
    public virtual void DrawEffects(int x, int y, int tileFrameX, int tileFrameY, BunnyTileDrawContext draw) { }
    public virtual void SpecialDraw(int x, int y, BunnyTileDrawContext draw) { }
    public virtual void EmitParticles(int x, int y, int tileFrameX, int tileFrameY, BunnyTileDrawContext draw) { }
    public virtual void ModifyLight(int x, int y, ref float r, ref float g, ref float b) { }
    public virtual void PlaceInWorld(int x, int y) { }
    public virtual bool Slope(int x, int y) => true;
    public virtual void MouseOver(int x, int y) { }
    public virtual void MouseOverFar(int x, int y) { }
    public virtual bool HasSmartInteract(int x,int y,BunnyTileSmartInteractSettings settings) => false;
    public virtual bool TileFrame(int x,int y,ref bool resetFrame,ref bool noBreak) => true;
    public virtual void KillTile(int x,int y,ref bool fail,ref bool effectOnly,ref bool noItem) { }
    public virtual void KillMultiTile(int x,int y,int frameX,int frameY) { }
    public virtual void NearbyEffects(int x, int y, bool closer) { }
    public virtual void AnimateTile(ref int frame, ref int frameCounter) { }
    public virtual void SetDrawPositions(int x, int y, ref int width, ref int top,
        ref int height, ref int frameX, ref int frameY) { }
    public virtual void AnimateIndividualTile(int x, int y, ref int addFrameX, ref int addFrameY) { }
    public virtual void AnimateIndividualTile(int type, int x, int y, ref int addFrameX, ref int addFrameY)
        => AnimateIndividualTile(x, y, ref addFrameX, ref addFrameY);
    public virtual void SetSpriteEffects(int x, int y, ref int effects) { }
    public int TileCallbackMask {
        get {
            var type = GetType();
            string[] methods = {
                nameof(CanKillTile), nameof(CanDrop), nameof(HitWire), nameof(RandomUpdate),
                nameof(RightClick), nameof(PreDraw), nameof(ModifyLight), nameof(PlaceInWorld),
                nameof(Slope), nameof(MouseOver), nameof(MouseOverFar), nameof(NearbyEffects), nameof(AnimateTile),
                nameof(SetDrawPositions), nameof(AnimateIndividualTile), nameof(SetSpriteEffects),
                nameof(PostDraw), nameof(DrawEffects), nameof(SpecialDraw), nameof(EmitParticles),
                nameof(HasSmartInteract), nameof(TileFrame), nameof(KillTile), nameof(KillMultiTile),
                nameof(NumDust), nameof(CreateDust), nameof(KillSound), nameof(GetItemDrops)
            };
            int mask = 0;
            for (int i = 0; i < methods.Length; ++i) {
                if (i == 5) {
                    var legacyDraw = type.GetMethod(methods[i], new[] {typeof(int), typeof(int)});
                    var contextDraw = type.GetMethod(methods[i], new[] {typeof(int), typeof(int), typeof(BunnyTileDrawContext)});
                    if (legacyDraw == null || contextDraw == null ||
                        legacyDraw.GetBaseDefinition().DeclaringType != typeof(BunnyTile) ||
                        contextDraw.GetBaseDefinition().DeclaringType != typeof(BunnyTile))
                        throw new InvalidOperationException(type.FullName + ": invalid callback " + methods[i]);
                    if (legacyDraw.DeclaringType != typeof(BunnyTile) ||
                        contextDraw.DeclaringType != typeof(BunnyTile)) mask |= 1 << i;
                    continue;
                }
                if(i==24) {
                    var handler=type.GetMethod(nameof(NumDust),new[]{typeof(int),typeof(int),
                        typeof(bool),typeof(int).MakeByRefType()});
                    if(handler==null||handler.GetBaseDefinition().DeclaringType!=typeof(BunnyTile))
                        throw new InvalidOperationException(type.FullName+": invalid callback NumDust");
                    if(handler.DeclaringType!=typeof(BunnyTile))mask|=1<<i;
                    continue;
                }
                if (i == 14) {
                    Type integer = typeof(int);
                    Type byRef = integer.MakeByRefType();
                    var legacy = type.GetMethod(methods[i], new[] {integer, integer, byRef, byRef});
                    var original = type.GetMethod(methods[i], new[] {integer, integer, integer, byRef, byRef});
                    if (legacy == null || original == null ||
                        legacy.GetBaseDefinition().DeclaringType != typeof(BunnyTile) ||
                        original.GetBaseDefinition().DeclaringType != typeof(BunnyTile))
                        throw new InvalidOperationException(type.FullName + ": invalid callback " + methods[i]);
                    if (legacy.DeclaringType != typeof(BunnyTile) ||
                        original.DeclaringType != typeof(BunnyTile)) mask |= 1 << i;
                    continue;
                }
                var callback = type.GetMethod(methods[i], System.Reflection.BindingFlags.Public |
                    System.Reflection.BindingFlags.Instance);
                if (callback == null || callback.GetBaseDefinition().DeclaringType != typeof(BunnyTile))
                    throw new InvalidOperationException(type.FullName + ": invalid callback " + methods[i]);
                if (callback.DeclaringType != typeof(BunnyTile)) mask |= 1 << i;
            }
            return mask;
        }
    }
}
