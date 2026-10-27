using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

/// <summary>
/// Equivalente ao GlobalItem do tModLoader para itens vanilla no Terraria Android.
/// <para>
/// Os hooks de conteúdo (AppliesToEntity, SetDefaults, ModifyTooltips,
/// ModifyWeaponDamage/Crit/Knockback e UpdateEquip) rodam UMA vez no MonoVM,
/// sobre uma foto de cada item vanilla tirada pelo jogo depois do SetDefaults.
/// O resultado vira tabelas nativas aplicadas sem C# por frame. Por isso esses
/// hooks não recebem Player: não dependem de quem segura o item nem do momento.
/// </para>
/// <para>
/// Os callbacks de disparo (CanShoot, ModifyShootStats, Shoot) rodam no momento
/// do uso, no mesmo caminho dos <see cref="BunnyShootSource"/>: só para os itens
/// vanilla em que AppliesToEntity é verdadeiro e que atiram (Shoot &gt; 0).
/// </para>
/// <para>
/// Os callbacks de prefixo (AppliesTo, AllowPrefix, ChoosePrefix,
/// DecidePrefixChance, ApplyPrefix) seguem o fluxo próprio do PrefixLoader.
/// </para>
/// </summary>
public abstract class BunnyGlobalItem
{
    // ---- conteúdo (itens vanilla) ----

    /// <summary>tModLoader: AppliesToEntity. Padrão: todos os itens vanilla.</summary>
    public virtual bool AppliesToEntity(BunnyVanillaItem item) => true;

    /// <summary>tModLoader: SetDefaults(Item). Altere as propriedades de <paramref name="item"/>.</summary>
    public virtual void SetDefaults(BunnyVanillaItem item) { }

    /// <summary>
    /// tModLoader: ModifyTooltips. A lista começa vazia; as linhas adicionadas
    /// aparecem no fim do tooltip do item (até 8 por item, 120 bytes cada).
    /// </summary>
    public virtual void ModifyTooltips(BunnyVanillaItem item, List<BunnyTooltipLine> tooltips) { }

    /// <summary>tModLoader: ModifyWeaponDamage, sem Player (bônus fixo do item).</summary>
    public virtual void ModifyWeaponDamage(BunnyVanillaItem item, ref StatModifier damage) { }

    /// <summary>tModLoader: ModifyWeaponCrit, sem Player (bônus fixo do item).</summary>
    public virtual void ModifyWeaponCrit(BunnyVanillaItem item, ref float crit) { }

    /// <summary>tModLoader: ModifyWeaponKnockback, sem Player (bônus fixo do item).</summary>
    public virtual void ModifyWeaponKnockback(BunnyVanillaItem item, ref StatModifier knockback) { }

    /// <summary>
    /// tModLoader: UpdateEquip/UpdateAccessory. Declare os bônus aplicados
    /// enquanto o item estiver equipado (armadura ou acessório).
    /// </summary>
    public virtual void UpdateEquip(BunnyVanillaItem item, BunnyEquipEffects effects) { }

    // ---- disparo (momento do uso) ----

    /// <summary>
    /// tModLoader: CanShoot(Item, Player). Falso em qualquer GlobalItem cancela o
    /// disparo (o item ainda gasta o tempo de uso, como no jogo).
    /// </summary>
    public virtual bool CanShoot(BunnyShootContext context) => true;

    /// <summary>tModLoader: ModifyShootStats. Altere posição, velocidade, tipo, dano e repulsão.</summary>
    public virtual void ModifyShootStats(BunnyShootContext context, ref BunnyShot shot) { }

    /// <summary>
    /// tModLoader: Shoot. Use <see cref="BunnyShootContext.Spawn"/> para disparos
    /// extras; retorne false para impedir o disparo original.
    /// </summary>
    public virtual bool Shoot(BunnyShootContext context, BunnyShot shot) => true;

    // ---- prefixos ----

    public virtual bool AppliesTo(BunnyPrefixItemSnapshot item) => true;
    public virtual bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => true;
    public virtual int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public virtual bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public virtual void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix,
        ref BunnyPrefixApplyStats effects) { }
}

/// <summary>Bônus declarados por <see cref="BunnyGlobalItem.UpdateEquip"/>.</summary>
public sealed class BunnyEquipEffects
{
    private readonly List<BunnyClassBonus> classBonuses = new();
    public IReadOnlyList<BunnyClassBonus> ClassBonuses => classBonuses;

