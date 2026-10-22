using System;
using System.Collections.Generic;
using System.Linq;

namespace BunnyLoader.Source;

/// <summary>
/// Descreve a bandeira de um NPC. BannerNpcClass pode apontar para outro NPC do
/// mesmo mod quando varias variantes compartilham o mesmo contador de abates.
/// ItemClass e o item que o BannerSystem deve entregar a cada ciclo de abates.
/// Sao nomes de classes registrados pelo Bunny, nao IDs alocados pelo mod.
/// </summary>
public sealed record BunnyBannerDefinition(
    string ModUid,
    string NpcClass,
    string BannerNpcClass,
    string ItemClass);

/// <summary>Estado persistente de uma bandeira. Fresh so existe quando ha premios para resgatar.</summary>
public readonly record struct BunnyBannerState(int KillCount, int Claimable, bool Fresh)
{
    public static BunnyBannerState Normalize(int kills, int claimable, bool fresh)
    {
        int count = Math.Max(0, kills);
        int ready = Math.Clamp(claimable, 0, ushort.MaxValue);
        return new BunnyBannerState(count, ready, ready > 0 && fresh);
    }
}

/// <summary>
/// Resultado de redimensionamento preparado fora da thread de desenho. Os cinco
/// arrays precisam ser publicados juntos pelo host, depois de verificar os
/// campos do IL2CPP; este metodo nao escreve memoria nativa.
/// </summary>
public sealed record BunnyBannerArrays(
    int[] KillCount,
    int[] ClaimableBanners,
    bool[] NewBanner,
    bool[] SceneBuff,
    bool[] PlayerSceneBuff);

/// <summary>
/// Nucleo gerenciado do BannerLoader. A identidade salva e UID/classe, nunca
/// o ID numerico atribuido durante a carga. O host continua responsavel pelos
/// hooks de BannerSystem, SceneMetrics, save binario e rede do Terraria.
/// </summary>
public sealed class BunnyBannerRegistry
{
    public const int MaximumBannerSlots = 262144;
    private readonly object gate = new();
    private readonly int vanillaNpcCount;
    private readonly Dictionary<string, BunnyBannerDefinition> definitions = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Binding> bindings = new(StringComparer.Ordinal);
    private readonly Dictionary<int, Binding> byNpc = new();
    private readonly Dictionary<int, int> itemByBanner = new();
    private readonly Dictionary<int, int> bannerByItem = new();
    private readonly Dictionary<string, BunnyBannerState> saved = new(StringComparer.Ordinal);

    private sealed record Binding(string Key, int NpcType, int BannerType, int ItemType);

    public BunnyBannerRegistry(int firstModNpc)
    {
        if (firstModNpc <= 0 || firstModNpc >= MaximumBannerSlots)
            throw new ArgumentOutOfRangeException(nameof(firstModNpc));
        vanillaNpcCount = firstModNpc;
    }

    public int Count { get { lock (gate) return bindings.Count; } }

    public static string MakeKey(string modUid, string npcClass)
    {
        ValidatePart(modUid, nameof(modUid));
        ValidatePart(npcClass, nameof(npcClass));
        return modUid + "/" + npcClass;
    }

    private static void ValidatePart(string part, string argument)
    {
        if (string.IsNullOrEmpty(part) || part.Length > 128)
            throw new ArgumentException("Identificador vazio ou muito longo", argument);
        foreach (char ch in part)
        {
            if (ch is >= 'a' and <= 'z' or >= 'A' and <= 'Z' or >= '0' and <= '9'
                or '_' or '-' or '.')
                continue;
            throw new ArgumentException("Use apenas letras ASCII, numeros, _, - ou .", argument);
        }
        if (part == "." || part == "..")
            throw new ArgumentException("Identificador reservado", argument);
    }

    public void Register(BunnyBannerDefinition definition)
    {
        ArgumentNullException.ThrowIfNull(definition);
        string key = MakeKey(definition.ModUid, definition.NpcClass);
        MakeKey(definition.ModUid, definition.BannerNpcClass);
        MakeKey(definition.ModUid, definition.ItemClass);
        lock (gate)
        {
            if (!definitions.TryAdd(key, definition))
                throw new ArgumentException("NPC ja registrado: " + key, nameof(definition));
        }
    }

