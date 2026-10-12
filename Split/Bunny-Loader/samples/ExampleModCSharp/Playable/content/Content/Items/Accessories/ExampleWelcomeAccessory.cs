using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWelcomeAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Welcome Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
    }
}
