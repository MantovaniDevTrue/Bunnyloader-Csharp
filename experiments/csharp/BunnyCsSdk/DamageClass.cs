using System;

namespace BunnyLoader.Source;

/// <summary>
/// Classe de dano do tModLoader 1.4.5, isolada da ponte Android.
/// Os bônus de uma classe não herdam outras classes automaticamente:
/// GetModifierInheritance define as cinco frações individualmente.
/// </summary>
/// <remarks>
/// Base: tModLoader, DamageClass.cs e VanillaDamageClasses.cs, licença MIT.
/// A assinatura SetDefaultStats usa DamageClassPlayer, o adaptador C# do Bunny,
/// até a integração nativa com Terraria.Player estar implementada.
/// </remarks>
public abstract class DamageClass
{
    public static DamageClass Default { get; } = new DefaultDamageClass();
    public static DamageClass Generic { get; } = new GenericDamageClass();
    public static DamageClass Melee { get; } = new MeleeDamageClass();
    public static DamageClass MeleeNoSpeed { get; } = new MeleeNoSpeedDamageClass();
    public static DamageClass Ranged { get; } = new RangedDamageClass();
    public static DamageClass Magic { get; } = new MagicDamageClass();
    public static DamageClass Summon { get; } = new SummonDamageClass();
    public static DamageClass SummonMeleeSpeed { get; } = new SummonMeleeSpeedDamageClass();
    public static DamageClass MagicSummonHybrid { get; } = new MagicSummonHybridDamageClass();
    public static DamageClass Throwing { get; } = new ThrowingDamageClass();

    public int Type { get; internal set; } = -1;
    public virtual string LocalizationCategory => "DamageClasses";
    public virtual string DisplayNameKey => GetType().Name;

    /// <summary>Por padrão, somente os cinco atributos de Generic são herdados.</summary>
    public virtual StatInheritanceData GetModifierInheritance(DamageClass damageClass) =>
        ReferenceEquals(damageClass, Generic) ? StatInheritanceData.Full : StatInheritanceData.None;

    /// <summary>Efeitos como magma e espectro são independentes dos bônus numéricos.</summary>
    public virtual bool GetEffectInheritance(DamageClass damageClass) => false;

    /// <summary>Por padrão, prefixos acompanham a herança de efeitos.</summary>
    public virtual bool GetPrefixInheritance(DamageClass damageClass) => GetEffectInheritance(damageClass);

    /// <summary>Aplicado ao iniciar/resetar o snapshot estatístico de um jogador.</summary>
    public virtual void SetDefaultStats(DamageClassPlayer player) { }

    public virtual bool UseStandardCritCalcs => true;
    public virtual bool ShowStatTooltipLine(DamageClassPlayer player, string lineName) => true;

    public bool CountsAsClass(DamageClass damageClass) => DamageClassLoader.CountsAs(this, damageClass);
    public bool CountsAsClass<T>() where T : DamageClass => CountsAsClass(DamageClassLoader.GetInstance<T>());

    public bool GetsPrefixesFor(DamageClass damageClass)
    {
        if (damageClass is null) throw new ArgumentNullException(nameof(damageClass));
        return ReferenceEquals(this, damageClass) || GetPrefixInheritance(damageClass);
    }

    public bool GetsPrefixesFor<T>() where T : DamageClass => GetsPrefixesFor(DamageClassLoader.GetInstance<T>());
}

public abstract class VanillaDamageClass : DamageClass
{
    protected abstract string LangKey { get; }
    public sealed override string DisplayNameKey => LangKey;
}

public sealed class DefaultDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.55";
    public override StatInheritanceData GetModifierInheritance(DamageClass other) => StatInheritanceData.None;
}
public sealed class GenericDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.55";
    public override StatInheritanceData GetModifierInheritance(DamageClass other) => StatInheritanceData.None;
    public override void SetDefaultStats(DamageClassPlayer player) => player.GetCritChance(this) = 4f;
}
public sealed class MeleeDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.2";
}
public sealed class MeleeNoSpeedDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.2";
    public override StatInheritanceData GetModifierInheritance(DamageClass other)
    {
        if (ReferenceEquals(other, Generic) || ReferenceEquals(other, Melee))
            return new StatInheritanceData(1f, 1f, 0f, 1f, 1f);
        return StatInheritanceData.None;
    }
    public override bool GetEffectInheritance(DamageClass other) => ReferenceEquals(other, Melee);
}
public sealed class RangedDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.3";
}
public sealed class MagicDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.4";
}
public sealed class SummonDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.53";
    public override bool UseStandardCritCalcs => false;
    public override bool ShowStatTooltipLine(DamageClassPlayer player, string lineName) =>
        lineName != "CritChance" && lineName != "Speed";
}
public sealed class SummonMeleeSpeedDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.53";
    public override StatInheritanceData GetModifierInheritance(DamageClass other)
    {
        if (ReferenceEquals(other, Melee))
            return new StatInheritanceData(attackSpeedInheritance: 1f);
        if (ReferenceEquals(other, Generic) || ReferenceEquals(other, Summon))
            return StatInheritanceData.Full;
        return StatInheritanceData.None;
    }
    public override bool GetEffectInheritance(DamageClass other) => ReferenceEquals(other, Summon);
    public override bool GetPrefixInheritance(DamageClass other) => ReferenceEquals(other, Melee);
    public override bool UseStandardCritCalcs => false;
    public override bool ShowStatTooltipLine(DamageClassPlayer player, string lineName) => lineName != "CritChance";
}
public sealed class MagicSummonHybridDamageClass : VanillaDamageClass
{
    protected override string LangKey => "tModLoader.MagicSummonHybridDamageClass";
    public override StatInheritanceData GetModifierInheritance(DamageClass other) =>
        ReferenceEquals(other, Generic) || ReferenceEquals(other, Magic) ||
        ReferenceEquals(other, Summon) ? StatInheritanceData.Full : StatInheritanceData.None;
    public override bool GetEffectInheritance(DamageClass other) =>
        ReferenceEquals(other, Magic) || ReferenceEquals(other, Summon);
}
public sealed class ThrowingDamageClass : VanillaDamageClass
{
    protected override string LangKey => "LegacyTooltip.58";
    public override bool GetPrefixInheritance(DamageClass other) => ReferenceEquals(other, Ranged);
}
