using BunnyLoader.Source;

namespace ExampleModCSharp;

// ExampleSinkItem.js: receita vanilla e dimensoes do original.
// O tile ExampleSink (com TileObjectData) ainda exige suporte proprio.
public sealed class ExampleSinkItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Sink";
        Texture = "Content/Items/Placeable/ExampleSinkItem.png";
        DefaultToPlaceableTile("ExampleSink");
        Width = 24;
        Height = 30;
        Value = 3000;
    }

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddIngredient<ExampleItem>()
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
