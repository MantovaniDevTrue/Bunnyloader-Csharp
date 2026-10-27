namespace BunnyLoader.Source;

public struct BunnyNpcAiState
{
    public int Slot;
    public int Type;
    public int Generation;
    public long NativeIdentity;
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
    public BunnyProjectileTileCollisionMode? NextTileCollisionMode;
    public bool HasTarget => TargetNpc >= 0;
}

public enum BunnyProjectileTileCollisionMode
{
    Vanilla = 0,
    KeepAlive = 1,
    Bounce = 2,
    Stop = 3
}

public enum BunnyProjectilePlatformCollision
{
    Vanilla = 0,
    SolidPlatforms = 1,
    PassThroughPlatforms = 2
}

public abstract class BunnyProjectile
{
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    private DamageClass? damageType;
    public DamageClass DamageType { get => damageType ?? (Minion ? DamageClass.Summon : DamageClass.Ranged); set => damageType = value ?? throw new System.ArgumentNullException(nameof(value)); }
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
    public float BounceRestitution { get; set; } = 1f;
    public float BounceMinimumSpeed { get; set; } = 0f;
    public int TileCollisionSurvivalLimit { get; set; } = 0;
    public BunnyProjectilePlatformCollision TilePlatformCollision { get; set; } = BunnyProjectilePlatformCollision.Vanilla;
    /// <summary>Tile collision width in pixels, 1..512; 0 preserves Terraria's width. Does not resize visuals or NPC damage hitboxes.</summary>
    public int TileCollisionBoxWidth { get; set; } = 0;
    /// <summary>Tile collision height in pixels, 1..512; 0 preserves Terraria's height.</summary>
    public int TileCollisionBoxHeight { get; set; } = 0;
    /// <summary>World-pixel translation of the resized tile collision box, -256..256; positive moves right. Not a normalized resizeAnchor value.</summary>
    public float TileCollisionAnchorOffsetX { get; set; } = 0f;
    /// <summary>World-pixel translation of the resized tile collision box, -256..256; positive moves down. Not a normalized resizeAnchor value.</summary>
    public float TileCollisionAnchorOffsetY { get; set; } = 0f;
    public bool IgnoreWater { get; set; } = true;
    public int Penetrate { get; set; } = 1;
    public int TimeLeft { get; set; } = 360;
    /// <summary>
    /// Configura as tabelas por tipo depois dos defaults do protótipo, como no
    /// SetupContent do tModLoader. Executado uma vez ao compilar/verificar o
    /// conteúdo; a política resultante é instalada na thread nativa do jogo.
    /// </summary>
    public virtual void SetStaticDefaults() { }
    public abstract void SetDefaults();
    public virtual void OnSpawn(ref BunnyProjectileAiState state) { }
    public virtual bool AI(ref BunnyProjectileAiState state) => false;
    public virtual void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage) { }
    public virtual void OnTileCollide(ref BunnyProjectileAiState state,
        float oldVelocityX, float oldVelocityY, float newVelocityX, float newVelocityY) { }
    public virtual void OnKill(ref BunnyProjectileAiState state) { }
    public virtual void OnDespawn(ref BunnyProjectileAiState state) { }
    public virtual int OnHitNPC(int damage) => 0;
    public virtual int OnKill(int damage) => 0;
    public virtual int OnKillDuration(int damage) => 0;
}

public struct BunnyPrefixApplyStats
{
    public int DamageBonus { get; set; }
    public int ValueBonus { get; set; }
}

