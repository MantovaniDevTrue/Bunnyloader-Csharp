using System;
using System.Globalization;

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
    private static int head;

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
        command = Commands[head];
        head = (head + 1) % MaxCommandsPerFrame;
        count--;
        return true;
    }

    public static string TakeWire()
    {
        if (!TryTake(out BunnyVisualCommand command))
            return "";

        return string.Join("|", "V1",
            ((int)command.Kind).ToString(CultureInfo.InvariantCulture),
            command.PlayerIndex.ToString(CultureInfo.InvariantCulture),
            command.EffectId.ToString(CultureInfo.InvariantCulture),
            command.X.ToString("R", CultureInfo.InvariantCulture),
            command.Y.ToString("R", CultureInfo.InvariantCulture),
            command.Scale.ToString("R", CultureInfo.InvariantCulture),
            command.Opacity.ToString("R", CultureInfo.InvariantCulture),
            command.Color.ToString(CultureInfo.InvariantCulture),
            command.DurationTicks.ToString(CultureInfo.InvariantCulture),
            command.Texture);
    }

    public static void ClearPending()
    {
        count = 0;
        head = 0;
    }

    private static bool Add(BunnyVisualCommand command)
    {
        if (command.PlayerIndex < 0 ||
            command.EffectId < 0 || command.DurationTicks < 0 ||
            float.IsNaN(command.Scale) || float.IsInfinity(command.Scale) ||
            command.Scale <= 0f || float.IsNaN(command.Opacity) ||
            command.Opacity < 0f || command.Opacity > 1f ||
            command.Texture.Length > 240 ||
            command.Texture.Contains('|') || command.Texture.Contains("..") ||
            command.Texture.IndexOf((char)92) >= 0)
            return false;

        if (command.Kind == BunnyVisualKind.PlayerOverlay ||
            command.Kind == BunnyVisualKind.ClearOverlay ||
            command.Kind == BunnyVisualKind.EquipmentSprite)
        {
            for (int i = 0; i < count; i++)
            {
                int index = (head + i) % MaxCommandsPerFrame;
                BunnyVisualCommand previous = Commands[index];
                if (previous.PlayerIndex == command.PlayerIndex &&
                    previous.EffectId == command.EffectId &&
                    (previous.Kind == command.Kind ||
                     (previous.Kind == BunnyVisualKind.PlayerOverlay && command.Kind == BunnyVisualKind.ClearOverlay) ||
                     (previous.Kind == BunnyVisualKind.ClearOverlay && command.Kind == BunnyVisualKind.PlayerOverlay)))
                {
                    Commands[index] = command;
                    return true;
                }
            }
        }

        if (count >= MaxCommandsPerFrame)
            return false;

        Commands[(head + count) % MaxCommandsPerFrame] = command;
        count++;
        return true;
    }
}
