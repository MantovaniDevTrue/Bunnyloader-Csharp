using BunnyLoader.Source;

namespace ExampleModCSharp;

// RarityLoader mantém Type/ModId da revisão atual e consulta a outra raridade
// do mesmo UID. Não grave IDs numéricos: eles pertencem à sessão do loader.
public sealed class ExampleModRarity : BunnyRarity
{
    public override BunnyColor RarityColor => new(200, 215, 230);

    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset > 0
            ? BunnyRarityRuntime.TypeOf<ExampleHigherTierModRarity>(ModId)
            : Type;
}
