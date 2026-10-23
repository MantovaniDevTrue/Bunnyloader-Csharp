using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

public enum BunnyAchievementCategory { Slayer, Collector, Explorer, Challenger }
public enum BunnyAchievementEventKind { NpcKilled, ItemPickup, ItemCraft, TileDestroyed }
public enum BunnyAchievementConditionKind { Flag, Integer, Float, NpcKilled, ItemPickup, ItemCraft, TileDestroyed }

public readonly record struct BunnyAchievementEvent(
    BunnyAchievementEventKind Kind, int ContentType, int Stack = 1, bool IsLocalPlayer = true);

public sealed class BunnyAchievementCondition
{
    private float value;
    internal BunnyAchievementCondition(string key, BunnyAchievementConditionKind kind,
        float maximum, int[] ids)
    {
        Key = key;
        Kind = kind;
        Maximum = maximum;
        ContentTypes = Array.AsReadOnly(ids);
    }

    public string Key { get; }
    public BunnyAchievementConditionKind Kind { get; }
    public float Maximum { get; }
    public IReadOnlyList<int> ContentTypes { get; }
    public float Value
    {
        get => value;
        set
        {
            if (!float.IsFinite(value) || value < 0f ||
                (Kind != BunnyAchievementConditionKind.Float && value != MathF.Truncate(value)))
                throw new ArgumentOutOfRangeException(nameof(value));
            this.value = MathF.Min(value, Maximum);
        }
    }
    public bool IsCompleted => value >= Maximum;
    public void Complete() => Value = Maximum;

    internal void Apply(BunnyAchievementEvent e)
    {
        if (IsCompleted || !e.IsLocalPlayer || !ContentTypes.Contains(e.ContentType)) return;
        bool matches = (Kind, e.Kind) switch
        {
            (BunnyAchievementConditionKind.NpcKilled, BunnyAchievementEventKind.NpcKilled) => true,
            (BunnyAchievementConditionKind.ItemPickup, BunnyAchievementEventKind.ItemPickup) => true,
            (BunnyAchievementConditionKind.ItemCraft, BunnyAchievementEventKind.ItemCraft) => true,
            (BunnyAchievementConditionKind.TileDestroyed, BunnyAchievementEventKind.TileDestroyed) => true,
            _ => false
        };
        if (matches) Value += Kind == BunnyAchievementConditionKind.ItemPickup
            ? Math.Max(e.Stack, 1) : 1;
    }
}

public abstract class BunnyAchievement
{
    private readonly List<BunnyAchievementCondition> conditions = new();
    private readonly HashSet<string> keys = new(StringComparer.Ordinal);

    public string Name { get; internal set; } = "";
    public string Texture { get; set; } = "";
    public string FriendlyName { get; set; } = "";
    public string Description { get; set; } = "";
    public virtual int Index => 0;
    public virtual BunnyAchievementCategory Category => BunnyAchievementCategory.Slayer;
    public virtual bool Hidden => false;
    public bool IsCompleted { get; internal set; }
    public IReadOnlyList<BunnyAchievementCondition> Conditions => conditions;
    public abstract void SetStaticDefaults();
    public virtual void OnCompleted() { }
    public virtual void OnNPCKilled(BunnyAchievementEvent e) { }
    public virtual void OnItemPickup(BunnyAchievementEvent e) { }
    public virtual void OnItemCraft(BunnyAchievementEvent e) { }
    public virtual void OnTileDestroyed(BunnyAchievementEvent e) { }

    public BunnyAchievementCondition AddCondition(string key = "Condition")
        => Add(key, BunnyAchievementConditionKind.Flag, 1, Array.Empty<int>());
    public BunnyAchievementCondition AddIntCondition(int maximum, string key = "Condition")
        => Add(key, BunnyAchievementConditionKind.Integer, maximum, Array.Empty<int>());
    public BunnyAchievementCondition AddFloatCondition(float maximum, string key = "Condition")
        => Add(key, BunnyAchievementConditionKind.Float, maximum, Array.Empty<int>());
    public BunnyAchievementCondition AddNPCKilledCondition(params int[] ids)
        => AddAutomatic(BunnyAchievementConditionKind.NpcKilled, ids);
    public BunnyAchievementCondition AddItemPickupCondition(params int[] ids)
        => AddAutomatic(BunnyAchievementConditionKind.ItemPickup, ids);
    public BunnyAchievementCondition AddItemCraftCondition(params int[] ids)
        => AddAutomatic(BunnyAchievementConditionKind.ItemCraft, ids);
    public BunnyAchievementCondition AddTileDestroyedCondition(params int[] ids)
        => AddAutomatic(BunnyAchievementConditionKind.TileDestroyed, ids);

