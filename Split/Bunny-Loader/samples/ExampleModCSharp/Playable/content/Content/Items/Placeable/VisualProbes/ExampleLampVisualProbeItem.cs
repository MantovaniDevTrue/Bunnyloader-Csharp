using BunnyLoader.Source;
namespace ExampleModCSharp.Items.Placeable.VisualProbes;
public sealed class ExampleLampVisualProbeItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Items/Placeable/VisualProbes/ExampleLampVisualProbeItem.png";
        DisplayName="Example Lamp Visual Probe (1x1)";
        Tooltip="C# special drawing and dust test; not the original 1x3 lamp.";
        DefaultToPlaceableTile("ExampleLampVisualProbe");
        Width=12;Height=34;
    }
    public override void AddRecipes()
    {
        CreateRecipe(1).AddIngredient<ExampleModCSharp.ExampleItem>(1)
            .AddTile(18).Register();
    }
}