    /// <summary>Mesmo canal de <see cref="BunnyItem.AddClassBonusOnEquip"/>.</summary>
    public void AddClassBonus(BunnyClassBonus bonus)
    {
        if (bonus.Class is null) throw new ArgumentNullException(nameof(bonus), "ClassBonus requer DamageClass");
        if (classBonuses.Count >= 16) throw new InvalidOperationException("UpdateEquip: limite de 16 bônus de classe por item");
        foreach (float value in new[] { bonus.Damage, bonus.DamageMultiplier, bonus.Crit, bonus.AttackSpeed,
                                        bonus.Knockback, bonus.KnockbackMultiplier, bonus.ArmorPenetration })
            if (!float.IsFinite(value)) throw new ArgumentOutOfRangeException(nameof(bonus), "ClassBonus requer valores finitos");
        classBonuses.Add(bonus);
    }
}

/// <summary>
/// Item vanilla como o jogo o deixa depois do SetDefaults. Em
/// <see cref="BunnyGlobalItem.SetDefaults"/> as propriedades graváveis podem ser
/// alteradas; nos outros hooks são somente leitura.
/// </summary>
public sealed class BunnyVanillaItem
{
    internal enum Kind { Int, Float, Bool }
    internal readonly struct Field
    {
        public readonly string Native;
        public readonly Kind Type;
        public readonly bool Writable;
        public Field(string native, Kind type, bool writable) { Native = native; Type = type; Writable = writable; }
    }

