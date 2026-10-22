using BunnyLoader.Source;

namespace ExampleModCSharp;

// ExampleHerbSeeds.js: o nome do tile e resolvido no registro nativo.
// A germinacao e os tiles de solo precisam existir antes de ativar o item.
public sealed class ExampleHerbSeeds : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Herb Seeds";
        Texture = "Content/Items/Placeable/ExampleHerbSeeds.png";
        DefaultToPlaceableTile("ExampleHerb");
        Width = 12;
        Height = 14;
        Value = 80;
    }

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddIngredient<ExampleTileItem>()
            .Register();
    }
}
