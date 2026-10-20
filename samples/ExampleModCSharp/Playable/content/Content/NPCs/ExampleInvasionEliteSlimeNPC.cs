using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleInvasionEliteSlimeNPC : BunnyNPC
{
    public override void SetDefaults()
    {
        DisplayName = "Elite Example Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 36;
        Height = 28;
        Damage = 14;
        Defense = 4;
        LifeMax = 80;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
    }

    public override bool AI(ref BunnyNpcAiState state)
    {
        float distance = state.PlayerX - state.X;
        if (!float.IsFinite(distance) || MathF.Abs(distance) < 16f)
            return false;
        state.VelocityX = Math.Clamp(state.VelocityX +
            (distance > 0f ? 0.08f : -0.08f), -4f, 4f);
        return true;
    }
}
