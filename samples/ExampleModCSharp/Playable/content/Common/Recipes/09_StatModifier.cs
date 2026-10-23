using BunnyLoader.Source;

namespace ExampleModCSharp.Recipes;

/// <summary>
/// Exemplo fiel das contas do tModLoader, sem dependência de JavaScript.
/// Esta classe é um exemplo matemático, NÃO é registrada como hook automático.
/// Para aplicar num combate real, chame o método no hook correspondente
/// e atribua o resultado ao campo modificável fornecido pelo Bunny.
/// </summary>
public static class ExampleStatModifierRecipe
{
    public static float CalculateDamage(float originalDamage)
    {
        StatModifier modifier = StatModifier.Default;
        modifier += 0.12f; // Bônus aditivo de 12%, comum em acessórios.
        modifier *= 1.10f; // Multiplicador separado de 10%, aplicado depois.
        modifier.Base += 2f; // Adicionado ao dano base, antes dos multiplicadores.
        modifier.Flat += 3f; // Adicionado por último, sem ser multiplicado.
        return modifier.ApplyTo(originalDamage);
    }

    public static float CalculateInheritedDamage(float originalDamage)
    {
        StatModifier melee = StatModifier.Default + 0.10f;
        StatModifier generic = StatModifier.Default + 0.20f;
        StatInheritanceData inheritance = StatInheritanceData.Full;

        // DamageClass usa Scale para herdar somente a porcentagem configurada.
        // CombineWith mantém aditivos somados: 10% + 20% = 30%, não 32%.
        StatModifier total = melee.CombineWith(generic.Scale(inheritance.damageInheritance));
        return total.ApplyTo(originalDamage);
    }

    /// <summary>
    /// Exemplo do fator de herança de velocidade de ataque, sem acessar o jogo.
    /// Na API original, GetTotalAttackSpeed parte do valor da classe (1 = 100%)
    /// e soma (velocidadeDaOrigem - 1) * attackSpeedInheritance.
    /// Use isto como referência matemática no hook real, não como um hook pronto.
    /// </summary>
    public static float CalculateInheritedAttackSpeed(float ownSpeed, float inheritedClassSpeed)
    {
        StatInheritanceData inheritance = new(attackSpeedInheritance: 0.5f);

        // Herda metade da diferença em relação a 1, não metade da velocidade total.
        // Exemplo: classe própria 1.05; origem 1.20 => 1.05 + 0.10 = 1.15.
        return ownSpeed + (inheritedClassSpeed - 1f) * inheritance.attackSpeedInheritance;
    }
}
