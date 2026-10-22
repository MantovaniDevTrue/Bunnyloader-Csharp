using Terraria;
using Terraria.Utilities;

namespace ExampleModCSharp.Common.Random;

/// <summary>
/// Exemplo no estilo do tModLoader: reaproveite o gerador ao longo do combate.
/// Não crie um UnifiedRandom novo a cada frame, nem use este gerador como Main.rand.
/// </summary>
public static class ExampleWeightedChoice
{
    private static readonly WeightedRandom<int> LootTable = new(2026,
        System.Tuple.Create(2, 7.0),
        System.Tuple.Create(3, 2.0),
        System.Tuple.Create(4, 1.0));

    /// <summary>
    /// Retorna um tipo de item vanilla. O código que faz o drop deve usar
    /// a API real de criação de itens da própria versão do Bunny Loader.
    /// Peso 7 significa sete vezes a chance de uma entrada com peso 1.
    /// </summary>
    public static int ChooseDropItemType() => LootTable.Get();

    /// <summary>Escolhe uma fase entre três, mantendo seed reprodutível.</summary>
    public static int ChooseAiPhase(int worldSeed)
    {
        var random = new UnifiedRandom(worldSeed);
        if (random.NextBool(6)) return 2;
        return random.NextFromList(0, 1);
    }
}
