using BunnyLoader.Source;

namespace ExampleModCSharp;

/// <summary>Tradução de ExampleGolfBall.js: a física continua no Terraria.</summary>
[BunnyName("en-US", "Example Golf Ball")]
[BunnyName("pt-BR", "Bola de Golfe de Exemplo")]
public sealed class ExampleGolfBall : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Golf Ball";
        Texture = "Content/Items/ExampleGolfBall.png";
        DefaultToGolfBall(nameof(ExampleGolfBallProjectile));
    }
}
