using BunnyLoader.Source;

namespace ExampleModCSharp;

/// <summary>
/// Tradução do projétil de golfe do Bunny JS. A IA 149 é GolfBall no Terraria
/// Android; não há simulação C# aproximada nem callback JS por atualização.
/// </summary>
[BunnyName("en-US", "Example Golf Ball")]
[BunnyName("pt-BR", "Bola de Golfe de Exemplo")]
public sealed class ExampleGolfBallProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example Golf Ball";
        Texture = "Content/Projectiles/ExampleGolfBallProjectile.png";
        Width = 7; Height = 7;
        AiStyle = 149;
        Friendly = true; Penetrate = -1; TileCollide = false;
        // São os defaults do Projectile antes do SetDefaults do mod JS.
        // O SDK tem defaults próprios para outros exemplos, por isso explicitamos.
        TimeLeft = 3600; IgnoreWater = false;
        NativeDefaults.SetBool("netImportant", true).SetBool("ranged", false);
    }

    public override void SetStaticDefaults()
    {
        // O host aplica a política na thread do jogo e antes do SetDefaults
        // nativo, que aloca o histórico de vinte posições e rotações.
        NativeDefaults.SetProjectileStaticBool("IsAGolfBall", true)
            .SetProjectileStaticInt("TrailingMode", 0)
            .SetProjectileStaticInt("TrailCacheLength", 20);
    }
}