    // Nome nativo do campo em Terraria.Item. Só os graváveis estão na lista
    // de NativeDefaults do host (nativeDefaultAllowed).
    internal static readonly Field[] Fields =
    {
        new("damage", Kind.Int, true), new("crit", Kind.Int, true), new("knockBack", Kind.Float, true),
        new("useTime", Kind.Int, true), new("useAnimation", Kind.Int, true), new("useStyle", Kind.Int, true),
        new("melee", Kind.Bool, true), new("ranged", Kind.Bool, true), new("magic", Kind.Bool, true),
        new("summon", Kind.Bool, true), new("accessory", Kind.Bool, true), new("vanity", Kind.Bool, true),
        new("consumable", Kind.Bool, true), new("maxStack", Kind.Int, true), new("value", Kind.Int, true),
        new("rare", Kind.Int, true), new("defense", Kind.Int, false), new("shoot", Kind.Int, true),
        new("shootSpeed", Kind.Float, true), new("ammo", Kind.Int, true), new("useAmmo", Kind.Int, true),
        new("pick", Kind.Int, true), new("axe", Kind.Int, true), new("hammer", Kind.Int, true),
        new("mana", Kind.Int, true), new("autoReuse", Kind.Bool, true), new("width", Kind.Int, true),
        new("height", Kind.Int, true), new("healLife", Kind.Int, true), new("healMana", Kind.Int, true),
        new("noMelee", Kind.Bool, true), new("potion", Kind.Bool, true), new("useTurn", Kind.Bool, true),
        new("tileBoost", Kind.Int, true), new("createTile", Kind.Int, false),
    };
    internal static readonly Dictionary<string, int> FieldIndex = BuildIndex();
    private static Dictionary<string, int> BuildIndex()
    {
        var index = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < Fields.Length; i++) index[Fields[i].Native] = i;
        return index;
    }

    private readonly double[] values;
    private readonly double[] original;
    internal bool Writable;

    internal BunnyVanillaItem(int type, double[] snapshot)
    {
        if (snapshot.Length != Fields.Length) throw new ArgumentException("Snapshot de item com campos errados");
        Type = type;
        values = (double[])snapshot.Clone();
        original = (double[])snapshot.Clone();
    }

    /// <summary>ItemID do item vanilla.</summary>
    public int Type { get; }

    private int I(string name) => (int)values[FieldIndex[name]];
    private float F(string name) => (float)values[FieldIndex[name]];
    private bool B(string name) => values[FieldIndex[name]] != 0;
    private void Set(string name, double value)
    {
        int i = FieldIndex[name];
        if (!Writable)
            throw new InvalidOperationException("BunnyVanillaItem." + name + " só pode ser alterado em SetDefaults");
        if (!Fields[i].Writable)
            throw new InvalidOperationException("BunnyVanillaItem." + name + " é somente leitura no Android");
        if (double.IsNaN(value) || double.IsInfinity(value))
            throw new ArgumentOutOfRangeException(name, "Valor precisa ser finito");
        values[i] = value;
    }

    public int Damage { get => I("damage"); set => Set("damage", Math.Clamp(value, 0, 100000)); }
    public int Crit { get => I("crit"); set => Set("crit", Math.Clamp(value, -100, 100)); }
    public float KnockBack { get => F("knockBack"); set => Set("knockBack", Math.Clamp(value, 0f, 1000f)); }
    public int UseTime { get => I("useTime"); set => Set("useTime", Math.Clamp(value, 1, 10000)); }
    public int UseAnimation { get => I("useAnimation"); set => Set("useAnimation", Math.Clamp(value, 1, 10000)); }
    public int UseStyle { get => I("useStyle"); set => Set("useStyle", Math.Clamp(value, 0, 64)); }
    public bool Melee { get => B("melee"); set => Set("melee", value ? 1 : 0); }
    public bool Ranged { get => B("ranged"); set => Set("ranged", value ? 1 : 0); }
    public bool Magic { get => B("magic"); set => Set("magic", value ? 1 : 0); }
    public bool Summon { get => B("summon"); set => Set("summon", value ? 1 : 0); }
    public bool Accessory { get => B("accessory"); set => Set("accessory", value ? 1 : 0); }
    public bool Vanity { get => B("vanity"); set => Set("vanity", value ? 1 : 0); }
    public bool Consumable { get => B("consumable"); set => Set("consumable", value ? 1 : 0); }
    public int MaxStack { get => I("maxStack"); set => Set("maxStack", Math.Clamp(value, 1, 9999)); }
    public int Value { get => I("value"); set => Set("value", Math.Clamp(value, 0, 100000000)); }
    public int Rare { get => I("rare"); set => Set("rare", Math.Clamp(value, -13, 11)); }
    /// <summary>Somente leitura no Android (o host não grava defesa por NativeDefaults).</summary>
    public int Defense => I("defense");
    public int Shoot { get => I("shoot"); set => Set("shoot", Math.Clamp(value, 0, 32767)); }
    public float ShootSpeed { get => F("shootSpeed"); set => Set("shootSpeed", Math.Clamp(value, 0f, 1000f)); }
    public int Ammo { get => I("ammo"); set => Set("ammo", Math.Clamp(value, 0, 32767)); }
    public int UseAmmo { get => I("useAmmo"); set => Set("useAmmo", Math.Clamp(value, 0, 32767)); }
    public int Pick { get => I("pick"); set => Set("pick", Math.Clamp(value, 0, 1000)); }
    public int Axe { get => I("axe"); set => Set("axe", Math.Clamp(value, 0, 1000)); }
    public int Hammer { get => I("hammer"); set => Set("hammer", Math.Clamp(value, 0, 1000)); }
    public int Mana { get => I("mana"); set => Set("mana", Math.Clamp(value, 0, 10000)); }
    public bool AutoReuse { get => B("autoReuse"); set => Set("autoReuse", value ? 1 : 0); }
    public int Width { get => I("width"); set => Set("width", Math.Clamp(value, 0, 1024)); }
    public int Height { get => I("height"); set => Set("height", Math.Clamp(value, 0, 1024)); }
    public int HealLife { get => I("healLife"); set => Set("healLife", Math.Clamp(value, 0, 10000)); }
    public int HealMana { get => I("healMana"); set => Set("healMana", Math.Clamp(value, 0, 10000)); }
    public bool NoMelee { get => B("noMelee"); set => Set("noMelee", value ? 1 : 0); }
    public bool Potion { get => B("potion"); set => Set("potion", value ? 1 : 0); }
    public bool UseTurn { get => B("useTurn"); set => Set("useTurn", value ? 1 : 0); }
    public int TileBoost { get => I("tileBoost"); set => Set("tileBoost", Math.Clamp(value, -100, 100)); }
    /// <summary>Somente leitura: tile que o item coloca (-1 = nenhum).</summary>
    public int CreateTile => I("createTile");

    /// <summary>Classe de dano vanilla pelas flags do item (Default quando nenhuma).</summary>
    public DamageClass DamageType =>
        Melee ? DamageClass.Melee : Ranged ? DamageClass.Ranged : Magic ? DamageClass.Magic :
        Summon ? DamageClass.Summon : DamageClass.Default;

    internal IEnumerable<(Field Field, double Value)> Changes()
    {
        for (int i = 0; i < values.Length; i++)
            if (values[i] != original[i]) yield return (Fields[i], values[i]);
    }
}
