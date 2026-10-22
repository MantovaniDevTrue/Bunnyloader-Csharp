using BunnyLoader.Source;

namespace ExampleModCSharp.DamageClasses;

/// <summary>
/// Classe de dano equivalente ao ExampleDamageClass do ExampleMod do tModLoader.
/// Os bônus Generic são herdados integralmente; efeitos melee/magic são
/// reconhecidos, mas isso NÃO faz seus bônus numéricos serem herdados.
/// </summary>
/// <remarks>
/// Esta classe serve para o compilador real C# e testes. A descoberta automática
/// de DamageClass e os hooks Terraria.Player ainda exigem o dispatcher nativo
/// do Bunny Loader. Não registrar classes manualmente no loop de jogo.
/// </remarks>
public sealed class ExampleDamageClass : DamageClass
{
    public override StatInheritanceData GetModifierInheritance(DamageClass damageClass)
    {
        if (ReferenceEquals(damageClass, Generic))
            return StatInheritanceData.Full;
        return StatInheritanceData.None;
    }

    public override bool GetEffectInheritance(DamageClass damageClass) =>
        ReferenceEquals(damageClass, Melee) || ReferenceEquals(damageClass, Magic);

    public override void SetDefaultStats(DamageClassPlayer player)
    {
        // Os métodos retornam ref: += altera diretamente o slot da classe.
        player.GetCritChance(this) += 4f;
        player.GetArmorPenetration(this) += 10f;
    }

    public override bool UseStandardCritCalcs => true;

    public override bool ShowStatTooltipLine(DamageClassPlayer player, string lineName) =>
        lineName != "Speed";
}
