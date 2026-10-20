using System;
using System.Collections.Generic;
using System.Threading;

namespace BunnyLoader.Source;

public abstract class BunnyRarity
{
    public string ModId { get; internal set; } = "";
    public int Type { get; internal set; } = -1;

    public virtual BunnyColor RarityColor => new(255, 255, 255);

    public virtual void SetStaticDefaults() { }

    public virtual int GetPrefixedRarity(int offset, float valueMultiplier) => Type;
}

public readonly record struct BunnyRarityEntry(
    int Type,
    string ModId,
    string ClassName,
    BunnyRarity Instance);

public sealed class BunnyRarityRegistry
{
    public const int TerrariaVanillaRarityCount = 12;
    public const int MaxRegisteredRarities = 4096;

    private readonly object gate = new();
    private readonly int firstCustomType;
    private readonly List<BunnyRarityEntry> entries = new();
    private readonly Dictionary<string, int> keys = new(StringComparer.Ordinal);
    private IReadOnlyList<BunnyRarityEntry>? sealedEntries;
    private bool sealedRegistry;

    public BunnyRarityRegistry(int firstCustomType = TerrariaVanillaRarityCount)
    {
        if (firstCustomType < TerrariaVanillaRarityCount ||
            firstCustomType >= TerrariaVanillaRarityCount + MaxRegisteredRarities)
            throw new ArgumentOutOfRangeException(nameof(firstCustomType));
        this.firstCustomType = firstCustomType;
    }

    public int RarityCount
    {
        get { lock (gate) return firstCustomType + entries.Count; }
    }

    public int Register(string modId, BunnyRarity rarity)
    {
        ValidateModId(modId);
        ArgumentNullException.ThrowIfNull(rarity);

        Type type = rarity.GetType();
        if (type == typeof(BunnyRarity) || type.IsAbstract || type.ContainsGenericParameters ||
            !typeof(BunnyRarity).IsAssignableFrom(type))
            throw new ArgumentException("Expected a concrete BunnyRarity implementation", nameof(rarity));

        string className = type.FullName ?? throw new ArgumentException("Rarity has no class name", nameof(rarity));
        string key = modId + "/" + className;
        lock (gate)
        {
            if (sealedRegistry)
                throw new InvalidOperationException("The rarity registry is already sealed");
            if (rarity.Type != -1)
                throw new InvalidOperationException("A rarity instance can only be registered once");
            if (keys.ContainsKey(key))
                throw new InvalidOperationException("Duplicate rarity: " + key);
            if (entries.Count >= MaxRegisteredRarities ||
                firstCustomType + entries.Count >= TerrariaVanillaRarityCount + MaxRegisteredRarities)
                throw new InvalidOperationException("Rarity registry capacity exceeded");

            int id = firstCustomType + entries.Count;
            var entry = new BunnyRarityEntry(id, modId, className, rarity);
            keys.Add(key, id);
            entries.Add(entry);
            rarity.Type = id;
            rarity.ModId = modId;
            return id;
        }
    }

    public int TypeOf<T>(string modId) where T : BunnyRarity
    {
        ValidateModId(modId);
        string name = typeof(T).FullName ?? throw new ArgumentException("Rarity has no class name");
        lock (gate)
        {
            if (keys.TryGetValue(modId + "/" + name, out int type)) return type;
        }
        throw new KeyNotFoundException("Rarity not registered: " + modId + "/" + name);
    }

    public IReadOnlyList<BunnyRarityEntry> Seal()
    {
        lock (gate)
        {
            if (sealedRegistry)
                return sealedEntries ?? throw new InvalidOperationException("Rarity registration failed during initialization");
            sealedRegistry = true;
            foreach (BunnyRarityEntry entry in entries)
                entry.Instance.SetStaticDefaults();
            IReadOnlyList<BunnyRarityEntry> published = Array.AsReadOnly(entries.ToArray());
            Volatile.Write(ref sealedEntries, published);
            return published;
        }
    }

    public bool TryGetColor(int type, out BunnyColor color)
    {
        IReadOnlyList<BunnyRarityEntry>? published = Volatile.Read(ref sealedEntries);
        if (published == null)
            throw new InvalidOperationException("Seal rarities before game callbacks");
        int index = type - firstCustomType;
        if ((uint)index >= (uint)published.Count)
        {
            color = default;
            return false;
        }
        color = published[index].Instance.RarityColor;
        return true;
    }

    public int ResolvePrefixedRarity(int originalType, float valueMultiplier, int nativeResult)
    {
        if (!float.IsFinite(valueMultiplier) || valueMultiplier < 0f)
            throw new ArgumentOutOfRangeException(nameof(valueMultiplier));
        IReadOnlyList<BunnyRarityEntry>? published = Volatile.Read(ref sealedEntries);
        if (published == null)
            throw new InvalidOperationException("Seal rarities before game callbacks");
        int index = originalType - firstCustomType;
        if ((uint)index >= (uint)published.Count) return nativeResult;
        BunnyRarity rarity = published[index].Instance;
        int offset = PrefixOffset(valueMultiplier);
        int result = rarity.GetPrefixedRarity(offset, valueMultiplier);
        if (result > -11) result = Math.Clamp(result, -1, firstCustomType + published.Count - 1);
        return result;
    }

    public static int PrefixOffset(float valueMultiplier)
    {
        if (!float.IsFinite(valueMultiplier) || valueMultiplier < 0f)
            throw new ArgumentOutOfRangeException(nameof(valueMultiplier));
        return valueMultiplier >= 1.2f ? 2 :
            valueMultiplier >= 1.05f ? 1 :
            valueMultiplier <= 0.8f ? -2 :
            valueMultiplier <= 0.95f ? -1 : 0;
    }

    private static void ValidateModId(string modId)
    {
        if (string.IsNullOrEmpty(modId) || modId.Length > 96)
            throw new ArgumentException("Mod ID must contain 1..96 characters", nameof(modId));
        foreach (char c in modId)
        {
            if (!char.IsAsciiLetterOrDigit(c) && c != '-' && c != '_' && c != '.')
                throw new ArgumentException("Invalid mod ID character", nameof(modId));
        }
    }
}
