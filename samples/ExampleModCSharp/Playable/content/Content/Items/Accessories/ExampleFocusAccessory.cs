// ExampleMod C# | Content/Items/Accessories/ExampleFocusAccessory.cs
// Acessorio: Tooltip descreve o efeito; o BunnyPlayer vinculado decide o bonus ao equipar.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Focus Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Concede +7 de defesa enquanto equipado";
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

