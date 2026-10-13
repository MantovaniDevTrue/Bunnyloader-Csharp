using System;

namespace BunnyLoader.Source;

public enum BunnyVisualKind
{
    Particle = 1,
    PlayerOverlay = 2,
    ClearOverlay = 3,
    EquipmentSprite = 4
}

public readonly struct BunnyVisualCommand
{
    public readonly BunnyVisualKind Kind;
    public readonly int PlayerIndex;
    public readonly int EffectId;
    public readonly string Texture;
    public readonly float X;
    public readonly float Y;
    public readonly float Scale;
    public readonly float Opacity;
    public readonly uint Color;
    public readonly int DurationTicks;

    public BunnyVisualCommand(BunnyVisualKind kind, int playerIndex, int effectId,
        string texture, float x, float y, float scale, float opacity,
        uint color, int durationTicks)
    {
        Kind = kind;
        PlayerIndex = playerIndex;
        EffectId = effectId;
        Texture = texture ?? "";
        X = x;
        Y = y;
        Scale = scale;
        Opacity = opacity;
        Color = color;
        DurationTicks = durationTicks;
    }
}

public static class BunnyVisual
{
    public const int MaxCommandsPerFrame = 64;
    private static readonly BunnyVisualCommand[] Commands =
        new BunnyVisualCommand[MaxCommandsPerFrame];
    private static int count;

    public static int PendingCount => count;

    public static bool SpawnParticle(int playerIndex, int effectId, string texture,
        float x, float y, float scale, uint color, int durationTicks)
    {
        return Add(new BunnyVisualCommand(BunnyVisualKind.Particle, playerIndex,
            effectId, texture, x, y, scale, 1f, color, durationTicks));
    }

    public static bool SetPlayerOverlay(int playerIndex, int effectId, string texture,
        float scale, float opacity, uint color, int durationTicks)
    {
        return Add(new BunnyVisualCommand(BunnyVisualKind.PlayerOverlay, playerIndex,
            effectId, texture, 0f, 0f, scale, opacity, color, durationTicks));
    }

    public static bool ClearPlayerOverlay(int playerIndex, int effectId)
    {
        return Add(new BunnyVisualCommand(BunnyVisualKind.ClearOverlay, playerIndex,
            effectId, "", 0f, 0f, 1f, 0f, 0, 0));
    }

    public static bool SetEquipmentSprite(int playerIndex, int effectId, string texture)
    {
        return Add(new BunnyVisualCommand(BunnyVisualKind.EquipmentSprite, playerIndex,
            effectId, texture, 0f, 0f, 1f, 1f, 0xFFFFFFFF, 1));
    }

    public static bool TryTake(out BunnyVisualCommand command)
    {
        if (count == 0)
        {
            command = default;
            return false;
        }
        command = Commands[0];
        for (int i = 1; i < count; i++)
            Commands[i - 1] = Commands[i];
        count--;
        return true;
    }

    public static void ClearPending()
    {
        count = 0;
    }

    private static bool Add(BunnyVisualCommand command)
    {
        if (count >= MaxCommandsPerFrame || command.PlayerIndex < 0 ||
            command.EffectId < 0 || command.DurationTicks < 0 ||
            float.IsNaN(command.Scale) || float.IsInfinity(command.Scale) ||
            command.Scale <= 0f || float.IsNaN(command.Opacity) ||
            command.Opacity < 0f || command.Opacity > 1f ||
            command.Texture.Length > 240)
            return false;

        Commands[count++] = command;
        return true;
    }
}
