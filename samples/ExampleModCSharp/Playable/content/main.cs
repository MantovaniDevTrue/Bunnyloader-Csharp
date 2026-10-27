// ExampleMod C# | main.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;
using System;

namespace ExampleModCSharp;

public sealed class ExampleBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Bolt";
        Texture = "Content/Projectiles/ExampleBolt.png";
        Width = 12;
        Height = 12;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 240;
    }

    public int OnHitBuffId() => 24;

    public override int OnHitNPC(int damage)
    {
        return Math.Clamp(45 + damage * 2, 45, 130);
    }

    public int OnKillBuffId() => 24;

    public override int OnKill(int damage)
    {
        return Math.Clamp(28 + damage, 28, 96);
    }

    public override int OnKillDuration(int damage)
    {
        return Math.Clamp(50 + damage * 2, 50, 150);
    }
}

