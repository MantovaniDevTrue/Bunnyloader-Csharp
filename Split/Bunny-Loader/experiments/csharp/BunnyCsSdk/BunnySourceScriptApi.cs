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
    public virtual int OnHitNPC(int damage) => 0;
    public virtual int OnKill(int damage) => 0;
    public virtual int OnKillDuration(int damage) => 0;
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

public abstract class BunnyBuff
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int DefenseBonus { get; set; } = 0;
    public abstract void SetDefaults();
}

public abstract class BunnyBuffPotion
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public string Buff { get; set; } = "";
    public int Duration { get; set; } = 600;
    public abstract void SetDefaults();
}

public abstract class BunnyAccessory
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int DefenseBonus { get; set; } = 0;
    public string Tooltip { get; set; } = "";
    public abstract void SetDefaults();
}

public abstract class BunnyPlayer
{
    public string Accessory { get; set; } = "";
    public int DefenseBonus { get; protected set; } = 0;
    public int MoveSpeedPercent { get; protected set; } = 0;
    public abstract void SetDefaults();
    public virtual void ResetEffects()
    {
        DefenseBonus = 0;
        MoveSpeedPercent = 0;
    }
    public abstract void UpdateEquips(bool accessoryEquipped);
    public virtual string OnEnterWorld() => "";
    public virtual string OnRespawn() => "";
    public virtual int UpdateMovement(bool accessoryEquipped) => 0;
    public virtual bool FrameEffects(bool accessoryEquipped) => false;
}
public abstract class BunnyNPC
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Width { get; set; } = 32;
    public int Height { get; set; } = 24;
    public int Damage { get; set; } = 7;
    public int Defense { get; set; } = 2;
    public int LifeMax { get; set; } = 25;
    public int AiStyle { get; set; } = 1;
    public int AnimationType { get; set; } = 1;
    public int Frames { get; set; } = 2;
    public abstract void SetDefaults();
}


public enum PrefixCategory
{
    Melee = 0,
    Ranged = 1,
    Magic = 2,
    Summon = 3,
    AnyWeapon = 4,
    Accessory = 5,
    Custom = 6
}

public struct PrefixStats
{
    public float DamageMultiplier { get; set; }
    public float KnockbackMultiplier { get; set; }
    public float UseTimeMultiplier { get; set; }
    public float ScaleMultiplier { get; set; }
    public float ShootSpeedMultiplier { get; set; }
    public float ManaMultiplier { get; set; }
    public int CriticalBonus { get; set; }
    public int TagDamageBonus { get; set; }
    public int ArmorPenetrationBonus { get; set; }

    public static PrefixStats Identity => new PrefixStats
    {
        DamageMultiplier = 1f,
        KnockbackMultiplier = 1f,
        UseTimeMultiplier = 1f,
        ScaleMultiplier = 1f,
        ShootSpeedMultiplier = 1f,
        ManaMultiplier = 1f
    };
}

public abstract class BunnyPrefix
{
    public string DisplayName { get; set; } = "";
    public PrefixCategory Category { get; set; } = PrefixCategory.Custom;
    public float RollWeight { get; set; } = 1f;

    public abstract void SetDefaults();
    public virtual void SetStats(ref PrefixStats stats) { }
    public virtual void ModifyValue(ref float valueMultiplier) { }
    public virtual void ApplyAccessoryEffects(ref int defenseBonus) { }
}
