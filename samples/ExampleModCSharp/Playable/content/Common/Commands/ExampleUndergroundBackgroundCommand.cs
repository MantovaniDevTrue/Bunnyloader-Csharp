using BunnyLoader.Source;
using ExampleModCSharp.Biomes;

namespace ExampleModCSharp.Commands;

// Comando de QA do loader: testa as imagens do ExampleMod sem exigir que o
// loader de biomas, ainda pendente, esteja concluído. Não simula um bioma.
public sealed class ExampleUndergroundBackgroundCommand : BunnyCommand
{
    public override string Command=>"exampleunderground";
    public override string Usage=>"/exampleunderground on|off";
    public override string Description=>"Example: ativa ou desativa o fundo subterrâneo C# original.";
    public override void Action(BunnyCommandContext context)
    {
        if(context.Arguments.Count!=1 || (context.Arguments[0]!="on" && context.Arguments[0]!="off"))
        {context.Caller.Reply(Usage);return;}
        if(context.Arguments[0]=="on") {
            // BiomeHigh permite testar também sob neve/selva. Um bioma real
            // poderá publicar BiomeLow; o núcleo respeita a prioridade vanilla.
            BunnyUndergroundBackgrounds.Activate<ExampleUndergroundBackgroundStyle>(context.ModId,BunnyBackgroundPriority.BiomeHigh);
            context.Caller.Reply("Example underground: ativo. Desça ao subsolo para ver as quatro camadas originais.");
        }else {
            BunnyUndergroundBackgrounds.Deactivate<ExampleUndergroundBackgroundStyle>(context.ModId);
            context.Caller.Reply("Example underground: desativado; o Terraria executa a transição para o fundo anterior.");
        }
    }
}
