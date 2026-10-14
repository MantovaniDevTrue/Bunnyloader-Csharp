using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGuardianShieldAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Guardian Shield";
        Texture = "Content/Items/Accessories/ExampleGuardianShieldAccessory.png";
        DefenseBonus = 0;
        // EquippedSprite registra um slot Shield nativo. O Terraria escolhe
        // o quadro de caminhada/salto e espelha a folha conforme a direcao.
        // So aparece equipado e visivel; ter o item na mochila nao basta.
        VisualMode = BunnyAccessoryVisualMode.EquippedSprite;
        VisualTexture = "Content/Items/Accessories/ExampleGuardianShieldAccessory_Shield.png";
        VisualScale = 1.15f;
        VisualColor = 0xFFFFFFFF;
        VisualOpacity = 1f;
        Tooltip = "+5 defesa, +4 regeneracao de vida; reduz dano de NPCs e projeteis";
    }
}

