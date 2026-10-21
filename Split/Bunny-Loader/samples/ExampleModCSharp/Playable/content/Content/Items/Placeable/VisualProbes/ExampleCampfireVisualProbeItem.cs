using BunnyLoader.Source;
namespace ExampleModCSharp.Items.Placeable.VisualProbes;
public sealed class ExampleCampfireVisualProbeItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Items/Placeable/VisualProbes/ExampleCampfireVisualProbeItem.png";
        DisplayName="Example Campfire Visual Probe (3x2)";
        Tooltip="3x2 C# drawing and dust test; wire toggle and campfire buff pending.";
        DefaultToPlaceableTile("ExampleCampfireVisualProbe");
        Width=30;Height=18;
    }
    public override void AddRecipes()
    {
        CreateRecipe(1).AddIngredient<ExampleModCSharp.ExampleItem>(1)
            .AddTile(18).Register();
    }
}
