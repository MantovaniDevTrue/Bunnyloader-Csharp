using BunnyLoader.Source;
namespace ExampleModCSharp;

[BunnyName("en-US", "Example Bullet")]
[BunnyName("pt-BR", "Example Bala")]
public sealed class ExampleBullet : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Bullet";
        Texture = "Content/Items/Ammo/ExampleBullet.png";
        Damage = 12; MaxStack = 9999; Consumable = true;
        KnockBack = 1; Value = SellPrice(silver: 10); Rare = 2;
        // AmmoID.Bullet é resolvido no Android; não é o ID do nosso item.
        // O jogo faz a seleção, soma o dano e consome a munição normalmente.
        NativeDefaults.SetBool("ranged", true)
            .SetId("ammo", "AmmoID", "Bullet")
            .SetProjectile("shoot", nameof(ExampleBulletProjectile))
            .SetFloat("shootSpeed", 4.5f);
    }
}
