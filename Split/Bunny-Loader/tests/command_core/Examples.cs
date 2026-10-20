using System;
using System.Collections.Generic;
using System.Globalization;
using BunnyLoader.Source;

namespace BunnyLoader.CommandTests;

/// <summary>Porta de Common/Commands/HealCommand.js do ExampleModBunny.</summary>
public sealed class ExampleHealCommand : BunnyCommand
{
    public override string Command => "heal";
    public override IReadOnlyList<string> Aliases => new[] { "curar" };
    public override BunnyCommandType Type => BunnyCommandType.Chat;
    public override string Usage => "/heal <quantidade>";
    public override string Description => "Cura o jogador que digitou o comando.";

    public override bool Action(BunnyCommandCaller caller, string input, IReadOnlyList<string> args)
    {
        if (args.Count == 0 ||
            !double.TryParse(args[0], NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
            !double.IsFinite(value) || value < 1 || value > int.MaxValue)
            throw new BunnyUsageException(caller.Translate(
                "Commands.HealCommand.Error", "Informe uma quantidade positiva."));

        // A referência usa Math.floor(Number(args[0])) antes de curar;
        // manter a mesma regra para valores decimais.
        int amount = (int)Math.Floor(value);
        IBunnyCommandPlayer player = caller.Player
            ?? throw new BunnyUsageException("Este comando exige um jogador.");
        int missing = Math.Max(0, player.MaximumLife - player.Life);
        int healed = Math.Min(missing, amount);
        if (healed <= 0)
        {
            caller.Reply(caller.Translate("Commands.HealCommand.Full",
                "A vida já está cheia."));
            return true;
        }

        player.Life += healed;
        player.HealEffect(healed);
        caller.Reply(caller.Translate("Commands.HealCommand.Healed",
            "Recuperou {0} de vida.").Replace("{0}", healed.ToString(CultureInfo.InvariantCulture)),
            BunnyCommandColors.Success);
        return true;
    }
}

/// <summary>Porta de Common/Commands/ExampleTimeCommand.js do ExampleModBunny.</summary>
public sealed class ExampleTimeCommand : BunnyCommand
{
    public override string Command => "addTime";
    public override BunnyCommandType Type => BunnyCommandType.World;
    public override string Usage => "/addtime <ticks>";
    public override string Description => "Avança ou retrocede o relógio do mundo.";

    public override bool Action(BunnyCommandCaller caller, string input, IReadOnlyList<string> args)
    {
        if (args.Count == 0)
            throw new BunnyUsageException(caller.Translate(
                "Commands.Common.NoParameters", "Informe a quantidade de ticks."));
        if (!long.TryParse(args[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out long ticks))
            throw new BunnyUsageException(caller.Translate(
                "Commands.Common.NotInteger", "{0} não é um número inteiro.")
                .Replace("{0}", args[0]));

        IBunnyCommandWorld world = caller.World;
        var result = BunnyCommandClock.AddTicks(
            world.DayTime, world.Time, world.DayLength, world.NightLength, ticks);
        world.SetClock(result.IsDay, result.Time);

        // netMode 3 também é servidor. Nunca transmitir WorldData pelo cliente:
        // a mudança do relógio precisa vir da autoridade do mundo.
        if ((world.NetMode & 2) != 0) world.BroadcastWorldData();
        return true;
    }
}
