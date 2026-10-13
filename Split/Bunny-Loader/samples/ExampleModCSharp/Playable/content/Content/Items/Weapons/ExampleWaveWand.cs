// ExampleMod C# | Content/Items/Weapons/ExampleWaveWand.cs
// Arma: Shoot usa nameof da classe de projetil; Texture e um caminho dentro de content/.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 12;
        UseTime = 16;
        ShootSpeed = 9f;
        Shoot = nameof(ExampleWaveBolt);
    }
}

