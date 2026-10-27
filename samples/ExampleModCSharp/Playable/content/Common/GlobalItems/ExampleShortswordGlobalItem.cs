// ExampleMod C# | Common/GlobalItems/ExampleShortswordGlobalItem.cs
// Porte do ShortswordGlobalItem do ExampleMod do tModLoader: muda um item vanilla.
// O Bunny avalia AppliesToEntity/SetDefaults uma vez sobre a foto dos itens vanilla
// e aplica o resultado no SetDefaults nativo do jogo. O Shoot roda no momento do
// uso, pelo mesmo caminho das armas C# que atiram.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShortswordGlobalItem : BunnyGlobalItem
{
    private const int CopperShortsword = 3507; // ItemID.CopperShortsword
    private const int Grenade = 30;            // ProjectileID.Grenade

    public override bool AppliesToEntity(BunnyVanillaItem item) => item.Type == CopperShortsword;

    public override void SetDefaults(BunnyVanillaItem item)
    {
        item.Damage = 50; // Como no tModLoader: dano 50.
    }

    public override bool Shoot(BunnyShootContext context, BunnyShot shot)
    {
        // Atira granadas sem motivo, como no original: do centro do jogador
        // (a posição do disparo vanilla) e com 5x a velocidade.
        context.Spawn(new BunnyShot
        {
            Type = Grenade,
            X = shot.X,
            Y = shot.Y,
            VelocityX = shot.VelocityX * 5f,
            VelocityY = shot.VelocityY * 5f,
            Damage = shot.Damage,
            KnockBack = shot.KnockBack,
        });
        // false impede o disparo vanilla: aqui, a estocada da espada curta,
        // que também é um projétil.
        return false;
    }
}
