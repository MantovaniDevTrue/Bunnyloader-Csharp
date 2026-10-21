using BunnyLoader.Source;
using System.IO;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo original inspirado em Common/Systems/DownedBossSystem.js do
/// ExampleModBunny. A ligação automática do runtime ainda não está habilitada.
/// </summary>
public sealed class ExampleDownedBossWorldSystem : BunnyWorldSystem
{
    public bool DownedExampleBoss { get; private set; }

    // Um NPC C# chamará este método apenas quando o servidor confirmar a morte.
    public void MarkExampleBossDefeated() => DownedExampleBoss = true;

    public override void ClearWorld()
    {
        // Um mundo novo nunca herda as flags do mundo anterior.
        DownedExampleBoss = false;
    }

    public override void SaveWorldData(BunnyWorldTag tag)
    {
        // Como no exemplo JS original, a chave só existe após a vitória.
        if (DownedExampleBoss) tag.SetBoolean("downedExampleBoss", true);
    }

    public override void LoadWorldData(BunnyWorldTag tag)
    {
        // Em mundos novos, ContainsKey devolve false sem gerar exceção.
        DownedExampleBoss = tag.ContainsKey("downedExampleBoss") &&
                            tag.GetBoolean("downedExampleBoss");
    }

    public override void NetSend(BinaryWriter writer)
    {
        // Equivale à flag enviada pelo DownedBossSystem do ExampleModBunny.
        writer.Write(DownedExampleBoss);
    }

    public override void NetReceive(BinaryReader reader)
    {
        // O cliente recebe o progresso do servidor, nunca grava o save local.
        DownedExampleBoss = reader.ReadBoolean();
    }
}
