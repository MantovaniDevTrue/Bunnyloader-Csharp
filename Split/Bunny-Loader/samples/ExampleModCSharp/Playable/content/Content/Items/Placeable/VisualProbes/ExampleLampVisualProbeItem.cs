using BunnyLoader.Source;
namespace ExampleModCSharp.Items.Placeable.VisualProbes;
public sealed class ExampleLampVisualProbeItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Items/Placeable/VisualProbes/ExampleLampVisualProbeItem.png";
        DisplayName="Example Lamp Visual Probe (1x3)";
        Tooltip="1x3 C# special draw test; wire toggle pending.";
        DefaultToPlaceableTile("ExampleLampVisualProbe");
        Width=12;Height=34;
    }
    public override void AddRecipes()
    {
        CreateRecipe(1).AddIngredient<ExampleModCSharp.ExampleItem>(1)
            .AddTile(18).Register();
    }
}
