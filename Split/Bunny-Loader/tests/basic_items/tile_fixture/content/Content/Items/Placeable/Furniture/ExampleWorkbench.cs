using BunnyLoader.Source;

namespace ExampleItems.Furniture;

public sealed class ExampleWorkbench : BunnyItem {
    public override void SetDefaults() {
        Texture = "Content/Items/Placeable/Furniture/ExampleWorkbench.png";
        DisplayName = "Example Workbench";
        DefaultToPlaceableTile("ExampleWorkbench");
        Width = 28;
        Height = 14;
        Value = 150;
    }

    public override void AddRecipes() {
        CreateRecipe()
            .AddIngredient(36)
            .AddIngredient("ExampleItem", 10)
            .Register();
    }
}
