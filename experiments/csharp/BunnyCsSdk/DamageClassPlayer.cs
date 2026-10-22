using System;

namespace BunnyLoader.Source;

/// <summary>
/// Snapshot C# de bônus por classe, seguindo Terraria.Player.GetTotal* do tModLoader.
/// Não lê/escreve no Terraria Android até o dispatcher nativo estar conectado.
/// Um slot de dano é struct, portanto GetDamage/GetKnockback retornam ref.
/// </summary>
public sealed class DamageClassPlayer
{
    private struct DamageData
    {
        public StatModifier Damage;
        public StatModifier Knockback;
        public float CritChance;
        public float AttackSpeed;
        public float ArmorPen;

        public DamageData()
        {
            Damage = StatModifier.Default;
            Knockback = StatModifier.Default;
            CritChance = 0f;
            AttackSpeed = 1f;
            ArmorPen = 0f;
        }
    }

    private DamageData[] damageData = Array.Empty<DamageData>();
    private int generation = -1;

    public DamageClassPlayer() => ResetEffects();

    /// <summary>
    /// Usar uma vez a cada atualização de atributos do jogador, antes de
    /// UpdateAccessory/UpdateEquips. Não invocar em toda leitura GetTotal*.
    /// </summary>
    public void ResetEffects()
    {
        int count = DamageClassLoader.DamageClassCount;
        if (damageData.Length != count)
            damageData = new DamageData[count];
        for (int i = 0; i < count; i++)
            damageData[i] = new DamageData();
        generation = DamageClassLoader.Generation;
        foreach (DamageClass damageClass in DamageClassLoader.All)
            damageClass.SetDefaultStats(this);
    }

    private ref DamageData Entry(DamageClass cls)
    {
        DamageClassLoader.Validate(cls);
        if (generation != DamageClassLoader.Generation || damageData.Length != DamageClassLoader.DamageClassCount)
            ResetEffects();
        return ref damageData[cls.Type];
    }

    public ref StatModifier GetDamage(DamageClass cls) => ref Entry(cls).Damage;
    public ref StatModifier GetKnockback(DamageClass cls) => ref Entry(cls).Knockback;
    public ref float GetCritChance(DamageClass cls) => ref Entry(cls).CritChance;
    public ref float GetAttackSpeed(DamageClass cls) => ref Entry(cls).AttackSpeed;
    public ref float GetArmorPenetration(DamageClass cls) => ref Entry(cls).ArmorPen;

    public ref StatModifier GetDamage<T>() where T : DamageClass => ref GetDamage(DamageClassLoader.GetInstance<T>());
    public ref StatModifier GetKnockback<T>() where T : DamageClass => ref GetKnockback(DamageClassLoader.GetInstance<T>());
    public ref float GetCritChance<T>() where T : DamageClass => ref GetCritChance(DamageClassLoader.GetInstance<T>());
    public ref float GetAttackSpeed<T>() where T : DamageClass => ref GetAttackSpeed(DamageClassLoader.GetInstance<T>());
    public ref float GetArmorPenetration<T>() where T : DamageClass => ref GetArmorPenetration(DamageClassLoader.GetInstance<T>());

    /// <summary>Soma a herança aditiva e multiplica apenas os fatores multiplicativos.</summary>
    public StatModifier GetTotalDamage(DamageClass cls)
    {
        StatModifier result = GetDamage(cls);
        foreach (DamageClass source in DamageClassLoader.All)
        {
            if (ReferenceEquals(source, cls)) continue;
            float factor = cls.GetModifierInheritance(source).damageInheritance;
            result = result.CombineWith(GetDamage(source).Scale(factor));
        }
        return result;
    }

    public float GetTotalCritChance(DamageClass cls)
    {
        float result = GetCritChance(cls);
        foreach (DamageClass source in DamageClassLoader.All)
        {
            if (ReferenceEquals(source, cls)) continue;
            result += GetCritChance(source) * cls.GetModifierInheritance(source).critChanceInheritance;
        }
        return result;
    }

    public float GetTotalAttackSpeed(DamageClass cls)
    {
        float result = GetAttackSpeed(cls);
        foreach (DamageClass source in DamageClassLoader.All)
        {
            if (ReferenceEquals(source, cls)) continue;
            // A velocidade neutra é 1; somente o excedente herda.
            result += (GetAttackSpeed(source) - 1f) *
                cls.GetModifierInheritance(source).attackSpeedInheritance;
        }
        return result;
    }

    public float GetTotalArmorPenetration(DamageClass cls)
    {
        float result = GetArmorPenetration(cls);
        foreach (DamageClass source in DamageClassLoader.All)
        {
            if (ReferenceEquals(source, cls)) continue;
            result += GetArmorPenetration(source) *
                cls.GetModifierInheritance(source).armorPenInheritance;
        }
        return result;
    }

    public StatModifier GetTotalKnockback(DamageClass cls)
    {
        StatModifier result = GetKnockback(cls);
        foreach (DamageClass source in DamageClassLoader.All)
        {
            if (ReferenceEquals(source, cls)) continue;
            result = result.CombineWith(GetKnockback(source).Scale(
                cls.GetModifierInheritance(source).knockbackInheritance));
        }
        return result;
    }

    public StatModifier GetTotalDamage<T>() where T : DamageClass => GetTotalDamage(DamageClassLoader.GetInstance<T>());
    public StatModifier GetTotalKnockback<T>() where T : DamageClass => GetTotalKnockback(DamageClassLoader.GetInstance<T>());
    public float GetTotalCritChance<T>() where T : DamageClass => GetTotalCritChance(DamageClassLoader.GetInstance<T>());
    public float GetTotalAttackSpeed<T>() where T : DamageClass => GetTotalAttackSpeed(DamageClassLoader.GetInstance<T>());
    public float GetTotalArmorPenetration<T>() where T : DamageClass => GetTotalArmorPenetration(DamageClassLoader.GetInstance<T>());
}
