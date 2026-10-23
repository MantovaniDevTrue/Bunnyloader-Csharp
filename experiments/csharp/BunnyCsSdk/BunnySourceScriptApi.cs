namespace BunnyLoader.Source;

public struct BunnyNpcAiState
{
    public int Slot;
    public int Type;
    public int Generation;
    public int Life;
    public int LifeMax;
    public int Age;
    public float X;
    public float Y;
    public float VelocityX;
    public float VelocityY;
    public float PlayerX;
    public float PlayerY;
}

public struct BunnyProjectileAiState
{
    public int Slot;
    public int Type;
    public int Generation;
    public int Owner;
    public int Age;
    public float X;
    public float Y;
    public float VelocityX;
    public float VelocityY;
    public float PlayerX;
    public float PlayerY;
    public int TargetNpc;
    public float TargetX;
    public float TargetY;
    public bool HasTarget => TargetNpc >= 0;
}

public enum BunnyProjectileTileCollisionMode
{
    Vanilla = 0,
    KeepAlive = 1
}

public abstract class BunnyProjectile
{
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Width { get; set; } = 8;
    public int Height { get; set; } = 8;
    public int AiStyle { get; set; } = 1;
    public int Frames { get; set; } = 1;
    /// <summary>Game update ticks per sprite frame; native animation, no Mono call per frame.</summary>
    public int AnimationFrameTicks { get; set; } = 6;
    /// <summary>Native nearest-hostile search radius, in pixels, 1..4000; only used by minions.</summary>
    public float MinionTargetRange { get; set; } = 700f;
    /// <summary>Native updates between target scans, 1..120. Cached targets are refreshed at this cadence.</summary>
    public int MinionScanTicks { get; set; } = 8;
    public bool Friendly { get; set; } = true;
    public bool Minion { get; set; }
    public bool MinionContactDamage { get; set; }
    /// <summary>Optional native per-update follow/attack controller. Null keeps custom MonoVM AI; lifecycle remains native.</summary>
    public BunnyMinionMovement? MinionMovement { get; set; }
    public float MinionSlots { get; set; }
    public bool TileCollide { get; set; } = true;
    public BunnyProjectileTileCollisionMode TileCollisionMode { get; set; } = BunnyProjectileTileCollisionMode.Vanilla;
    public bool IgnoreWater { get; set; } = true;
    public int Penetrate { get; set; } = 1;
    public int TimeLeft { get; set; } = 360;
    public abstract void SetDefaults();
    public virtual void OnSpawn(ref BunnyProjectileAiState state) { }
    public virtual bool AI(ref BunnyProjectileAiState state) => false;
    public virtual void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage) { }
    public virtual void OnKill(ref BunnyProjectileAiState state) { }
    public virtual void OnDespawn(ref BunnyProjectileAiState state) { }
    public virtual int OnHitNPC(int damage) => 0;
    public virtual int OnKill(int damage) => 0;
    public virtual int OnKillDuration(int damage) => 0;
}

public abstract class BunnyWeapon : BunnyRecipeSource
{
    public BunnyHoldout? Holdout { get; set; }
    /// <summary>Som próprio do mod; tem precedência sobre o SoundID vanilla.</summary>
    public BunnySoundStyle? CustomUseSound { get; set; }
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Damage { get; set; } = 20;
    public int UseTime { get; set; } = 18;
    public float ShootSpeed { get; set; } = 12f;
    public string Shoot { get; set; } = "";
    public bool Summon { get; set; }
    public string MinionBuff { get; set; } = "";
    public int MinionBuffTime { get; set; } = 2;
    public abstract void SetDefaults();
}

public abstract class BunnyBuff
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int DefenseBonus { get; set; } = 0;
    /// <summary>Defense percentage after equipment bonuses; 100 preserves the defense.</summary>
    public int DefenseMultiplierPercent { get; set; } = 100;
    /// <summary>Texto da dica. "{0}" vira DefenseBonus (equivale ao ModifyDescription do ExampleDefenseBuff).</summary>
    public string Description { get; set; } = "";
    /// <summary>Marca como debuff (Main.debuff): a enfermeira e as pocoes de remocao o tratam como ruim.</summary>
    public bool IsDebuff { get; set; } = false;
    /// <summary>Main.buffNoSave: nao e gravado no personagem.</summary>
    public bool NoSave { get; set; } = false;
    /// <summary>Main.buffNoTimeDisplay: esconde o tempo restante na barra.</summary>
    public bool NoTimeDisplay { get; set; } = false;
    /// <summary>Prevents nurse removal through BuffID.Sets.NurseCannotRemoveDebuff.</summary>
    public bool NurseCannotRemove { get; set; } = false;
    /// <summary>Nome de um BunnyProjectile do mesmo mod (minion). O buff e renovado enquanto o jogador tiver esse projetil e removido sem ele.</summary>
    public string MinionProjectile { get; set; } = "";
    public abstract void SetDefaults();
}

public abstract class BunnyBuffPotion : BunnyRecipeSource
{
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public string Buff { get; set; } = "";
    public int Duration { get; set; } = 600;
    public abstract void SetDefaults();
}

public enum BunnyAccessoryVisualMode
{
    None = 0,
    EquippedSprite = 1,
    EnergyBarrier = 2,
    RunningTrail = 3
}

