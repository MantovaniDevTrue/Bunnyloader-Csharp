using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

namespace ExampleModCSharp.Commands;

// Testa exclusivamente as imagens e animações do ExampleMod. Este comando não
// substitui o futuro ExampleSurfaceBiome nem simula sua detecção por tiles.
public sealed class ExampleSurfaceBackgroundCommand : BunnyCommand
{
    public override string Command=>"examplesurface";
    public override string Usage=>"/examplesurface on|off";
    public override string Description=>"Example: testa as três camadas do fundo de superfície C#.";
    public override void Action(BunnyCommandContext context)
    {
        if(context.Arguments.Count!=1 || (context.Arguments[0]!="on" && context.Arguments[0]!="off"))
        {context.Caller.Reply(Usage);return;}
        if(context.Arguments[0]=="on") {
            BunnySurfaceBackgrounds.Activate<ExampleSurfaceBackgroundStyle>(context.ModId,BunnyBackgroundPriority.BiomeHigh);
            context.Caller.Reply("Example surface: ativo. Na superfície, observe longe, meio com quatro frames e frente com parallax.");
        }else {
            BunnySurfaceBackgrounds.Deactivate<ExampleSurfaceBackgroundStyle>(context.ModId);
            context.Caller.Reply("Example surface: desativado; as camadas anteriores voltam por transição.");
        }
    }
}
