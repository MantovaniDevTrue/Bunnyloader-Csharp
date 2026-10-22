using System;

namespace BunnyLoader.Source;

/// <summary>
/// Composição de atributos compatível com Terraria.ModLoader.StatModifier.
/// Base é aplicado antes dos multiplicadores; Flat, depois. Somar 0.12f concede 12%.
/// Esta estrutura apenas calcula valores; não registra hooks nem altera dano no Terraria.
/// </summary>
/// <remarks>
/// Contrato matemático adaptado de tModLoader (licença MIT):
/// https://github.com/tModLoader/tModLoader/blob/stable/patches/tModLoader/Terraria/ModLoader/StatModifier.cs
/// Use new StatModifier() ou StatModifier.Default para a identidade.
/// default(StatModifier) produz campos zeroados, como no tModLoader, e NÃO é a identidade.
/// </remarks>
public struct StatModifier
{
    public static readonly StatModifier Default = new();

    public float Base = 0f;
    public float Additive { get; private set; } = 1f;
    public float Multiplicative { get; private set; } = 1f;
    public float Flat = 0f;

    public StatModifier() { }

    /// <summary>Permite compor as quatro etapas sem perder o estado inicial.</summary>
    public StatModifier(float additive, float multiplicative, float flat = 0f, float @base = 0f)
    {
        Additive = additive;
        Multiplicative = multiplicative;
        Flat = flat;
        Base = @base;
    }

    /// <summary>Somar 0.12f representa mais 12% aditivos, como no tModLoader.</summary>
    public static StatModifier operator +(StatModifier modifier, float add)
        => new(modifier.Additive + add, modifier.Multiplicative, modifier.Flat, modifier.Base);

    public static StatModifier operator -(StatModifier modifier, float subtract)
        => new(modifier.Additive - subtract, modifier.Multiplicative, modifier.Flat, modifier.Base);

    /// <summary>Multiplica o fator separado; para bônus normais prefira o operador +.</summary>
    public static StatModifier operator *(StatModifier modifier, float factor)
        => new(modifier.Additive, modifier.Multiplicative * factor, modifier.Flat, modifier.Base);

    public static StatModifier operator /(StatModifier modifier, float divisor)
        => new(modifier.Additive, modifier.Multiplicative / divisor, modifier.Flat, modifier.Base);

    public static StatModifier operator +(float add, StatModifier modifier) => modifier + add;
    public static StatModifier operator *(float factor, StatModifier modifier) => modifier * factor;

    public static bool operator ==(StatModifier left, StatModifier right)
        => left.Additive == right.Additive
        && left.Multiplicative == right.Multiplicative
        && left.Flat == right.Flat
        && left.Base == right.Base;

    public static bool operator !=(StatModifier left, StatModifier right) => !(left == right);

    /// <summary>Ordem exata: (valor + Base) * Additive * Multiplicative + Flat.</summary>
    public float ApplyTo(float baseValue)
        => (baseValue + Base) * Additive * Multiplicative + Flat;

    /// <summary>Combina bônus aditivos por soma; multiplica os fatores multiplicativos.</summary>
    public StatModifier CombineWith(StatModifier other)
        => new(Additive + other.Additive - 1f, Multiplicative * other.Multiplicative,
            Flat + other.Flat, Base + other.Base);

    /// <summary>
    /// Escala para herança de DamageClass. NÃO use Scale(0.12f) para conceder 12%;
    /// para isso use modifier += 0.12f.
    /// </summary>
    public StatModifier Scale(float scale)
        => new(1f + (Additive - 1f) * scale, 1f + (Multiplicative - 1f) * scale,
            Flat * scale, Base * scale);

    /// <summary>Inverte ApplyTo quando Additive * Multiplicative é diferente de zero.</summary>
    public float Undo(float currentValue)
        => ((currentValue - Flat) / (Multiplicative * Additive)) - Base;

    public override bool Equals(object? obj) => obj is StatModifier other && this == other;

    public override int GetHashCode()
    {
        int hash = 1713062080;
        hash = unchecked(hash * -1521134295 + Additive.GetHashCode());
        hash = unchecked(hash * -1521134295 + Multiplicative.GetHashCode());
        hash = unchecked(hash * -1521134295 + Flat.GetHashCode());
        hash = unchecked(hash * -1521134295 + Base.GetHashCode());
        return hash;
    }
}
