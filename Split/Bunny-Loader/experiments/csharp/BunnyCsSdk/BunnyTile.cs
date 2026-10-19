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
    public int AdjacentTile { get; set; } = -1;
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
}
