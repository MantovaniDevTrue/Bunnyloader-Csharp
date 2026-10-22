using System;
using System.Collections.Generic;
using System.Linq;

namespace BunnyLoader.Source;

/// <summary>
/// Gênero sugerido pela seleção aleatória de cabelo.
/// Não impede um personagem de selecionar qualquer penteado manualmente.
/// </summary>
public enum Gender
{
    Unspecified = 0,
    Male = 1,
    Female = 2
}

/// <summary>
/// Condição avaliada ao abrir ou atualizar a lista do Cabeleireiro.
/// O predicado deve consultar um estado atual e não modificar o mundo.
/// </summary>
public sealed class Condition
{
    private readonly Func<bool> predicate;

    public string Description { get; }

    public Condition(string description, Func<bool> predicate)
    {
        if (string.IsNullOrWhiteSpace(description))
            throw new ArgumentException("A condição precisa de uma descrição.", nameof(description));
        this.predicate = predicate ?? throw new ArgumentNullException(nameof(predicate));
        Description = description;
    }

    public bool IsMet() => predicate();
}

/// <summary>
/// Contrato C# de penteados inspirado em Terraria.ModLoader.ModHair.
/// Esta classe NÃO instala ganchos nativos sozinha: HairLoader prepara os dados,
/// e a integração IL2CPP será exigida antes de liberar uma build Android.
/// </summary>
public abstract class ModHair
{
    /// <summary>ID atribuído pelo HairLoader. -1 significa não registrado.</summary>
    public int Type { get; internal set; } = -1;

    /// <summary>Nome interno estável usado na identificação do personagem salvo.</summary>
    public virtual string Name => GetType().Name;

    /// <summary>Caminho relativo da textura principal, sem extensão .png.</summary>
    public virtual string Texture => "Content/Hairs/" + Name;

    /// <summary>
    /// Textura com chapéu. Se o arquivo _Alt não existir, o loader reutiliza
    /// a textura principal sem criar uma referência nula.
    /// </summary>
    public virtual string AltTexture => Texture + "_Alt";

    /// <summary>Controla a presença na criação de personagem.</summary>
    public virtual bool AvailableDuringCharacterCreation => true;

    /// <summary>Gênero sugerido quando o gerador escolhe este cabelo.</summary>
    public virtual Gender RandomizedCharacterCreationGender => Gender.Unspecified;

    /// <summary>
    /// Condições do Cabeleireiro, com a mesma separação da API do tModLoader.
    /// Estas condições não escondem o cabelo na criação do personagem.
    /// </summary>
    public virtual IEnumerable<Condition> GetUnlockConditions() => Array.Empty<Condition>();

    /// <summary>AutoStaticDefaults segue o contrato do tModLoader.</summary>
    public virtual void AutoStaticDefaults() { }

    /// <summary>Chamado uma vez quando o conteúdo é registrado, antes das texturas.</summary>
    public virtual void SetStaticDefaults() { }

    /// <summary>
    /// Avalia apenas a disponibilidade; não mexe na UI nem no jogador.
    /// A criação tem precedência caso os dois contextos sejam sinalizados.
    /// </summary>
    public bool IsUnlocked(bool isAtCharacterCreation, bool isAtStylist)
    {
        if (isAtCharacterCreation)
            return AvailableDuringCharacterCreation;
        if (!isAtStylist)
            return false;

        var conditions = GetUnlockConditions()
            ?? throw new InvalidOperationException(Name + ": GetUnlockConditions retornou null.");
        foreach (Condition condition in conditions)
        {
            if (condition is null)
                throw new InvalidOperationException(Name + ": condição de desbloqueio nula.");
            if (!condition.IsMet())
                return false;
        }
        return true;
    }
}

/// <summary>
/// Alias opcional que preserva o padrão BunnyItem/BunnyTile dos exemplos C#.
/// Mods novos podem preferir herdar diretamente de ModHair.
/// </summary>
public abstract class BunnyHair : ModHair { }

/// <summary>
/// Descrição imutável destinada ao futuro instalador nativo de texturas.
/// FullName usa UID/classe em vez do ID numérico, pois este muda com a ordem dos mods.
/// </summary>
public sealed record HairDefinition(
    int Type,
    string FullName,
    string MainTexture,
    string AltTexture,
    Gender RandomGender);

/// <summary>
/// Registro gerenciado, independente de QuickJS.
/// Um HairLoader pertence a uma única sessão de carregamento; Reset limpa
/// integralmente as identidades e os IDs quando a sessão é encerrada.
/// </summary>
public sealed class HairLoader
{
    public const int VanillaHairCount = 228;
    public const int MaximumHairType = 4095;

    private readonly Dictionary<int, ModHair> byType = new();
    private readonly Dictionary<string, HairDefinition> byKey =
        new(StringComparer.Ordinal);
    private readonly Dictionary<int, HairDefinition> definitions = new();
    private readonly int firstModHairType;
    private int nextType;