public abstract class BunnyWeapon : BunnyShootSource
{
    public string ModRarityClassName { get; private set; } = "";
    public void SetRarity<T>() where T : BunnyRarity
        => ModRarityClassName = BunnyRarityReference.Resolve(GetType(), typeof(T));
    public BunnyHoldout? Holdout { get; set; }
    /// <summary>Som próprio do mod; tem precedência sobre o SoundID vanilla.</summary>
    public BunnySoundStyle? CustomUseSound { get; set; }
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    private DamageClass? damageType;
    public DamageClass DamageType { get => damageType ?? (Summon ? DamageClass.Summon : DamageClass.Ranged); set => damageType = value ?? throw new System.ArgumentNullException(nameof(value)); }
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int Damage { get; set; } = 20;
    public int UseTime { get; set; } = 18;
    public float ShootSpeed { get; set; } = 12f;
    public string Shoot { get; set; } = "";
    public bool? PrefixChance { get; set; }
    public bool Summon { get; set; }
    public string MinionBuff { get; set; } = "";
    public int MinionBuffTime { get; set; } = 2;
    public abstract void SetDefaults();
    public virtual void ConfigurePrefixRules(BunnyItemPrefixRules rules) { }
    public virtual bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => true;
    public virtual int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public virtual bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public virtual void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix, ref BunnyPrefixApplyStats effects) { }
    public virtual void ApplyPrefix(ref BunnyPrefixApplyStats effects) { }
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
    public string ModRarityClassName { get; private set; } = "";
    public void SetRarity<T>() where T : BunnyRarity
        => ModRarityClassName = BunnyRarityReference.Resolve(GetType(), typeof(T));
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public string Buff { get; set; } = "";
    public int Duration { get; set; } = 600;
    public bool? PrefixChance { get; set; }
    public abstract void SetDefaults();
    public virtual void ConfigurePrefixRules(BunnyItemPrefixRules rules) { }
    public virtual bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => true;
    public virtual int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public virtual bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public virtual void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix, ref BunnyPrefixApplyStats effects) { }
    public virtual void ApplyPrefix(ref BunnyPrefixApplyStats effects) { }
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
    public string ModRarityClassName { get; private set; } = "";
    public void SetRarity<T>() where T : BunnyRarity
        => ModRarityClassName = BunnyRarityReference.Resolve(GetType(), typeof(T));
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public int DefenseBonus { get; set; } = 0;
    public bool? PrefixChance { get; set; }
    public string Tooltip { get; set; } = "";
    public BunnyAccessoryVisualMode VisualMode { get; set; } = BunnyAccessoryVisualMode.None;
    public string VisualTexture { get; set; } = "";
    public float VisualScale { get; set; } = 1f;
    public float VisualOpacity { get; set; } = 1f;
    public uint VisualColor { get; set; } = 0xFFFFFFFF;
    public abstract void SetDefaults();
    public virtual void ConfigurePrefixRules(BunnyItemPrefixRules rules) { }
    public virtual bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => true;
    public virtual int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public virtual bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public virtual void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix, ref BunnyPrefixApplyStats effects) { }
    public virtual void ApplyPrefix(ref BunnyPrefixApplyStats effects) { }
}

public abstract class BunnyPlayer
{
    public string Accessory { get; set; } = "";
    public string FishingQuestFishName { get; set; } = "";
    public BunnyQuestCatchRules FishingCatchRules { get; set; }
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

public sealed class BunnyItemPrefixRules
{
    private readonly System.Collections.Generic.SortedSet<int> blocked = new();
    private readonly System.Collections.Generic.SortedSet<string> named =
        new(System.StringComparer.Ordinal);
    private readonly System.Collections.Generic.SortedDictionary<int, (int Min, int Max)> stackBlocked = new();
    private readonly System.Collections.Generic.SortedDictionary<string, (int Min, int Max)> namedStackBlocked =
        new(System.StringComparer.Ordinal);
    private readonly System.Collections.Generic.SortedDictionary<string, (string Prefix, char Attribute, int Min, int Max)> attributeDenials =
        new(System.StringComparer.Ordinal);
    private bool hasStackPrefixChance;
    private int chanceMinStack;
    private int chanceMaxStack;
    private bool chanceEnabled;
    private bool hasStackChoice;
    private string stackChoicePrefix = "";
    private int stackChoiceMin;
    private int stackChoiceMax;

    public void ChooseWhenStackBetween(int prefixType, int minimum, int maximum)
    {
        if (prefixType < 1 || prefixType > 255)
            throw new System.ArgumentOutOfRangeException(nameof(prefixType));
        SetStackChoice(prefixType.ToString(System.Globalization.CultureInfo.InvariantCulture), minimum, maximum);
    }

