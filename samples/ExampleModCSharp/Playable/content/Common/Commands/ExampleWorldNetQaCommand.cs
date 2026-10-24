using BunnyLoader.Source;

namespace ExampleModCSharp.Commands;

public sealed class ExampleWorldNetQaCommand : BunnyCommand
{
    public override string Command => "worldnetqa";
    public override BunnyCommandType Type =>
        BunnyCommandType.Chat | BunnyCommandType.Server;
    public override string Usage => "/worldnetqa [mark]";
    public override string Description => "Verifica progressao C# sincronizada entre host e cliente.";

    public override void Action(BunnyCommandContext context)
    {
        if (context.Arguments.Count == 0)
        {
            context.Caller.Reply("WorldNetQA: boss=" +
                ExampleWorldProgressSystem.DownedExampleBoss +
                " frames=" + ExampleWorldProgressSystem.WorldFrames +
                " mode=" + context.Caller.NetworkMode);
            return;
        }

        if (context.Arguments.Count != 1 || context.Arguments[0] != "mark")
            throw new BunnyUsageException("Uso: " + Usage);
        if (context.Caller.NetworkMode != 0 && context.Caller.NetworkMode != 2)
        {
            context.Caller.Reply("WorldNetQA: alteracao permitida somente no host.");
            return;
        }

        ExampleWorldProgressSystem.MarkBossDefeated();
        context.Caller.Reply("WorldNetQA: boss=True (host).");
    }
}