    private BunnyAchievementCondition AddAutomatic(BunnyAchievementConditionKind kind, int[] ids)
    {
        ArgumentNullException.ThrowIfNull(ids);
        if (ids.Length == 0 || ids.Length > 1024 || ids.Any(id => id < -65 || id > 32767))
            throw new ArgumentOutOfRangeException(nameof(ids));
        return Add(kind.ToString(), kind, 1, ids.Distinct().ToArray());
    }

    private BunnyAchievementCondition Add(string key, BunnyAchievementConditionKind kind,
        float maximum, int[] ids)
    {
        if (string.IsNullOrEmpty(key) || key.Length > 72 ||
            key.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '_') || !keys.Add(key))
            throw new ArgumentException("Invalid or duplicate condition key", nameof(key));
        if (!float.IsFinite(maximum) || maximum <= 0 ||
            (kind != BunnyAchievementConditionKind.Float && maximum != MathF.Truncate(maximum)))
            throw new ArgumentOutOfRangeException(nameof(maximum));
        if (conditions.Count >= 32) throw new InvalidOperationException("Too many conditions");
        var condition = new BunnyAchievementCondition(key, kind, maximum, ids);
        conditions.Add(condition);
        return condition;
    }

    internal void Process(BunnyAchievementEvent e)
    {
        if (IsCompleted || !e.IsLocalPlayer) return;
        foreach (var condition in conditions) condition.Apply(e);
        switch (e.Kind)
        {
            case BunnyAchievementEventKind.NpcKilled: OnNPCKilled(e); break;
            case BunnyAchievementEventKind.ItemPickup: OnItemPickup(e); break;
            case BunnyAchievementEventKind.ItemCraft: OnItemCraft(e); break;
            case BunnyAchievementEventKind.TileDestroyed: OnTileDestroyed(e); break;
            default: throw new ArgumentOutOfRangeException(nameof(e));
        }
        if (conditions.Count > 0 && conditions.All(c => c.IsCompleted))
        {
            IsCompleted = true;
            OnCompleted();
        }
    }
}

public sealed record BunnyAchievementSnapshot(string Name, string[] Keys,
    BunnyAchievementConditionKind[] Kinds, float[] Values);

public sealed class BunnyAchievementRegistry
{
    private sealed class Mod
    {
        public string Revision = "";
        public Assembly Assembly = null!;
        public Dictionary<string, BunnyAchievement> Entries = new(StringComparer.Ordinal);
    }

    private readonly object gate = new();
    private readonly Dictionary<string, Mod> mods = new(StringComparer.Ordinal);

    public int Register(string modId, string revision, Assembly assembly)
    {
        ValidateIdentity(modId, revision);
        ArgumentNullException.ThrowIfNull(assembly);
        lock (gate)
        {
            if (mods.TryGetValue(modId, out var old) && old.Revision == revision &&
                ReferenceEquals(old.Assembly, assembly)) return old.Entries.Count;
            var types = assembly.GetTypes()
                .Where(t => t.IsClass && !t.IsAbstract && !t.ContainsGenericParameters &&
                    t.BaseType == typeof(BunnyAchievement))
                .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
            if (types.Length > 128) throw new InvalidOperationException("Too many achievements");
            var next = new Mod { Revision = revision, Assembly = assembly };
            foreach (var type in types)
            {
                var instance = (BunnyAchievement)(Activator.CreateInstance(type)
                    ?? throw new InvalidOperationException("Achievement instance creation failed"));
                if (!next.Entries.TryAdd(type.Name, instance))
                    throw new InvalidOperationException("Duplicate achievement class");
                instance.Name = modId + "/" + type.Name;
                instance.Texture = "Content/Achievements/" + type.Name;
                instance.SetStaticDefaults();
                if (instance.Conditions.Count == 0 || instance.Index < 0 || instance.Index > 255 ||
                    !Enum.IsDefined(instance.Category) || !ValidTexture(instance.Texture))
                    throw new InvalidOperationException("Invalid achievement definition: " + type.Name);
            }
            mods[modId] = next;
            return next.Entries.Count;
        }
    }

