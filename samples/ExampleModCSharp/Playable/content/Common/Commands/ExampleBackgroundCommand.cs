using BunnyLoader.Source;
using ExampleModCSharp.Backgrounds;

namespace ExampleModCSharp.Commands;

public sealed class ExampleBackgroundCommand : BunnyCommand
{
    public override string Command => "examplebackground";
    public override string Usage => "/examplebackground [caminho]";
    public override string Description => "Example: consulta o slot nativo e verifica o asset de fundo carregado.";
    // Caminhos de assets preservam maiusculas/minusculas no Android.
    public override bool IsCaseSensitive => true;

    public override void Action(BunnyCommandContext context)
    {
        // ModId vem do registro do comando: funciona com outro UID, em outro
        // mod e com uma ordem diferente de carregamento, sem editar o nucleo.
        if (context.Arguments.Count == 0)
        {
            int slot = BunnyBackgroundTextures.GetSlot<ExampleBackgroundTexture>(context.ModId);
            context.Caller.Reply("Example background: asset nativo pronto, slot temporario " + slot);
            return;
        }
        if (BunnyBackgroundTextures.TryGetSlot(context.ModId, context.Arguments[0], out int found))
            context.Caller.Reply("Example background: asset nativo pronto, slot temporario " + found);
        else
            context.Caller.Reply("Example background: caminho ausente ou textura ainda nao publicada.", 255, 100, 100);
    }
}
