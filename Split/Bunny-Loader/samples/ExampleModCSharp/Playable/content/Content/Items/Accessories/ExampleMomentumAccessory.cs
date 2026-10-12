using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMomentumAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Momentum Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Aumenta a aceleracao ao correr em 6%";
    }
}
