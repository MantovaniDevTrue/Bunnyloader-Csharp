namespace BunnyLoader.Source;

/// <summary>
/// Frações de herança de uma classe de dano, conforme o contrato do tModLoader.
/// Full herda 100%; None não herda nenhuma estatística. Cada campo é independente.
/// Esta estrutura define apenas os fatores; o loader de DamageClass precisa
/// consumi-los explicitamente para afetar o Terraria.
/// </summary>
/// <remarks>
/// Adaptado de tModLoader (MIT):
/// https://github.com/tModLoader/tModLoader/blob/stable/patches/tModLoader/Terraria/ModLoader/StatInheritanceData.cs
/// </remarks>
public struct StatInheritanceData
{
    public static readonly StatInheritanceData Full = new(1f, 1f, 1f, 1f, 1f);
    public static readonly StatInheritanceData None = new(0f, 0f, 0f, 0f, 0f);

    public float damageInheritance;
    public float critChanceInheritance;
    public float attackSpeedInheritance;
    public float armorPenInheritance;
    public float knockbackInheritance;

    /// <summary>
    /// Exemplo: new StatInheritanceData(damageInheritance: 1f,
    /// critChanceInheritance: 0.5f) herda todo o dano e metade do crítico.
    /// Os demais campos permanecem em zero.
    /// </summary>
    public StatInheritanceData(
        float damageInheritance = 0f,
        float critChanceInheritance = 0f,
        float attackSpeedInheritance = 0f,
        float armorPenInheritance = 0f,
        float knockbackInheritance = 0f)
    {
        this.damageInheritance = damageInheritance;
        this.critChanceInheritance = critChanceInheritance;
        this.attackSpeedInheritance = attackSpeedInheritance;
        this.armorPenInheritance = armorPenInheritance;
        this.knockbackInheritance = knockbackInheritance;
    }
}
