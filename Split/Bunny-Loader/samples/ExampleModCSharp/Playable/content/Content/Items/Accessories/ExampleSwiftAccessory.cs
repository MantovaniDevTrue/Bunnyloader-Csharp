using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSwiftAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Swift Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
    }
}
