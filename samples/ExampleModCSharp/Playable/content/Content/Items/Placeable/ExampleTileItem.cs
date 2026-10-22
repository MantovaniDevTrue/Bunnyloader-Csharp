using BunnyLoader.Source;

namespace ExampleModCSharp;

// Porta do ModItem ExampleTileItem.js: a classe, nao um ID fixo,
// identifica o tile que sera resolvido pelo registro nativo compartilhado.
public sealed class ExampleTileItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Tile Item";
        Texture = "Content/Items/Placeable/ExampleTileItem.png";
        DefaultToPlaceableTile(nameof(ExampleTile));
        Width = 12;
        Height = 12;
    }

    public override void AddRecipes()
    {
        CreateRecipe(10)
            .AddIngredient<ExampleItem>()
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
