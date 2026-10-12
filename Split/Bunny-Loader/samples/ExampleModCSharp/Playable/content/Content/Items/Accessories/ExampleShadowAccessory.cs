using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShadowAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Shadow Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
    }
}
