using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items;

public sealed class ExampleItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/ExampleItem.png";
        DisplayName = "Example Item";
        MaxStack = 9999;
        Value = 100;
    }

    public override void AddRecipes()
    {
        CreateRecipe(999).AddIngredient(BunnyVanillaItems.DirtBlock, 10)
            .AddTile(BunnyVanillaTiles.WorkBenches).Register();
    }
}
