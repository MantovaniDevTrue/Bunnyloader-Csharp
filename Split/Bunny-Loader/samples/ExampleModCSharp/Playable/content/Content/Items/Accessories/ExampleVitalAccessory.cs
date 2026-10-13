using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Vital Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Regenera +12 vida; reducao adicional contra NPC e projetil; esquiva no quarto golpe";
    }
}
