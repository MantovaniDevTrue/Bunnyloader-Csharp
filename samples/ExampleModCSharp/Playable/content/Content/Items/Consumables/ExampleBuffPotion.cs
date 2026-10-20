// ExampleMod C# | Content/Items/Consumables/ExampleBuffPotion.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Potion";
        Texture = "Content/Items/Consumables/ExampleBuffPotion.png";
        // O Mono executa este helper; o registro antecipado reserva apenas nomes, assets e o Buff.
        SetRarity<ExampleHigherTierModRarity>();
        Buff = nameof(ExampleDefenseBuff);
        Duration = 600;
    }
    public override void AddRecipes()
    {
        // A mesma API atende armas, acessórios e poções. O tipo resolve a bancada
        // C# deste mod; nenhum ID temporário do inventário é persistido na receita.
        CreateRecipe().AddIngredient<ExampleItem>(10)
            .AddTile<ExampleTiles.Furniture.ExampleWorkbench>()
            .AddBehavior(new Recipes.ExampleCraftingReward()).Register();
    }
}

