using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Item";
        Tooltip = "Item description";
        TooltipTranslations["pt-BR"] = "Descrição do item";
        Texture = "Content/Items/ExampleItem.png";
        MaxStack = 9999;
        Value = 100;
    }

    public override void AddRecipes()
    {
        CreateRecipe(999)
            .AddIngredient(BunnyVanillaItems.DirtBlock, 10)
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
