using System;

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
    Cut = 32768
}

public enum BunnyTileShape {
    Single = 0,
    Style2x1 = 1
}

public abstract class BunnyTile {
    public string Texture { get; set; } = "";
    public string DisplayName { get; set; } = "";
    public BunnyTileFlags Flags { get; set; }
    public BunnyTileShape Shape { get; set; }
    public int CoordinateHeight { get; set; } = 18;
    public int AnimationFrameHeight { get; set; } = 0;
    public int AnimationFrames { get; set; } = 1;
    public int AnimationFrameTicks { get; set; } = 0;
    public int AdjacentTile { get; set; } = -1;
    public int[] AdjacentTiles { get; set; } = Array.Empty<int>();
    public bool AllowBreaking { get; set; } = true;
    public bool AllowDrops { get; set; } = true;
    public bool CacheDrawData { get; set; } = true;
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
    public virtual bool CanKillTile(int x, int y) => true;
    public virtual bool CanDrop(int x, int y) => true;
    public virtual void HitWire(int x, int y) { }
    public virtual void RandomUpdate(int x, int y) { }
    public virtual bool RightClick(int x, int y) => false;
    public virtual bool PreDraw(int x, int y) => true;
    public virtual void ModifyLight(int x, int y, ref float r, ref float g, ref float b) { }
    public virtual void PlaceInWorld(int x, int y) { }
    public virtual bool Slope(int x, int y) => true;
    public virtual void MouseOver(int x, int y) { }
    public virtual void MouseOverFar(int x, int y) { }
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
                nameof(SetDrawPositions), nameof(AnimateIndividualTile), nameof(SetSpriteEffects)
            };
            int mask = 0;
            for (int i = 0; i < methods.Length; ++i) {
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
