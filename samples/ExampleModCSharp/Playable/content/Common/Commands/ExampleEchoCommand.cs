using BunnyLoader.Source;

namespace ExampleModCSharp.Commands;

/// <summary>
/// Baseado no padrao de classe do ExampleModBunny (HealCommand e ExampleTimeCommand).
/// Exemplo carregavel na fase de validacao Android do CommandLoader C#.
/// /echo texto ou /dizer texto responde no chat sem alterar o mundo.
/// </summary>
public sealed class ExampleEchoCommand : BunnyCommand
{
    public override string Command => "echo";
    public override System.Collections.Generic.IReadOnlyList<string> Aliases => new[] { "dizer" };
    public override string Usage => "/echo <texto>";
    public override string Description => "Responde no chat com o texto digitado.";

    public override void Action(BunnyCommandContext context)
    {
        // O acesso ao jogo e feito pelo host; o MonoVM trabalha com snapshots sem ponte JS.
        if (context.Arguments.Count == 0)
            throw new BunnyUsageException("Uso: " + Usage);
        context.Caller.Reply(string.Join(" ", context.Arguments), 124, 252, 0);
    }
}
