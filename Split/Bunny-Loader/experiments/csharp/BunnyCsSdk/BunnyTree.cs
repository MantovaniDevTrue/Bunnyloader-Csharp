using System;

namespace BunnyLoader.Source;

public abstract class BunnyTree
{
    public string Texture { get; set; } = "";
    public string GrowsOnTile { get; set; } = "";
    public string SaplingTile { get; set; } = "";
    public string WoodItem { get; set; } = "";
    public bool CanDropAcorn { get; set; } = true;
    public int LeafGoreType { get; set; } = -1;
    public string LeafGoreName { get; set; } = "";
    public int DustType { get; set; } = 7;
    public string ShakeItem { get; set; } = "";
    public bool ShakeLeaves { get; set; } = true;
    public virtual void SetDefaults() { }
}
