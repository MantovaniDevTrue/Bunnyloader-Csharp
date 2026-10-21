namespace BunnyLoader.Source;

public abstract class BunnyGore
{
    public string Texture { get; set; } = "";
    public int SpecialAI { get; set; }
    public bool SafeGore { get; set; }
    public virtual void SetDefaults() { }
}
