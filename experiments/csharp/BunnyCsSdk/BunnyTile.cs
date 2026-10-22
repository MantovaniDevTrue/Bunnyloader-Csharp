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
    Solid = 512,
    BlockLight = 1024,
    MergeDirt = 2048,
    ChecksForMerge = 4096
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
