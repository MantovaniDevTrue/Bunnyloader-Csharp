using BunnyLoader.Source;

namespace ExampleModCSharp;

// ExampleCampfire.js: o item esta completo; a logica do tile permanece
// isolada ate a portabilidade de NearbyEffects e emissao de luz nativa.
public sealed class ExampleCampfire : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Campfire";
        Texture = "Content/Items/Placeable/ExampleCampfire.png";
        DefaultToPlaceableTile("ExampleCampfire");
    }

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddNativeRecipeGroup("Wood", 10)
            .AddIngredient("ExampleTorch", 5)
            .Register();
    }
}
