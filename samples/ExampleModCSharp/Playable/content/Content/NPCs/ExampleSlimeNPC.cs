using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSlimeNPC : BunnyNPC
{
    public override void SetDefaults()
    {
        DisplayName = "Example Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 32;
        Height = 24;
        Damage = 7;
        Defense = 2;
        LifeMax = 25;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
    }

    public override bool AI(ref BunnyNpcAiState state)
    {
        float direction = state.PlayerX - state.X;
        if (!float.IsFinite(direction) || MathF.Abs(direction) < 12f)
            return false;
        state.VelocityX = Math.Clamp(state.VelocityX +
            (direction > 0f ? 0.055f : -0.055f), -3f, 3f);
        return true;
    }

    public static int HitEvents;
    public static int KillEvents;

    public override void OnHit(ref BunnyNpcAiState state, int damage)
    {
        if (damage > 0) HitEvents++;
    }

    public override void OnKill(ref BunnyNpcAiState state)
    {
        KillEvents++;
    }

}

