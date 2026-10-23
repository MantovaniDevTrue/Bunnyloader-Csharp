using System.Threading;
using BunnyLoader.Source;

namespace FiveLoadersCSharp;

public sealed class QaCommand : BunnyCommand
{
    public override string Command => "lote";
    public override string Usage => "/lote [add|tempo 1|tempo 2]";
    public override string Description => "Mostra os resultados dos cinco loaders de QA.";

    public override void Action(BunnyCommandContext context)
    {
        // Só single player neste teste: o mesmo mod contém NetSend/NetReceive
        // para o ensaio multiplayer, mas o cliente nunca altera o progresso.
        if (context.Arguments.Count > 0)
        {
            if (context.Caller.NetworkMode != 0)
                throw new BunnyUsageException("Altere o contador no single player deste QA.");
            if (context.Arguments[0] == "add") QaWorld.AddPress();
            else if (context.Arguments.Count == 2 && context.Arguments[0] == "tempo" &&
                     (context.Arguments[1] == "1" || context.Arguments[1] == "2"))
                Volatile.Write(ref QaWorld.ClockMultiplier, context.Arguments[1] == "2" ? 2 : 1);
            else throw new BunnyUsageException(Usage);
        }
        context.Caller.Reply("QA5: mundo=" + QaWorld.Active + " pontos=" + Volatile.Read(ref QaWorld.SavedPresses) + " saves=" + Volatile.Read(ref QaWorld.Saves));
        context.Caller.Reply("StatModifier: " + QaStats.Damage() + " (esperado 40); updates=" + QaUpdates.Observed + "/19");
        context.Caller.Reply("Callbacks ausentes:" + QaUpdates.Missing());
        context.Caller.Reply("Gore: pedidos=" + Volatile.Read(ref QaBolt.GoreQueued) + " recusados=" + Volatile.Read(ref QaBolt.GoreRejected));
        context.Caller.Reply("Toque: press=" + Volatile.Read(ref QaTouch.Presses) + " hold=" + Volatile.Read(ref QaTouch.Holds) + " duplo=" + Volatile.Read(ref QaTouch.DoubleTaps));
    }
}