    public HairLoader(int firstModHairType = VanillaHairCount)
    {
        if (firstModHairType < VanillaHairCount || firstModHairType > MaximumHairType)
            throw new ArgumentOutOfRangeException(nameof(firstModHairType));
        this.firstModHairType = firstModHairType;
        nextType = firstModHairType;
    }

    /// <summary>
    /// Registra conteúdo sem ler arquivos diretamente do MonoVM.
    /// fileExists é fornecido pelo empacotador do mod e recebe o caminho
    /// relativo completo com extensão .png. O arquivo principal é obrigatório.
    /// </summary>
    public HairDefinition Register(string modUid, ModHair hair, Func<string, bool> fileExists)
    {
        if (hair is null) throw new ArgumentNullException(nameof(hair));
        if (fileExists is null) throw new ArgumentNullException(nameof(fileExists));
        ValidateIdentity(modUid, nameof(modUid));
        ValidateIdentity(hair.Name, nameof(hair.Name));

        string key = modUid + "/" + hair.Name;
        if (byKey.ContainsKey(key))
            throw new InvalidOperationException("Cabelo duplicado: " + key);
        if (hair.Type >= 0)
            throw new InvalidOperationException("Esta instância de cabelo já está registrada.");
        if (nextType > MaximumHairType)
            throw new InvalidOperationException("Limite de IDs de cabelos de mod atingido.");

        int type = nextType;
        hair.Type = type;
        try
        {
            // Assim como no tModLoader, o Type já existe em SetStaticDefaults.
            // Se a textura ou um hook falhar, o registro inteiro é desfeito.
            hair.AutoStaticDefaults();
            hair.SetStaticDefaults();

            string texture = ValidateTexture(hair.Texture);
            string alternate = ValidateTexture(hair.AltTexture);
            if (!fileExists(texture + ".png"))
                throw new InvalidOperationException(key + ": textura principal ausente: " + texture + ".png");
            if (!fileExists(alternate + ".png"))
                alternate = texture;

            Gender gender = hair.RandomizedCharacterCreationGender;
            if (!Enum.IsDefined(typeof(Gender), gender))
                throw new InvalidOperationException(key + ": gênero de randomização inválido.");

            var definition = new HairDefinition(type, key, texture, alternate, gender);
            byType.Add(type, hair);
            byKey.Add(key, definition);
            definitions.Add(type, definition);
            nextType++;
            return definition;
        }
        catch
        {
            byType.Remove(type);
            byKey.Remove(key);
            definitions.Remove(type);
            hair.Type = -1;
            throw;
        }
    }

    /// <summary>
    /// Resolve o penteado salvo pela identidade do mod, não por número.
    /// Quando o mod não está instalado, retorna false para o host usar cabelo 0.
    /// </summary>
    public bool TryResolveSavedHair(string fullName, out int type)
    {
        if (fullName is not null && byKey.TryGetValue(fullName, out HairDefinition? found))
        {
            type = found.Type;
            return true;
        }
        type = 0;
        return false;
    }

    public bool TryGet(int type, out ModHair? hair) => byType.TryGetValue(type, out hair);

    /// <summary>
    /// Snapshot sem acesso ao mundo. O host deve instalar estes recursos
    /// somente na thread do jogo, antes de expor os IDs ao jogador.
    /// </summary>
    public IReadOnlyList<HairDefinition> GetDefinitions() =>
        definitions.Values.OrderBy(hair => hair.Type).ToArray();

    /// <summary>
    /// Consulta para a criação/Cabeleireiro. Não faz cache permanente:
    /// uma condição pode mudar conforme a progressão do mundo.
    /// </summary>
    public IReadOnlyList<int> GetUnlockedTypes(bool characterCreation, bool stylist)
    {
        if (!characterCreation && !stylist) return Array.Empty<int>();
        var result = new List<int>();
        foreach (KeyValuePair<int, ModHair> entry in byType.OrderBy(pair => pair.Key))
        {
            if (entry.Value.IsUnlocked(characterCreation, stylist))
                result.Add(entry.Key);
        }
        return result;
    }

    public void Reset()
    {
        foreach (ModHair hair in byType.Values)
            hair.Type = -1;
        byType.Clear();
        byKey.Clear();
        definitions.Clear();
        nextType = firstModHairType;
    }

    private static string ValidateTexture(string texture)
    {
        if (string.IsNullOrWhiteSpace(texture) ||
            texture.Length > 200 ||
            texture.StartsWith("/", StringComparison.Ordinal) ||
            texture.Contains("..", StringComparison.Ordinal) ||
            texture.Contains('\\') ||
            texture.Contains(':') ||
            texture.EndsWith(".png", StringComparison.OrdinalIgnoreCase) ||
            texture.Any(char.IsControl))
            throw new ArgumentException("Textura deve ser um caminho relativo sem extensão .png.");
        return texture;
    }

    private static void ValidateIdentity(string? value, string parameter)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > 128 ||
            value.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-')))
            throw new ArgumentException("Identificador inválido de conteúdo.", parameter);
    }
}
