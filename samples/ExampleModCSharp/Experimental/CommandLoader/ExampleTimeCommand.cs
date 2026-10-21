using BunnyLoader.Source;
using System;
using System.Globalization;

namespace ExampleModCSharp.Commands;

/// <summary>
/// /addTime ticks: ciclo do ExampleTimeCommand.js do ExampleModBunny.
/// O pedido World so pode alterar o relogio no servidor ou no solo.
/// O C++ usa Main.dayTime/Main.time e transmite WorldData no multiplayer.
/// </summary>
public sealed class ExampleTimeCommand : BunnyCommand
{
    public override string Command => "addTime";
    public override BunnyCommandType Type => BunnyCommandType.World;
    public override string Usage => "/addTime <ticks>";
    public override string Description => "Avanca ou retrocede a hora do mundo.";

    public override void Action(BunnyCommandContext context)
    {
        if (context.Arguments.Count == 0)
            throw new BunnyUsageException("Informe a quantidade de ticks");
        if (!double.TryParse(context.Arguments[0], NumberStyles.Float,
                             CultureInfo.InvariantCulture, out double raw) ||
            !double.IsFinite(raw) || Math.Truncate(raw) != raw)
            throw new BunnyUsageException("Ticks precisam ser um numero inteiro");

        // So importa o deslocamento dentro do ciclo de 86400 ticks.
        // Assim, valores grandes nao estouram a API nativa int32.
        int delta = (int)(raw % 86400.0);
        context.Caller.AdvanceWorldTime(delta);
    }
}
