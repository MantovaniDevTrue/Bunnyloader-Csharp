using BunnyLoader.Source;

namespace DeclaredContentCSharp;

[BunnyContent(BunnyContentKind.Item, "Declared Material", "Content/Items/DeclaredMaterial.png")]
public sealed class DeclaredMaterial : SharedMaterial<int>
{
    public override void SetDefaults()
    {
        base.SetDefaults();
        Rare = 1;
        Tooltip = "Metadata in the attribute; reusable defaults in C#.";
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2, 1).Register();
    }
}
