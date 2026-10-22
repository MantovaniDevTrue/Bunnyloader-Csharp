using BunnyLoader.Source;

namespace ExampleModCSharp;

// ExampleLamp.js: o item continua com seus valores originais; o
// ExampleLamp (tile) sera registrado como categoria separada.
public sealed class ExampleLamp : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Lamp";
        Texture = "Content/Items/Placeable/ExampleLamp.png";
        DefaultToPlaceableTile("ExampleLamp");
        Width = 10;
        Height = 24;
        Value = 500;
    }

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddIngredient<ExampleItem>()
            .AddTile("ExampleWorkbench")
            .Register();
    }
}
