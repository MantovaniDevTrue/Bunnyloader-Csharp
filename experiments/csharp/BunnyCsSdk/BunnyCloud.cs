using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

/// <summary>
/// Snapshot recebido do Terraria somente quando o CloudLoader atualiza os
/// pesos de aparecimento. Não consultar o jogo a cada quadro pelo MonoVM.
/// </summary>
public sealed class BunnyCloudEnvironment
{
    private readonly HashSet<string> activeBiomes;

    public bool GameMenu { get; }
    public bool TenthAnniversaryWorld { get; }

    public BunnyCloudEnvironment(bool gameMenu, bool tenthAnniversaryWorld,
        IEnumerable<string>? activeBiomeKeys = null)
    {
        GameMenu = gameMenu;
        TenthAnniversaryWorld = tenthAnniversaryWorld;
        activeBiomes = new HashSet<string>(StringComparer.Ordinal);
        if (activeBiomeKeys is null) return;
        foreach (string key in activeBiomeKeys)
        {
            if (string.IsNullOrWhiteSpace(key) || key.Length > 160)
                throw new ArgumentException("Identificador de bioma inválido", nameof(activeBiomeKeys));
            activeBiomes.Add(key);
        }
    }

    /// <summary>
    /// A chave é o identificador registrado do bioma, nunca seu nome traduzido.
    /// No menu não existe um jogador em um bioma válido.
    /// </summary>
    public bool InBiome(string biomeKey) =>
        !GameMenu && !string.IsNullOrWhiteSpace(biomeKey) && activeBiomes.Contains(biomeKey);
}

/// <summary>Valores compatíveis com o SpriteEffects usado pelo CloudLoader JS.</summary>
[Flags]
public enum BunnyCloudSpriteEffects
{
    None = 0,
    FlipHorizontally = 1,
    FlipVertically = 2
}

/// <summary>
/// Cópia de uma nuvem que o Terraria já criou. O núcleo copiará de volta apenas
/// alterações permitidas e somente se o slot ainda contiver a mesma nuvem.
/// </summary>
public sealed class BunnyCloudSpawn
{
    public int Type { get; }
    public int Slot { get; }
    public float Scale { get; }
    public BunnyCloudSpriteEffects SpriteEffects { get; set; }

    public BunnyCloudSpawn(int type, int slot, float scale,
        BunnyCloudSpriteEffects spriteEffects)
    {
        if (type < 0) throw new ArgumentOutOfRangeException(nameof(type));
        if (slot < 0 || slot >= 200) throw new ArgumentOutOfRangeException(nameof(slot));
        if (!float.IsFinite(scale) || scale <= 0f || scale > 10f)
            throw new ArgumentOutOfRangeException(nameof(scale));
        if (!Enum.IsDefined(typeof(BunnyCloudSpriteEffects), spriteEffects)
            && spriteEffects != (BunnyCloudSpriteEffects)3)
            throw new ArgumentOutOfRangeException(nameof(spriteEffects));
        Type = type;
        Slot = slot;
        Scale = scale;
        SpriteEffects = spriteEffects;
    }

    public bool HasValidVisuals => ((int)SpriteEffects & ~3) == 0;
}

/// <summary>
/// Contrato C# em desenvolvimento, equivalente ao ModCloud do ExampleModBunny.
/// ATENÇÃO: ainda falta conectá-lo ao parser, TextureAssets.Cloud, CloudMasks
/// e aos hooks IL2CPP/MonoVM. Uma subclasse compilável ainda NÃO aparece no jogo.
/// </summary>
public abstract class BunnyCloud
{
    public string Texture { get; protected set; } = "";
    public virtual bool RareCloud => false;

    public virtual void SetStaticDefaults() { }

    /// <summary>
    /// Chamado em atualizações limitadas do pool, não a cada frame.
    /// Peso zero desabilita a nuvem. O núcleo nunca deve usar NaN/infinito.
    /// </summary>
    public virtual float SpawnChance(BunnyCloudEnvironment environment) => 1f;

    /// <summary>O Terraria já criou o slot; não criar outra nuvem aqui.</summary>
    public virtual void OnSpawn(BunnyCloudSpawn cloud) { }

    /// <summary>Validação feita na carga, antes de publicar o ID do conteúdo.</summary>
    public void Validate()
    {
        if (string.IsNullOrEmpty(Texture) || Texture.Length > 240 ||
            Texture[0] == '/' || Texture.Contains("..", StringComparison.Ordinal) ||
            Texture.Contains('\\') || Texture.Contains(':'))
            throw new ArgumentException("Textura deve ser um caminho relativo sem extensão");
        foreach (char c in Texture)
            if (!char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '/')
                throw new ArgumentException("Caractere inválido no caminho da textura");
    }

    /// <summary>Descarta apenas os pesos inválidos; não derruba o céu vanilla.</summary>
    public static float SafeSpawnChance(float value) =>
        float.IsFinite(value) && value > 0f && value <= 1000000f ? value : 0f;
}
