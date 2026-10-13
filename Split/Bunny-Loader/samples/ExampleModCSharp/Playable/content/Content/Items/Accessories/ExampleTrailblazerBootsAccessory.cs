using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTrailblazerBootsAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Trailblazer Boots";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "+12% velocidade, +8% aceleracao; efeito visual ao equipar";
    }
}
