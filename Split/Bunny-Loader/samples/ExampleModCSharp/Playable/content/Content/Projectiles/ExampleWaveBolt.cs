// ExampleMod C# | Content/Projectiles/ExampleWaveBolt.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 2;
        TimeLeft = 180;
    }

    public override bool AI(ref BunnyProjectileAiState state)
    {
        float vx = state.VelocityX;
        float vy = state.VelocityY;
        if (!float.IsFinite(vx) || !float.IsFinite(vy)) return false;
        float turn = MathF.Sin(state.Age * 0.12f) * 0.018f;
        float sine = MathF.Sin(turn);
        float cosine = MathF.Cos(turn);
        state.VelocityX = vx * cosine - vy * sine;
        state.VelocityY = vx * sine + vy * cosine;
        return true;
    }
}

