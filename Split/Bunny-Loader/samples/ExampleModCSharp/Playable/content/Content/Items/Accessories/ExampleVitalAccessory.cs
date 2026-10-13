using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Vital Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Aumenta a regeneracao de vida em 12 enquanto equipado";
    }
}
