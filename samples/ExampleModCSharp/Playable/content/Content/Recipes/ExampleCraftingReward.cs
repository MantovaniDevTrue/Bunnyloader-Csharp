using BunnyLoader.Source;

namespace ExampleModCSharp.Recipes;

// Compartilhado por receitas de categorias diferentes, sem alterar o núcleo.
public sealed class ExampleCraftingReward : BunnyRecipeBehavior
{
    public override void OnCraft(BunnyCraftContext context)
    {
        // O snapshot não expõe ponteiros. A bridge concede o material na thread
        // do Terraria, depois de fabricar, somente se este callback terminar bem.
        context.GiveItem<ExampleItem>();
    }
}
