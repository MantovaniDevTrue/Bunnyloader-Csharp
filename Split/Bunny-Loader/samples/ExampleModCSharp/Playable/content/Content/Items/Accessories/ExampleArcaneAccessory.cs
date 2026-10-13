using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleArcaneAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Arcane Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Aumenta a regeneracao de mana em 10 enquanto equipado";
    }
}
