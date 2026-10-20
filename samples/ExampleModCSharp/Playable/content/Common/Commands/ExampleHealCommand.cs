using BunnyLoader.Source;
using System;
using System.Collections.Generic;
using System.Globalization;

namespace ExampleModCSharp.Commands;

/// <summary>
/// /heal quantidade ou /curar quantidade, seguindo HealCommand.js do
/// ExampleModBunny. Action roda no MonoVM; o C++ atualiza statLife
/// e chama Player.HealEffect na thread do Terraria.
/// </summary>
public sealed class ExampleHealCommand : BunnyCommand
{
    public override string Command => "heal";
    public override IReadOnlyList<string> Aliases => new[] { "curar" };
    public override BunnyCommandType Type => BunnyCommandType.Chat;
    public override string Usage => "/heal <quantidade>";
    public override string Description => "Cura a vida do jogador.";

    public override void Action(BunnyCommandContext context)
    {
        if (context.Arguments.Count == 0 ||
            !double.TryParse(context.Arguments[0], NumberStyles.Float,
                             CultureInfo.InvariantCulture, out double raw) ||
            !double.IsFinite(raw))
            throw new BunnyUsageException("Informe uma quantidade positiva");

        double amount = Math.Floor(raw);
        if (amount <= 0)
            throw new BunnyUsageException("Informe uma quantidade positiva");

        // O jogo limita a cura pela vida maxima. Limitar o pedido nativo
        // evita overflow sem reduzir a cura efetiva do comando original.
        context.Caller.Heal((int)Math.Min(amount, 1000000.0));
    }
}
