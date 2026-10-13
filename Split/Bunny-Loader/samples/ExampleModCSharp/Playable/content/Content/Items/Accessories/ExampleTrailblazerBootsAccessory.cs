using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTrailblazerBootsAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Trailblazer Boots";
        Texture = "Content/Items/Accessories/ExampleTrailblazerBootsAccessory.png";
        DefenseBonus = 0;
        VisualMode = BunnyAccessoryVisualMode.RunningTrail;
        VisualTexture = "Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png";
        VisualScale = 1f;
        VisualColor = 0xFFFF9632;
        VisualOpacity = 0.85f;
        Tooltip = "+12% velocidade, +8% aceleracao";
    }
}