    /// <summary>
    /// Converte classes em IDs reais somente apos o registro de conteudo.
    /// Todos os conflitos sao checados antes de publicar qualquer mapeamento.
    /// </summary>
    public bool TryBind(string modUid, string npcClass, int npcType, int bannerType, int itemType)
    {
        string key = MakeKey(modUid, npcClass);
        if (npcType < vanillaNpcCount || npcType >= MaximumBannerSlots ||
            bannerType < vanillaNpcCount || bannerType >= MaximumBannerSlots ||
            itemType <= 0)
            return false;
        lock (gate)
        {
            if (!definitions.ContainsKey(key) || bindings.ContainsKey(key) ||
                byNpc.ContainsKey(npcType))
                return false;
            if (itemByBanner.TryGetValue(bannerType, out int currentItem) && currentItem != itemType)
                return false;
            if (bannerByItem.TryGetValue(itemType, out int currentBanner) && currentBanner != bannerType)
                return false;

            var binding = new Binding(key, npcType, bannerType, itemType);
            bindings.Add(key, binding);
            byNpc.Add(npcType, binding);
            itemByBanner[bannerType] = itemType;
            bannerByItem[itemType] = bannerType;
            return true;
        }
    }

    public int NpcToBanner(int npcType)
    {
        lock (gate) return byNpc.TryGetValue(npcType, out var b) ? b.BannerType : 0;
    }

    public int BannerToItem(int bannerType)
    {
        lock (gate) return itemByBanner.TryGetValue(bannerType, out int item) ? item : 0;
    }

    public int ItemToBanner(int itemType)
    {
        lock (gate) return bannerByItem.TryGetValue(itemType, out int banner) ? banner : 0;
    }

    public bool IsModBanner(int bannerType)
    {
        lock (gate) return itemByBanner.ContainsKey(bannerType);
    }

    public int RequiredSlots
    {
        get
        {
            lock (gate) return Math.Max(vanillaNpcCount,
                itemByBanner.Count == 0 ? vanillaNpcCount : itemByBanner.Keys.Max() + 1);
        }
    }

    /// <summary>
    /// A API prepara cinco arrays do mesmo tamanho. A publicacao no IL2CPP
    /// exige transacao nativa: nunca substitua apenas um dos arrays.
    /// </summary>
    public static BunnyBannerArrays Grow(BunnyBannerArrays arrays, int requested)
    {
        ArgumentNullException.ThrowIfNull(arrays);
        if (requested <= 0 || requested > MaximumBannerSlots)
            throw new ArgumentOutOfRangeException(nameof(requested));
        ArgumentNullException.ThrowIfNull(arrays.KillCount);
        ArgumentNullException.ThrowIfNull(arrays.ClaimableBanners);
        ArgumentNullException.ThrowIfNull(arrays.NewBanner);
        ArgumentNullException.ThrowIfNull(arrays.SceneBuff);
        ArgumentNullException.ThrowIfNull(arrays.PlayerSceneBuff);
        int capacity = Math.Max(requested, new[] {
            arrays.KillCount.Length, arrays.ClaimableBanners.Length,
            arrays.NewBanner.Length, arrays.SceneBuff.Length, arrays.PlayerSceneBuff.Length
        }.Max());
        if (capacity > MaximumBannerSlots)
            throw new ArgumentOutOfRangeException(nameof(arrays));
        return new BunnyBannerArrays(
            Resize(arrays.KillCount, capacity), Resize(arrays.ClaimableBanners, capacity),
            Resize(arrays.NewBanner, capacity), Resize(arrays.SceneBuff, capacity),
            Resize(arrays.PlayerSceneBuff, capacity));
    }

    private static T[] Resize<T>(T[] array, int length)
    {
        if (array.Length == length) return array;
        var result = new T[length];
        Array.Copy(array, result, array.Length);
        return result;
    }