public abstract class BunnyAccessory : BunnyRecipeSource
{
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int DefenseBonus { get; set; } = 0;
    public string Tooltip { get; set; } = "";
    public BunnyAccessoryVisualMode VisualMode { get; set; } = BunnyAccessoryVisualMode.None;
    public string VisualTexture { get; set; } = "";
    public float VisualScale { get; set; } = 1f;
    public float VisualOpacity { get; set; } = 1f;
    public uint VisualColor { get; set; } = 0xFFFFFFFF;
    public abstract void SetDefaults();
}

public abstract class BunnyPlayer
{
    public string Accessory { get; set; } = "";
    public int DefenseBonus { get; protected set; } = 0;
    public int MoveSpeedPercent { get; protected set; } = 0;
    public abstract void SetDefaults();
    public virtual void RegisterHooks(BunnyHookRegistry hooks) { }
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
    public virtual bool PreUpdate() => false;
    public virtual bool PostUpdate() => false;
    public virtual string UpdateDead() => "";
    public virtual int UpdateLifeRegen(bool accessoryEquipped) => 0;
    public virtual int UpdateManaRegen(bool accessoryEquipped) => 0;
    public virtual void ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers) { }
    public virtual void ModifyHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtModifiers modifiers) { }
    public virtual void ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers) { }
    public virtual bool FreeDodge(bool accessoryEquipped, BunnyHurtModifiers modifiers) => false;
    public virtual void OnHurt(bool accessoryEquipped, BunnyHurtInfo info) { }
    public virtual void OnHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtInfo info) { }
    public virtual void OnHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtInfo info) { }
}

public sealed class BunnyHurtModifiers
{
    public int Damage { get; set; }
    public int HitDirection { get; set; }
    public int CurrentLife { get; set; }
    public int MaxLife { get; set; }
    public bool PvP { get; set; }
    public bool Quiet { get; set; }
    public bool Crit { get; set; }
    public bool Dodgeable { get; set; }
    public int NpcIndex { get; set; } = -1;
    public int ProjectileIndex { get; set; } = -1;
    public int NpcType { get; set; } = -1;
    public int NpcDamage { get; set; } = -1;
    public int NpcLife { get; set; } = -1;
    public int ProjectileType { get; set; } = -1;
    public int ProjectileDamage { get; set; } = -1;
    public int ProjectileOwner { get; set; } = -1;
    public float AttackerX { get; set; } = -1f;
    public float AttackerY { get; set; } = -1f;
    public float AttackerVelocityX { get; set; } = -1f;
    public float AttackerVelocityY { get; set; } = -1f;
    public float AttackerAngle { get; set; } = -1f;
    public float AttackerDistance { get; set; } = -1f;
    public bool Cancelled { get; private set; }

    public void Cancel() => Cancelled = true;
}

public readonly struct BunnyHurtInfo
{
    public int Damage { get; }
    public int SourceDamage { get; }
    public int HitDirection { get; }
    public int LifeRemaining { get; }
    public bool PvP { get; }
    public bool Crit { get; }
    public int NpcIndex { get; }
    public int ProjectileIndex { get; }
    public int NpcType { get; }
    public int NpcDamage { get; }
    public int NpcLife { get; }
    public int ProjectileType { get; }
    public int ProjectileDamage { get; }
    public int ProjectileOwner { get; }
    public float AttackerX { get; }
    public float AttackerY { get; }
    public float AttackerVelocityX { get; }
    public float AttackerVelocityY { get; }
    public float AttackerAngle { get; }
    public float AttackerDistance { get; }

    public BunnyHurtInfo(int damage, int sourceDamage, int hitDirection,
                         int lifeRemaining, bool pvp, bool crit,
                         int npcIndex = -1, int projectileIndex = -1,
                         int npcType = -1, int npcDamage = -1, int npcLife = -1,
                         int projectileType = -1, int projectileDamage = -1,
                         int projectileOwner = -1,
                         float attackerX = -1f, float attackerY = -1f,
                         float attackerVelocityX = -1f, float attackerVelocityY = -1f,
                         float attackerAngle = -1f, float attackerDistance = -1f)
    {
        Damage = damage;
        SourceDamage = sourceDamage;
        HitDirection = hitDirection;
        LifeRemaining = lifeRemaining;
        PvP = pvp;
        Crit = crit;
        NpcIndex = npcIndex;
        ProjectileIndex = projectileIndex;
        NpcType = npcType;
        NpcDamage = npcDamage;
        NpcLife = npcLife;
        ProjectileType = projectileType;
        ProjectileDamage = projectileDamage;
        ProjectileOwner = projectileOwner;
        AttackerX = attackerX;
        AttackerY = attackerY;
        AttackerVelocityX = attackerVelocityX;
        AttackerVelocityY = attackerVelocityY;
        AttackerAngle = attackerAngle;
        AttackerDistance = attackerDistance;
    }
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
    public virtual void OnSpawn(ref BunnyNpcAiState state) { }
    public virtual bool AI(ref BunnyNpcAiState state) => false;
    public virtual void OnHit(ref BunnyNpcAiState state, int damage) { }
    public virtual void OnKill(ref BunnyNpcAiState state) { }
    public virtual void OnDespawn(ref BunnyNpcAiState state) { }
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


public abstract class BunnyScreenEffect
{
    public string DisplayName { get; set; } = "";
    public string ShaderFile { get; set; } = "";
    public string PipelineFile { get; set; } = "";
    public int Layer { get; set; } = 0;
    public int Order { get; set; } = 0;
    public float Intensity { get; set; } = 1f;
    public abstract void SetDefaults();
}
