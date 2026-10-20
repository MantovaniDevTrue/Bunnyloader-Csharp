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
    public int DustType { get; set; } = 7;
    public virtual void SetDefaults() { }
}