    /// <summary>
    /// Le os contadores da sessao antes de salvar. Chaves de mods ausentes
    /// permanecem no snapshot para voltar quando o mod for reativado.
    /// </summary>
    public IReadOnlyDictionary<string, BunnyBannerState> Capture(
        int[] kills, int[] claimable, bool[] fresh)
    {
        ArgumentNullException.ThrowIfNull(kills);
        ArgumentNullException.ThrowIfNull(claimable);
        ArgumentNullException.ThrowIfNull(fresh);
        lock (gate)
        {
            foreach (var binding in bindings.Values)
            {
                int id = binding.BannerType;
                if (id >= kills.Length || id >= claimable.Length || id >= fresh.Length)
                    throw new ArgumentException("BannerSystem nao foi redimensionado");
            }
            foreach (var binding in bindings.Values)
            {
                int id = binding.BannerType;
                var state = BunnyBannerState.Normalize(kills[id], claimable[id], fresh[id]);
                if (state.KillCount == 0 && state.Claimable == 0) saved.Remove(binding.Key);
                else saved[binding.Key] = state;
            }
            return new Dictionary<string, BunnyBannerState>(saved, StringComparer.Ordinal);
        }
    }

    /// <summary>
    /// Carrega o sidecar. Dados invalidos sao normalizados e nenhuma entrada
    /// pode atingir indices do Terraria ate haver um TryBind valido.
    /// </summary>
    public void Load(IReadOnlyDictionary<string, BunnyBannerState>? snapshot)
    {
        lock (gate)
        {
            saved.Clear();
            if (snapshot == null) return;
            if (snapshot.Count > MaximumBannerSlots)
                throw new ArgumentOutOfRangeException(nameof(snapshot));
            foreach (var entry in snapshot)
            {
                if (entry.Key == null || entry.Key.Length > 260) continue;
                int slash = entry.Key.IndexOf('/');
                if (slash <= 0 || slash == entry.Key.Length - 1) continue;
                try
                {
                    if (MakeKey(entry.Key[..slash], entry.Key[(slash + 1)..]) != entry.Key) continue;
                }
                catch (ArgumentException) { continue; }
                BunnyBannerState value = BunnyBannerState.Normalize(
                    entry.Value.KillCount, entry.Value.Claimable, entry.Value.Fresh);
                if (value.KillCount != 0 || value.Claimable != 0)
                    saved[entry.Key] = value;
            }
        }
    }

    /// <summary>
    /// Restaura somente IDs de banners pertencentes a mods. Arrays vanilla nao
    /// sao tocados. O host deve chamar RefreshHasBannerAvailable e
    /// RefreshAnyNewClaimableBanners depois da copia na thread do jogo.
    /// </summary>
    public void Restore(int[] kills, int[] claimable, bool[] fresh)
    {
        ArgumentNullException.ThrowIfNull(kills);
        ArgumentNullException.ThrowIfNull(claimable);
        ArgumentNullException.ThrowIfNull(fresh);
        lock (gate)
        {
            foreach (int id in itemByBanner.Keys)
            {
                if (id >= kills.Length || id >= claimable.Length || id >= fresh.Length)
                    throw new ArgumentException("BannerSystem nao foi redimensionado");
            }
            foreach (int id in itemByBanner.Keys)
            {
                kills[id] = 0;
                claimable[id] = 0;
                fresh[id] = false;
            }
            foreach (var binding in bindings.Values)
            {
                if (!saved.TryGetValue(binding.Key, out var state)) continue;
                int id = binding.BannerType;
                kills[id] = Math.Max(kills[id], state.KillCount);
                claimable[id] = Math.Max(claimable[id], state.Claimable);
                fresh[id] |= state.Fresh;
            }
        }
    }

    /// <summary>Descarrega somente um mod, sem apagar o sidecar do mundo.</summary>
    public void Unregister(string modUid)
    {
        ValidatePart(modUid, nameof(modUid));
        lock (gate)
        {
            foreach (string key in definitions.Keys.Where(key => key.StartsWith(modUid + "/", StringComparison.Ordinal)).ToArray())
            {
                definitions.Remove(key);
                bindings.Remove(key);
            }
            RebuildIndices();
        }
    }

    public void ResetWorld()
    {
        lock (gate) saved.Clear();
    }

    private void RebuildIndices()
    {
        byNpc.Clear();
        itemByBanner.Clear();
        bannerByItem.Clear();
        foreach (var entry in bindings.Values)
        {
            byNpc.Add(entry.NpcType, entry);
            itemByBanner[entry.BannerType] = entry.ItemType;
            bannerByItem[entry.ItemType] = entry.BannerType;
        }
    }
}
