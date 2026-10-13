using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMysticWardAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Mystic Ward";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "+3 defesa, +8 regeneracao de mana; protecao com pouca vida e contra projeteis proximos";
    }
}
