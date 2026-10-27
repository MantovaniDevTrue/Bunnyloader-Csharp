using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMysticWardAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Mystic Ward";
        Texture = "Content/Items/Accessories/ExampleMysticWardAccessory.png";
        DefenseBonus = 0;
        VisualMode = BunnyAccessoryVisualMode.EnergyBarrier;
        VisualTexture = "Content/Effects/MysticWardBarrier.png";
        VisualScale = 1.1f;
        VisualColor = 0xFF66CCFF;
        VisualOpacity = 0.78f;
        Tooltip = "+3 defesa, +8 regeneracao de mana; protecao com pouca vida e contra projeteis proximos";
    }
}
