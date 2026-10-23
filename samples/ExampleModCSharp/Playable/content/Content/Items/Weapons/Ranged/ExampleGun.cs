using BunnyLoader.Source;
namespace ExampleModCSharp;

/// <summary>Port do ExampleGun.js: a munição escolhe o projétil; o som usa o áudio nativo Android.</summary>
[BunnyName("en-US", "Example Gun")]
[BunnyName("pt-BR", "Example Arma")]
public sealed class ExampleGun : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Gun";
        Texture = "Content/Items/Weapons/Ranged/ExampleGun.png";
        Damage = 20; KnockBack = 5; Crit = 0;
        UseTime = 10; UseAnimation = 10; UseStyle = 5;
        AutoReuse = true; NoMelee = true;
        Value = SellPrice(gold: 10); Rare = 2;
        // PurificationPowder é o fallback vanilla. A bala do inventário fornece
        // seu shoot/dano/velocidade ao disparo; não fabricamos munição na API.
        NativeDefaults.SetBool("ranged", true)
            .SetId("shoot", "ProjectileID", "PurificationPowder")
            .SetFloat("shootSpeed", 10)
            .SetId("useAmmo", "AmmoID", "Bullet");
        // Declarar shoot também sincroniza a direção com a mira antes do tiro nativo.
        Holdout = new BunnyHoldout(-18, 0);
        // O caminho tem extensão para eliminar ambiguidade de assets. O núcleo
        // carrega SoundPool e constrói LegacySoundStyle, sem executar JavaScript.
        // MaxInstances=3 evita sobreposição ilimitada; ReplaceOldest é o padrão original.
        CustomUseSound = new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",
            volume: 0.9f, pitchVariance: 0.2f, maxInstances: 3);
    }
}
