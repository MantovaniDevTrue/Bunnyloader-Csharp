using BunnyLoader.Source;
namespace ExampleModCSharp;

// Sem IA artificial: aiStyle=1 executa a física e a rotação nativas de bala.
[BunnyName("en-US", "Example Bullet")]
[BunnyName("pt-BR", "Example Bala")]
public sealed class ExampleBulletProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example Bullet";
        Texture = "Content/Projectiles/ExampleBulletProjectile.png";
        Width = 8; Height = 8; AiStyle = 1; Friendly = true;
        Penetrate = 5; TimeLeft = 600; IgnoreWater = true; TileCollide = true;
        // extraUpdates inclui subatualizações nativas, não callbacks C# extras.
        // Imunidade local -1 impede acertar repetidamente o mesmo NPC com esta bala.
        NativeDefaults.SetFloat("scale", 1).SetFloat("light", .5f)
            .SetBool("ranged", true).SetBool("hostile", false)
            .SetInt("extraUpdates", 1).SetBool("usesLocalNPCImmunity", true)
            .SetInt("localNPCHitCooldown", -1);
    }
}
