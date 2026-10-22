using System;
using BunnyLoader.Source;
using ExampleModCSharp.DamageClasses;

namespace ExampleModCSharp;

public sealed class ExampleWaveBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Bolt";
        DamageType = new ExampleDamageClass();
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

    public static int HitEvents;
    public static int KillEvents;

    public override void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage)
    {
        if (npcIndex >= 0 && damage >= 0)
        {
            HitEvents++;
            ExampleSoundEffects.PlayHit((int)state.X, (int)state.Y);
        }
    }

    public override void OnKill(ref BunnyProjectileAiState state)
    {
        KillEvents++;
    }

}