    public void ChooseWhenStackBetween(string ownModPrefixName, int minimum, int maximum)
        => SetStackChoice(AttributePrefixName(ownModPrefixName), minimum, maximum);

    private void SetStackChoice(string prefix, int minimum, int maximum)
    {
        if (minimum < 0 || maximum < minimum || maximum > 999999)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        if (hasStackChoice)
            throw new System.InvalidOperationException("Only one conditional prefix choice is supported");
        hasStackChoice = true;
        stackChoicePrefix = prefix;
        stackChoiceMin = minimum;
        stackChoiceMax = maximum;
    }

    public string SerializeStackChoice()
    {
        if (!hasStackChoice) return "";
        return stackChoicePrefix + ":" +
            stackChoiceMin.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
            stackChoiceMax.ToString(System.Globalization.CultureInfo.InvariantCulture);
    }

    public void Deny(int prefixType)
    {
        if (prefixType < 1 || prefixType > 255)
            throw new System.ArgumentOutOfRangeException(nameof(prefixType));
        if (!blocked.Contains(prefixType) && blocked.Count + named.Count + stackBlocked.Count + namedStackBlocked.Count + attributeDenials.Count >= 32)
            throw new System.InvalidOperationException("At most 32 denied prefixes per item");
        blocked.Add(prefixType);
    }