    public bool Unload(string modId, string revision)
    {
        ValidateIdentity(modId, revision);
        lock (gate)
            return mods.TryGetValue(modId, out var state) &&
                state.Revision == revision && mods.Remove(modId);
    }

    public BunnyAchievement? Find(string modId, string name)
    {
        lock (gate)
            return mods.TryGetValue(modId, out var state) &&
                state.Entries.TryGetValue(name, out var result) ? result : null;
    }

    public int Dispatch(string modId, BunnyAchievementEvent e)
    {
        if (!Enum.IsDefined(e.Kind) || e.ContentType < -65 || e.ContentType > 32767 ||
            e.Stack < 0 || e.Stack > 999999)
            throw new ArgumentOutOfRangeException(nameof(e));
        lock (gate)
        {
            if (!mods.TryGetValue(modId, out var state)) return 0;
            int completed = 0;
            foreach (var achievement in state.Entries.Values)
            {
                bool wasCompleted = achievement.IsCompleted;
                achievement.Process(e);
                if (!wasCompleted && achievement.IsCompleted) completed++;
            }
            return completed;
        }
    }

    public IReadOnlyList<BunnyAchievementSnapshot> Snapshot(string modId)
    {
        lock (gate)
        {
            if (!mods.TryGetValue(modId, out var state))
                return Array.Empty<BunnyAchievementSnapshot>();
            return state.Entries.Values.OrderBy(a => a.Name, StringComparer.Ordinal)
                .Select(a => new BunnyAchievementSnapshot(a.Name,
                    a.Conditions.Select(c => c.Key).ToArray(),
                    a.Conditions.Select(c => c.Kind).ToArray(),
                    a.Conditions.Select(c => c.Value).ToArray())).ToArray();
        }
    }

    internal IReadOnlyList<BunnyAchievement> Definitions(string modId)
    {
        lock (gate)
            return mods.TryGetValue(modId, out var state)
                ? state.Entries.Values.OrderBy(item => item.Name, StringComparer.Ordinal).ToArray()
                : Array.Empty<BunnyAchievement>();
    }

    public int Restore(string modId, IReadOnlyList<BunnyAchievementSnapshot> saved)
    {
        ArgumentNullException.ThrowIfNull(saved);
        lock (gate)
        {
            if (!mods.TryGetValue(modId, out var state)) return 0;
            int restored = 0;
            foreach (var snapshot in saved)
            {
                if (snapshot == null || !snapshot.Name.StartsWith(modId + "/", StringComparison.Ordinal) ||
                    !state.Entries.TryGetValue(snapshot.Name[(modId.Length + 1)..], out var item) ||
                    snapshot.Keys == null || snapshot.Kinds == null || snapshot.Values == null ||
                    item.Conditions.Count != snapshot.Keys.Length ||
                    item.Conditions.Count != snapshot.Kinds.Length ||
                    item.Conditions.Count != snapshot.Values.Length) continue;
                bool valid = true;
                for (int i = 0; i < item.Conditions.Count; i++)
                {
                    var c = item.Conditions[i];
                    float value = snapshot.Values[i];
                    if (c.Key != snapshot.Keys[i] || c.Kind != snapshot.Kinds[i] ||
                        !float.IsFinite(value) || value < 0 ||
                        (c.Kind != BunnyAchievementConditionKind.Float && value != MathF.Truncate(value)))
                    {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;
                for (int i = 0; i < item.Conditions.Count; i++)
                    item.Conditions[i].Value = snapshot.Values[i];
                item.IsCompleted = item.Conditions.All(c => c.IsCompleted);
                restored++;
            }
            return restored;
        }
    }

    private static bool ValidTexture(string path)
        => !string.IsNullOrWhiteSpace(path) && path.Length <= 200 &&
           !path.StartsWith('/') && !path.Contains("..") &&
           !path.Contains('\\') && !path.Contains(':') &&
           !path.Any(c => c < 32 || c == 127);

    private static void ValidateIdentity(string modId, string revision)
    {
        if (string.IsNullOrEmpty(modId) || modId.Length > 96 ||
            modId.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '-' && c != '_' && c != '.') ||
            string.IsNullOrWhiteSpace(revision) || revision.Length > 72 ||
            revision.Any(c => c < 33 || c > 126))
            throw new ArgumentException("Invalid mod identity/revision");
    }
}
