using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTrailblazerBootsAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Trailblazer Boots";
        Texture = "Content/Items/Accessories/ExampleTrailblazerBootsAccessory.png";
        DefenseBonus = 0;
        // RunningTrail veste a folha Shoes e usa chamas NATIVAS do Terraria.
        // Correr no chao (|velocidade X| > 1) chama a rotina de chamas por tile;
        // parado, pulando, montado ou com visual oculto nao deixa o rastro.
        // A textura usa os quadros do jogador; nao e uma imagem repetida no ar.
        VisualMode = BunnyAccessoryVisualMode.RunningTrail;
        VisualTexture = "Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png";
        VisualScale = 1f;
        VisualColor = 0xFFFF9632;
        VisualOpacity = 0.85f;
        Tooltip = "+12% velocidade, +8% aceleracao";
    }
}

