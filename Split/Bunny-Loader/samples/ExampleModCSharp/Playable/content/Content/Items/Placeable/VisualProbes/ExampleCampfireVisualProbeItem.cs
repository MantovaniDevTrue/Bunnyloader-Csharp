using BunnyLoader.Source;
namespace ExampleModCSharp.Items.Placeable.VisualProbes;
public sealed class ExampleCampfireVisualProbeItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Items/Placeable/VisualProbes/ExampleCampfireVisualProbeItem.png";
        DisplayName="Example Campfire Visual Probe (1x1)";
        Tooltip="C# flame and dust test; not the original 3x2 campfire.";
        DefaultToPlaceableTile("ExampleCampfireVisualProbe");
        Width=30;Height=18;
    }
    public override void AddRecipes()
    {
        CreateRecipe(1).AddIngredient<ExampleModCSharp.ExampleItem>(1)
            .AddTile(18).Register();
    }
}
