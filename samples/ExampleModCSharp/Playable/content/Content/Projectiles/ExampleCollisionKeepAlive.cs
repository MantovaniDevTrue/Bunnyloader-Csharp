using BunnyLoader.Source;
namespace ExampleModCSharp;

// QA singleplayer: política nativa imediata; callback C# é uma notificação posterior.
public sealed class ExampleCollisionKeepAlive : BunnyProjectile
{
    public static int Impacts { get; private set; }
    public override void SetDefaults()
    {
        DisplayName = "Example QA Collision KeepAlive";
        Texture = "Content/Projectiles/ExampleCollisionKeepAlive.png";
        Width = 8; Height = 8; AiStyle = 0; Friendly = true;
        Penetrate = -1; TimeLeft = 180; TileCollide = true;
        TileCollisionMode = BunnyProjectileTileCollisionMode.KeepAlive;
        TileCollisionSurvivalLimit = 3;
        TileCollisionBoxWidth = 6; TileCollisionBoxHeight = 6;
        TilePlatformCollision = BunnyProjectilePlatformCollision.SolidPlatforms;
    }
    public override void OnSpawn(ref BunnyProjectileAiState state) => Impacts = 0;
    public override void OnTileCollide(ref BunnyProjectileAiState state,
        float oldVelocityX, float oldVelocityY, float newVelocityX, float newVelocityY)
    {
        Impacts++;
    }
}
