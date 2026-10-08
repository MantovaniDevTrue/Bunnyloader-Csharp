namespace BunnyLoader.Source;

public abstract class BunnyProjectile
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Width { get; set; } = 8;
    public int Height { get; set; } = 8;
    public int AiStyle { get; set; } = 1;
    public bool Friendly { get; set; } = true;
    public int Penetrate { get; set; } = 1;
    public int TimeLeft { get; set; } = 360;
    public abstract void SetDefaults();
}

public abstract class BunnyWeapon
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Damage { get; set; } = 20;
    public int UseTime { get; set; } = 18;
    public float ShootSpeed { get; set; } = 12f;
    public string Shoot { get; set; } = "";
    public abstract void SetDefaults();
}
