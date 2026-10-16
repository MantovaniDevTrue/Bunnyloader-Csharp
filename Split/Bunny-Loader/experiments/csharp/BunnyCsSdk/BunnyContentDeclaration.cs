using System;

namespace BunnyLoader.Source;

public enum BunnyContentKind
{
    Item, Tile, Weapon, Projectile, Buff, BuffPotion, Accessory, Player, NPC, Prefix
}

/// <summary>Declares content identity before Mono starts. Required for custom base classes.</summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
public sealed class BunnyContentAttribute : Attribute
{
    public BunnyContentKind Kind { get; }
    public string DisplayName { get; }
    public string Texture { get; }

    public BunnyContentAttribute(BunnyContentKind kind, string displayName, string texture)
    {
        if (!Enum.IsDefined(typeof(BunnyContentKind), kind))
            throw new ArgumentOutOfRangeException(nameof(kind));
        if (string.IsNullOrWhiteSpace(displayName) || displayName.Length > 100 ||
            displayName.IndexOfAny(new[] { '\0', '\t', '\r', '\n' }) >= 0)
            throw new ArgumentException("Invalid content display name", nameof(displayName));
        if (string.IsNullOrWhiteSpace(texture) || texture.Length > 200 ||
            texture.StartsWith('/') || texture.Contains("..") || texture.Contains('\\') ||
            texture.Contains(':') || texture.IndexOfAny(new[] { '\0', '\t', '\r', '\n' }) >= 0)
            throw new ArgumentException("Texture must be a relative asset path", nameof(texture));
        Kind = kind;
        DisplayName = displayName;
        Texture = texture;
    }
}
