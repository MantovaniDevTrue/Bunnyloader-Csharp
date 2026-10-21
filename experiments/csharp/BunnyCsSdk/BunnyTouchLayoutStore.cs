using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace BunnyLoader.Source;

/// <summary>
/// Representa um espaço do editor. ActionFullName e GroupActions contêm apenas
/// identidades de mod; ações vanilla são mantidas pelo host em seus próprios IDs.
/// As flags Unbound impedem substituir o comando vanilla de outro mod.
/// </summary>
public sealed class BunnyTouchLayoutSlot
{
    public string SlotId { get; init; } = "";
    public int Index { get; init; }
    public bool ActionUnbound { get; set; }
    public string? ActionFullName { get; set; }
    public string?[] GroupActions { get; init; } = Array.Empty<string?>();
    public bool[] GroupUnbound { get; init; } = Array.Empty<bool>();
}

/// <summary>
/// Arquivo BunnyLoader.touchbuttons.json no mesmo esquema do loader JS.
/// O host deve capturar os Slots ANTES de salvar o perfil vanilla e restaurar
/// os IDs depois; nunca gravar os valores C# no arquivo padrão do jogo.
/// </summary>
public sealed class BunnyTouchLayoutStore
{
    private const int MaxProfiles = 64;
    private const int MaxSlots = 256;
    private const int MaxGroups = 16;
    private const int MaxFileBytes = 1024 * 1024;

    private sealed class Row
    {
        [JsonPropertyName("slot")]
        public string Slot { get; set; } = "";
        [JsonPropertyName("index")]
        public int Index { get; set; }
        [JsonPropertyName("action")]
        public string? Action { get; set; }
        [JsonPropertyName("group")]
        public List<string?> Group { get; set; } = new();
    }

    private sealed class Document
    {
        [JsonPropertyName("version")]
        public int Version { get; set; } = 1;
        [JsonPropertyName("profiles")]
        public Dictionary<string, List<Row>> Profiles { get; set; } =
            new(StringComparer.Ordinal);
    }

    private readonly Document document = new();

    public int ProfileCount => document.Profiles.Count;
    public bool HasProfile(string id) => document.Profiles.ContainsKey(id);

    public static BunnyTouchLayoutStore Parse(string json)
    {
        ArgumentNullException.ThrowIfNull(json);
        if (Encoding.UTF8.GetByteCount(json) > MaxFileBytes)
            throw new InvalidDataException("Layout de controles maior que 1 MiB");
        using JsonDocument parsed = JsonDocument.Parse(json, new JsonDocumentOptions
        {
            MaxDepth = 16,
            AllowTrailingCommas = false,
            CommentHandling = JsonCommentHandling.Disallow
        });
        JsonElement root = parsed.RootElement;
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty("version", out var version) ||
            !version.TryGetInt32(out int number) || number != 1 ||
            !root.TryGetProperty("profiles", out var profiles) ||
            profiles.ValueKind != JsonValueKind.Object)
            throw new InvalidDataException("Formato de perfis incompatível");

