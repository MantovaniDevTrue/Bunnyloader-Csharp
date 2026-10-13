using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGuardianShieldAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Guardian Shield";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "+5 defesa, +4 regeneracao de vida; reduz dano de NPCs e projeteis";
    }
}
