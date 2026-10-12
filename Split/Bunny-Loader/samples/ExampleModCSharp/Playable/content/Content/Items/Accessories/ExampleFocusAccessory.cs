using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Focus Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Concede +7 de defesa enquanto equipado";
    }
}