    public void Deny(string ownModPrefixName)
    {
        if (string.IsNullOrEmpty(ownModPrefixName) || ownModPrefixName.Length > 64 ||
            !(ownModPrefixName[0] is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or '_'))
            throw new System.ArgumentOutOfRangeException(nameof(ownModPrefixName));
        foreach (char ch in ownModPrefixName)
            if (!(ch is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9' or '_'))
                throw new System.ArgumentOutOfRangeException(nameof(ownModPrefixName));
        if (!named.Contains(ownModPrefixName) && blocked.Count + named.Count + stackBlocked.Count + namedStackBlocked.Count + attributeDenials.Count >= 32)
            throw new System.InvalidOperationException("At most 32 denied prefixes per item");
        named.Add(ownModPrefixName);
    }

    public void DenyWhenStackBetween(int prefixType, int minimum, int maximum)
    {
        if (prefixType < 1 || prefixType > 255 ||
            minimum < 0 || maximum < minimum || maximum > 999999)
            throw new System.ArgumentOutOfRangeException(nameof(prefixType));
        if (blocked.Contains(prefixType) || stackBlocked.ContainsKey(prefixType))
            throw new System.InvalidOperationException("Duplicate prefix deny rule");
        if (blocked.Count + named.Count + stackBlocked.Count + namedStackBlocked.Count + attributeDenials.Count >= 32)
            throw new System.InvalidOperationException("At most 32 denied prefixes per item");
        stackBlocked.Add(prefixType, (minimum, maximum));
    }

    public void DenyWhenStackBetween(string ownModPrefixName, int minimum, int maximum)
    {
        if (string.IsNullOrEmpty(ownModPrefixName) || ownModPrefixName.Length > 64 ||
            !(ownModPrefixName[0] is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or '_') ||
            minimum < 0 || maximum < minimum || maximum > 999999)
            throw new System.ArgumentOutOfRangeException(nameof(ownModPrefixName));
        foreach (char ch in ownModPrefixName)
            if (!(ch is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9' or '_'))
                throw new System.ArgumentOutOfRangeException(nameof(ownModPrefixName));
        if (named.Contains(ownModPrefixName) || namedStackBlocked.ContainsKey(ownModPrefixName))
            throw new System.InvalidOperationException("Duplicate prefix deny rule");
        if (blocked.Count + named.Count + stackBlocked.Count + namedStackBlocked.Count + attributeDenials.Count >= 32)
            throw new System.InvalidOperationException("At most 32 denied prefixes per item");
        namedStackBlocked.Add(ownModPrefixName, (minimum, maximum));
    }

    private void AddAttributeDeny(string prefix, char attribute, int minimum, int maximum)
    {
        int limit = attribute == 'D' ? 100000 : 10000;
        if (minimum < 0 || maximum < minimum || maximum > limit)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        string key = prefix + ":" + attribute;
        if (attributeDenials.ContainsKey(key))
            throw new System.InvalidOperationException("Duplicate prefix attribute deny rule");
        if (blocked.Count + named.Count + stackBlocked.Count + namedStackBlocked.Count + attributeDenials.Count >= 32)
            throw new System.InvalidOperationException("At most 32 denied prefixes per item");
        attributeDenials.Add(key, (prefix, attribute, minimum, maximum));
    }

    private static string AttributePrefixName(string name)
    {
        if (string.IsNullOrEmpty(name) || name.Length > 64 ||
            !(name[0] is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or '_'))
            throw new System.ArgumentOutOfRangeException(nameof(name));
        foreach (char c in name)
            if (!(c is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9' or '_'))
                throw new System.ArgumentOutOfRangeException(nameof(name));
        return name;
    }

    private static string AttributePrefixId(int prefixType)
    {
        if (prefixType < 1 || prefixType > 255)
            throw new System.ArgumentOutOfRangeException(nameof(prefixType));
        return prefixType.ToString(System.Globalization.CultureInfo.InvariantCulture);
    }

    public void DenyWhenDamageBetween(int prefixType, int minimum, int maximum)
        => AddAttributeDeny(AttributePrefixId(prefixType), 'D', minimum, maximum);

    public void DenyWhenDamageBetween(string ownModPrefixName, int minimum, int maximum)
        => AddAttributeDeny(AttributePrefixName(ownModPrefixName), 'D', minimum, maximum);

    public void DenyWhenManaBetween(int prefixType, int minimum, int maximum)
        => AddAttributeDeny(AttributePrefixId(prefixType), 'M', minimum, maximum);

    public void DenyWhenManaBetween(string ownModPrefixName, int minimum, int maximum)
        => AddAttributeDeny(AttributePrefixName(ownModPrefixName), 'M', minimum, maximum);

    public string SerializeAttributeDenials()
    {
        var result = new System.Collections.Generic.List<string>(attributeDenials.Count);
        foreach (var entry in attributeDenials.Values)
            result.Add(entry.Prefix + ":" + entry.Attribute + ":" +
                entry.Min.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
                entry.Max.ToString(System.Globalization.CultureInfo.InvariantCulture));
        return string.Join(",", result);
    }

    public void SetPrefixChanceWhenStackBetween(int minimum, int maximum, bool allowed)
    {
        if (minimum < 0 || maximum < minimum || maximum > 999999)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        if (hasStackPrefixChance)
            throw new System.InvalidOperationException("Only one stack prefix chance rule is supported");
        hasStackPrefixChance = true;
        chanceMinStack = minimum;
        chanceMaxStack = maximum;
        chanceEnabled = allowed;
    }

    public string SerializeStackChance()
    {
        if (!hasStackPrefixChance) return "";
        return chanceMinStack.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
            chanceMaxStack.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
            (chanceEnabled ? "1" : "0");
    }

    public string SerializeStackDenials()
    {
        var values = new System.Collections.Generic.List<string>(
            stackBlocked.Count + namedStackBlocked.Count);
        foreach (var entry in stackBlocked)
            values.Add(entry.Key.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
                entry.Value.Min.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
                entry.Value.Max.ToString(System.Globalization.CultureInfo.InvariantCulture));
        foreach (var entry in namedStackBlocked)
            values.Add(entry.Key + ":" +
                entry.Value.Min.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" +
                entry.Value.Max.ToString(System.Globalization.CultureInfo.InvariantCulture));
        return string.Join(",", values);
    }

    public string Serialize()
    {
        var values = new System.Collections.Generic.List<string>();
        foreach (int id in blocked)
            values.Add(id.ToString(System.Globalization.CultureInfo.InvariantCulture));
        foreach (string name in named)
            values.Add(name);
        return string.Join(",", values);
    }
}

public sealed class BunnyPrefixRollRules
{
    private readonly System.Collections.Generic.SortedDictionary<int, int> rules = new();
    private readonly System.Collections.Generic.SortedDictionary<string, int> namedRules =
        new(System.StringComparer.Ordinal);
    private readonly System.Collections.Generic.SortedDictionary<int, bool> canRoll = new();
    private readonly System.Collections.Generic.SortedDictionary<string, bool> namedCanRoll =
        new(System.StringComparer.Ordinal);
    private readonly System.Collections.Generic.SortedSet<int> preferredItems = new();
    private readonly System.Collections.Generic.SortedSet<string> namedPreferredItems =
        new(System.StringComparer.Ordinal);

    private void Reserve(bool exists)
    {
        if (!exists && rules.Count + namedRules.Count >= 32)
            throw new System.InvalidOperationException("BunnyPrefix supports at most 32 item-specific rules");
    }

    public void SetWeight(int itemType, int weight)
    {
        if (itemType < 1 || itemType > 32767 || weight < 0 || weight > 20)
            throw new System.ArgumentOutOfRangeException(nameof(itemType));
        Reserve(rules.ContainsKey(itemType));
        rules[itemType] = weight;
    }

    public void SetWeight(string itemName, int weight)
    {
        if (string.IsNullOrEmpty(itemName) || itemName.Length > 64 ||
            weight < 0 || weight > 20 ||
            !(itemName[0] is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or '_'))
            throw new System.ArgumentOutOfRangeException(nameof(itemName));
        foreach (char c in itemName)
            if (!(c is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9' or '_'))
                throw new System.ArgumentOutOfRangeException(nameof(itemName));
        Reserve(namedRules.ContainsKey(itemName));
        namedRules[itemName] = weight;
    }

    public void SetCanRoll(int itemType, bool allowed)
    {
        if (!rules.ContainsKey(itemType))
            throw new System.InvalidOperationException("SetWeight must be called before SetCanRoll");
        canRoll[itemType] = allowed;
        if (!allowed) preferredItems.Remove(itemType);
    }

    public void SetCanRoll(string itemName, bool allowed)
    {
        if (string.IsNullOrEmpty(itemName) || !namedRules.ContainsKey(itemName))
            throw new System.InvalidOperationException("SetWeight must be called before SetCanRoll");
        namedCanRoll[itemName] = allowed;
        if (!allowed) namedPreferredItems.Remove(itemName);
    }

    public void PreferFor(int itemType)
    {
        if (!rules.TryGetValue(itemType, out int weight) ||
            !(canRoll.TryGetValue(itemType, out bool allowed) ? allowed : weight > 0))
            throw new System.InvalidOperationException("PreferFor requires an allowed item rule");
        preferredItems.Add(itemType);
    }

    public void PreferFor(string itemName)
    {
        if (string.IsNullOrEmpty(itemName) ||
            !namedRules.TryGetValue(itemName, out int weight) ||
            !(namedCanRoll.TryGetValue(itemName, out bool allowed) ? allowed : weight > 0))
            throw new System.InvalidOperationException("PreferFor requires an allowed item rule");
        namedPreferredItems.Add(itemName);
    }

    public void Deny(int itemType)
    {
        SetWeight(itemType, 0);
        canRoll.Remove(itemType);
        preferredItems.Remove(itemType);
    }

    public void Deny(string itemName)
    {
        SetWeight(itemName, 0);
        namedCanRoll.Remove(itemName);
        namedPreferredItems.Remove(itemName);
    }
    public void Allow(int itemType, int weight = 1) => SetWeight(itemType, weight);
    public void Allow(string itemName, int weight = 1) => SetWeight(itemName, weight);

    public string Serialize()
    {
        var fields = new System.Collections.Generic.List<string>(rules.Count + namedRules.Count);
        foreach (var entry in rules)
        {
            string key = entry.Key.ToString(System.Globalization.CultureInfo.InvariantCulture)
                + ":" + entry.Value.ToString(System.Globalization.CultureInfo.InvariantCulture);
            bool overrideExists = canRoll.TryGetValue(entry.Key, out bool allowed);
            if (preferredItems.Contains(entry.Key))
            {
                if (!(overrideExists ? allowed : entry.Value > 0))
                    throw new System.InvalidOperationException("Preferred item cannot be denied");
                key += ":1:1";
            }
            else if (overrideExists)
                key += allowed ? ":1" : ":0";
            fields.Add(key);
        }
        foreach (var entry in namedRules)
        {
            string key = entry.Key + ":" +
                entry.Value.ToString(System.Globalization.CultureInfo.InvariantCulture);
            bool overrideExists = namedCanRoll.TryGetValue(entry.Key, out bool allowed);
            if (namedPreferredItems.Contains(entry.Key))
            {
                if (!(overrideExists ? allowed : entry.Value > 0))
                    throw new System.InvalidOperationException("Preferred item cannot be denied");
                key += ":1:1";
            }
            else if (overrideExists)
                key += allowed ? ":1" : ":0";
            fields.Add(key);
        }
        return string.Join(",", fields);
    }
}

public sealed class BunnyPrefixItemConditions
{
    public int MinDamage { get; private set; } = 0;
    public int MaxDamage { get; private set; } = 100000;
    public int MinMana { get; private set; } = 0;
    public int MaxMana { get; private set; } = 10000;
    public int MinStack { get; private set; } = 0;
    public int MaxStack { get; private set; } = 999999;
    public bool HasStackRollWeight { get; private set; }
    public int RollWeightMinStack { get; private set; }
    public int RollWeightMaxStack { get; private set; }
    public int RollWeightInRange { get; private set; }
    public bool HasAttributeRollWeight { get; private set; }
    public char AttributeRollWeightKind { get; private set; }
    public int AttributeRollWeightMinimum { get; private set; }
    public int AttributeRollWeightMaximum { get; private set; }
    public int AttributeRollWeightValue { get; private set; }

    public void RequireDamageBetween(int minimum, int maximum)
    {
        if (minimum < 0 || maximum < minimum || maximum > 100000)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        MinDamage = minimum;
        MaxDamage = maximum;
    }

    public void RequireManaBetween(int minimum, int maximum)
    {
        if (minimum < 0 || maximum < minimum || maximum > 10000)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        MinMana = minimum;
        MaxMana = maximum;
    }

    public void RequireStackBetween(int minimum, int maximum)
    {
        if (minimum < 0 || maximum < minimum || maximum > 999999)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        MinStack = minimum;
        MaxStack = maximum;
    }

    public void OverrideRollWeightWhenStackBetween(int minimum, int maximum, int weight)
    {
        if (minimum < 0 || maximum < minimum || maximum > 999999 ||
            weight < 0 || weight > 20)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        if (HasStackRollWeight || HasAttributeRollWeight)
            throw new System.InvalidOperationException("Only one dynamic roll weight rule is supported");
        HasStackRollWeight = true;
        RollWeightMinStack = minimum;
        RollWeightMaxStack = maximum;
        RollWeightInRange = weight;
    }

    private void OverrideAttributeRollWeight(char attribute, int minimum, int maximum, int weight)
    {
        int limit = attribute == 'D' ? 100000 : 10000;
        if (minimum < 0 || maximum < minimum || maximum > limit ||
            weight < 0 || weight > 20)
            throw new System.ArgumentOutOfRangeException(nameof(minimum));
        if (HasStackRollWeight || HasAttributeRollWeight)
            throw new System.InvalidOperationException("Only one dynamic roll weight rule is supported");
        HasAttributeRollWeight = true;
        AttributeRollWeightKind = attribute;
        AttributeRollWeightMinimum = minimum;
        AttributeRollWeightMaximum = maximum;
        AttributeRollWeightValue = weight;
    }

    public void OverrideRollWeightWhenDamageBetween(int minimum, int maximum, int weight)
        => OverrideAttributeRollWeight('D', minimum, maximum, weight);

    public void OverrideRollWeightWhenManaBetween(int minimum, int maximum, int weight)
        => OverrideAttributeRollWeight('M', minimum, maximum, weight);
}

public readonly struct BunnyPrefixTooltipLine
{
    public string Text { get; }
    public bool IsNegative { get; }
    public BunnyPrefixTooltipLine(string text, bool isNegative = false)
    {
        Text = text;
        IsNegative = isNegative;
    }
}

public abstract class BunnyPrefix
{
    public string DisplayName { get; set; } = "";
    public PrefixCategory Category { get; set; } = PrefixCategory.Custom;
    public float RollWeight { get; set; } = 1f;

    public abstract void SetDefaults();
    public virtual void ConfigureRollRules(BunnyPrefixRollRules rules) { }
    public virtual void ConfigureItemConditions(BunnyPrefixItemConditions conditions) { }
    public virtual bool CanRoll(BunnyPrefixItemSnapshot item) => true;
    public virtual int RollChance(BunnyPrefixItemSnapshot item) =>
        (int)System.MathF.Round(RollWeight);
    public virtual bool AllStatChangesHaveEffectOn(BunnyPrefixItemSnapshot item) => true;
    public virtual void Apply(BunnyPrefixItemSnapshot item, ref BunnyPrefixApplyStats effects) { }
    public virtual System.Collections.Generic.IReadOnlyList<BunnyPrefixTooltipLine> GetTooltipLines(
        BunnyPrefixItemSnapshot item) => System.Array.Empty<BunnyPrefixTooltipLine>();
    public virtual void SetStats(ref PrefixStats stats) { }
    public bool RequireEffectiveStats { get; set; } = true;
    public string ExtraTooltip { get; set; } = "";
    private readonly System.Collections.Generic.List<string> tooltipLines = new();
    private readonly System.Collections.Generic.List<string> tooltipItemLines = new();

    public void AddTooltipLine(string text)
    {
        if (string.IsNullOrEmpty(text) || tooltipLines.Count + tooltipItemLines.Count >= 4 ||
            System.Text.Encoding.UTF8.GetByteCount(text) > 120)
            throw new System.ArgumentOutOfRangeException(nameof(text));
        foreach (char value in text)
            if (char.IsControl(value))
                throw new System.ArgumentOutOfRangeException(nameof(text));
        tooltipLines.Add(text);
    }

    public void AddTooltipItemLine(string template)
    {
        if (string.IsNullOrEmpty(template) || tooltipLines.Count + tooltipItemLines.Count >= 4 ||
            System.Text.Encoding.UTF8.GetByteCount(template) > 120)
            throw new System.ArgumentOutOfRangeException(nameof(template));
        foreach (char value in template)
            if (char.IsControl(value))
                throw new System.ArgumentOutOfRangeException(nameof(template));
        bool variable = template.Contains("{damage}", System.StringComparison.Ordinal) ||
                        template.Contains("{mana}", System.StringComparison.Ordinal) ||
                        template.Contains("{stack}", System.StringComparison.Ordinal);
        string rest = template.Replace("{damage}", "", System.StringComparison.Ordinal)
                              .Replace("{mana}", "", System.StringComparison.Ordinal)
                              .Replace("{stack}", "", System.StringComparison.Ordinal);
        if (!variable || rest.Contains('{') || rest.Contains('}'))
            throw new System.ArgumentOutOfRangeException(nameof(template));
        tooltipItemLines.Add(template);
    }

    public string SerializeTooltipItemLines()
    {
        var lines = new System.Collections.Generic.List<string>(tooltipItemLines.Count);
        foreach (string template in tooltipItemLines)
            lines.Add(System.Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(template)));
        return string.Join(",", lines);
    }

    public string SerializeTooltipLines()
    {
        var lines = new System.Collections.Generic.List<string>(tooltipLines.Count);
        foreach (string line in tooltipLines)
            lines.Add(System.Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(line)));
        return string.Join(",", lines);
    }

    public virtual void Apply(ref BunnyPrefixApplyStats effects) { }
    public virtual void ModifyValue(ref float valueMultiplier) { }
    public virtual void ApplyAccessoryEffects(ref int defenseBonus) { }
    public virtual void ApplyAccessoryEffects(BunnyPrefixPlayerSnapshot player, ref int defenseBonus) { }
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
