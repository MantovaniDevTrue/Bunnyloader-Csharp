using System;

namespace BunnyLoader.Source;

/// <summary>Nome localizado registrado no núcleo antes de carregar as tabelas de conteúdo.</summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
public sealed class BunnyNameAttribute : Attribute
{
    public string Culture { get; }
    public string Text { get; }
    public BunnyNameAttribute(string culture,string text)
    {
        if(string.IsNullOrEmpty(culture)||culture.Length<2||culture.Length>20)throw new ArgumentException("Cultura inválida",nameof(culture));
        foreach(char c in culture)if(!char.IsAsciiLetterOrDigit(c)&&c!='-')throw new ArgumentException("Cultura inválida",nameof(culture));
        if(string.IsNullOrWhiteSpace(text)||System.Text.Encoding.UTF8.GetByteCount(text)>128)throw new ArgumentException("Nome localizado inválido",nameof(text));
        foreach(char c in text)if(c<32||c==127)throw new ArgumentException("Controle em nome localizado",nameof(text));
        Culture=culture;Text=text;
    }
}

public enum BunnyContentKind
{
    Item, Tile, Tree, Weapon, Projectile, Buff, BuffPotion, Accessory, Player, NPC, Prefix, Wall
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