        var result = new BunnyTouchLayoutStore();
        int profileCount = 0;
        foreach (JsonProperty profile in profiles.EnumerateObject())
        {
            ValidateProfile(profile.Name);
            if (++profileCount > MaxProfiles || profile.Value.ValueKind != JsonValueKind.Array)
                throw new InvalidDataException("Quantidade/formato de perfis inválido");
            var rows = new List<Row>();
            var keys = new HashSet<string>(StringComparer.Ordinal);
            foreach (JsonElement entry in profile.Value.EnumerateArray())
            {
                if (rows.Count == MaxSlots || entry.ValueKind != JsonValueKind.Object ||
                    !entry.TryGetProperty("slot", out var slot) ||
                    slot.ValueKind != JsonValueKind.String ||
                    !entry.TryGetProperty("index", out var index) ||
                    !index.TryGetInt32(out int i) || i < 0 || i >= MaxSlots)
                    throw new InvalidDataException("Espaço de controle inválido");
                string key = slot.GetString() ?? "";
                ValidateSlot(key);
                if (!keys.Add(key)) throw new InvalidDataException("SlotId duplicado");
                string? action = ReadOptionalAction(entry, "action");
                var row = new Row { Slot = key, Index = i, Action = action };
                if (entry.TryGetProperty("group", out var group))
                {
                    if (group.ValueKind != JsonValueKind.Array)
                        throw new InvalidDataException("Grupo inválido");
                    foreach (var item in group.EnumerateArray())
                    {
                        if (row.Group.Count == MaxGroups)
                            throw new InvalidDataException("Grupo excede 16 ações");
                        if (item.ValueKind != JsonValueKind.Null &&
                            item.ValueKind != JsonValueKind.String)
                            throw new InvalidDataException("Ação de grupo inválida");
                        string? value = item.ValueKind == JsonValueKind.Null
                            ? null : item.GetString();
                        if (value is not null) ValidateAction(value);
                        row.Group.Add(value);
                    }
                }
                rows.Add(row);
            }
            result.document.Profiles.Add(profile.Name, rows);
        }
        return result;
    }

    public string Serialize() => JsonSerializer.Serialize(document);

    // Este método só recebe o mapa de ações conhecido do host: nenhuma
    // enumeração de objetos do Terraria nem varredura por frame é necessária.
    public void Capture(string profileId, IReadOnlyList<BunnyTouchLayoutSlot> slots,
                        Func<string, bool> isLoadedAction)
    {
        ValidateProfile(profileId);
        ArgumentNullException.ThrowIfNull(slots);
        ArgumentNullException.ThrowIfNull(isLoadedAction);
        if (slots.Count > MaxSlots) throw new ArgumentOutOfRangeException(nameof(slots));
        if (!document.Profiles.TryGetValue(profileId, out var previous) &&
            document.Profiles.Count >= MaxProfiles)
            throw new InvalidOperationException("Limite de perfis de controles");
        var old = new Dictionary<string, Row>(StringComparer.Ordinal);
        if (previous is not null)
            foreach (Row row in previous) old[row.Slot] = row;
        var rows = new List<Row>(slots.Count);
        var visited = new HashSet<string>(StringComparer.Ordinal);
        foreach (var slot in slots)
        {
            if (slot.Index < 0 || slot.Index >= MaxSlots)
                throw new InvalidDataException("Índice de slot inválido");
            string key = string.IsNullOrEmpty(slot.SlotId)
                ? "#" + slot.Index : slot.SlotId;
            ValidateSlot(key);
            if (!visited.Add(key)) throw new InvalidDataException("SlotId duplicado");
            if (slot.GroupActions.Length > MaxGroups ||
                slot.GroupUnbound.Length < slot.GroupActions.Length)
                throw new InvalidDataException("Grupos incompatíveis");
            old.TryGetValue(key, out Row? before);
            string? action = Keep(slot.ActionFullName, slot.ActionUnbound,
                                  before?.Action, isLoadedAction);
            var group = new List<string?>(slot.GroupActions.Length);
            bool any = action is not null;
            for (int g = 0; g < slot.GroupActions.Length; ++g)
            {
                string? last = before is not null && g < before.Group.Count
                    ? before.Group[g] : null;
                string? value = Keep(slot.GroupActions[g], slot.GroupUnbound[g],
                                     last, isLoadedAction);
                group.Add(value);
                if (value is not null) any = true;
            }
            if (any) rows.Add(new Row
            {
                Slot = key, Index = slot.Index, Action = action, Group = group
            });
        }
        // Perfis não abertos não são apagados, inclusive de mods desinstalados.
        if (rows.Count == 0) document.Profiles.Remove(profileId);
        else document.Profiles[profileId] = rows;
    }

    public void Restore(string profileId, IReadOnlyList<BunnyTouchLayoutSlot> slots,
                        Func<string, bool> isLoadedAction)
    {
        ValidateProfile(profileId);
        ArgumentNullException.ThrowIfNull(slots);
        ArgumentNullException.ThrowIfNull(isLoadedAction);
        if (!document.Profiles.TryGetValue(profileId, out List<Row>? rows)) return;
        var bySlot = new Dictionary<string, BunnyTouchLayoutSlot>(StringComparer.Ordinal);
        foreach (var slot in slots)
        {
            string key = string.IsNullOrEmpty(slot.SlotId) ? "#" + slot.Index : slot.SlotId;
            ValidateSlot(key);
            bySlot[key] = slot;
        }
        foreach (Row row in rows)
        {
            if (!bySlot.TryGetValue(row.Slot, out BunnyTouchLayoutSlot? slot) &&
                row.Index >= 0 && row.Index < slots.Count)
                slot = slots[row.Index];
            if (slot is null) continue;
            // Só restaura nos espaços vazios. Comandos vanilla nunca são pisados.
            if (slot.ActionUnbound && row.Action is not null &&
                isLoadedAction(row.Action))
                slot.ActionFullName = row.Action;
            for (int i = 0; i < row.Group.Count && i < slot.GroupActions.Length &&
                            i < slot.GroupUnbound.Length; i++)
                if (slot.GroupUnbound[i] && row.Group[i] is { } action &&
                    isLoadedAction(action))
                    slot.GroupActions[i] = action;
        }
    }

    public static BunnyTouchLayoutStore Load(string saveDirectory)
    {
        string path = PathFor(saveDirectory);
        if (!File.Exists(path)) return new BunnyTouchLayoutStore();
        var info = new FileInfo(path);
        if (info.Length > MaxFileBytes)
            throw new InvalidDataException("Layout de controles maior que 1 MiB");
        return Parse(File.ReadAllText(path, Encoding.UTF8));
    }

    public void Save(string saveDirectory)
    {
        string path = PathFor(saveDirectory);
        string json = Serialize();
        if (Encoding.UTF8.GetByteCount(json) > MaxFileBytes)
            throw new InvalidDataException("Layout excede 1 MiB");
        Directory.CreateDirectory(saveDirectory);
        string temp = path + ".tmp";
        try
        {
            File.WriteAllText(temp, json, new UTF8Encoding(false));
            File.Move(temp, path, true);
        }
        finally
        {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }

    private static string PathFor(string directory)
    {
        if (string.IsNullOrWhiteSpace(directory))
            throw new ArgumentException("Diretório do jogo ausente", nameof(directory));
        return Path.Combine(Path.GetFullPath(directory), "BunnyLoader.touchbuttons.json");
    }

    private static string? Keep(string? current, bool unbound,
                                string? previous, Func<string, bool> loaded)
    {
        if (current is not null)
        {
            ValidateAction(current);
            return current;
        }
        return unbound && previous is not null && !loaded(previous)
            ? previous : null;
    }

    private static string? ReadOptionalAction(JsonElement element, string property)
    {
        if (!element.TryGetProperty(property, out var value) ||
            value.ValueKind == JsonValueKind.Null) return null;
        if (value.ValueKind != JsonValueKind.String)
            throw new InvalidDataException("Tipo de ação inválido");
        string? text = value.GetString();
        if (text is not null) ValidateAction(text);
        return text;
    }

    private static void ValidateProfile(string value)
    {
        if (value.Length == 0 || value.Length > 100 || HasControl(value))
            throw new InvalidDataException("Identificador de perfil inválido");
    }

    private static void ValidateSlot(string value)
    {
        if (value.Length == 0 || value.Length > 128 || HasControl(value))
            throw new InvalidDataException("SlotId inválido");
    }

    private static void ValidateAction(string value)
    {
        int slash = value.IndexOf('/');
        if (slash <= 0 || slash != value.LastIndexOf('/') || slash == value.Length - 1)
            throw new InvalidDataException("Identidade de ação inválida");
        ValidPart(value.AsSpan(0, slash));
        ValidPart(value.AsSpan(slash + 1));
    }

    private static void ValidPart(ReadOnlySpan<char> part)
    {
        if (part.Length == 0 || part.Length > 80) throw new InvalidDataException("ID inválido");
        foreach (char c in part)
            if (!(c is >= 'a' and <= 'z' or >= 'A' and <= 'Z' or >= '0' and <= '9' ||
                  c == '_' || c == '-' || c == '.'))
                throw new InvalidDataException("ID contém caracteres inválidos");
    }

    private static bool HasControl(string value)
    {
        foreach (char c in value)
            if (char.IsControl(c)) return true;
        return false;
    }
}
