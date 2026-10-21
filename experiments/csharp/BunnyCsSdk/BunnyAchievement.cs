using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;

namespace BunnyLoader.Source;

public enum BunnyAchievementCategory
{
    Slayer = 0,
    Collector = 1,
    Explorer = 2,
    Challenger = 3
}

public enum BunnyAchievementEvent
{
    Manual = 0,
    ItemCraft = 1,
    ItemPickup = 2,
    NpcKilled = 3,
    TileDestroyed = 4
}

/// <summary>Uma condição fica vinculada à conquista que a criou. Alterar Value atualiza a conclusão.</summary>
public sealed class BunnyAchievementCondition
{
    private readonly BunnyAchievement owner;
    private readonly HashSet<int>? acceptedIds;
    private double value;

    internal BunnyAchievementCondition(
        BunnyAchievement owner, string key, double maximum,
        BunnyAchievementEvent source, int[]? ids)
    {
        this.owner = owner;
        Key = key;
        Maximum = maximum;
        Source = source;
        acceptedIds = ids == null ? null : new HashSet<int>(ids);
    }

    public string Key { get; }
    public double Maximum { get; }
    public BunnyAchievementEvent Source { get; }
    public bool IsCompleted => value >= Maximum;

    public double Value
    {
        get => value;
        set
        {
            if (double.IsNaN(value) || double.IsInfinity(value) || value < 0)
                throw new ArgumentOutOfRangeException(nameof(value));
            SetProgress(value, true);
        }
    }

    public void Complete() => Value = Maximum;
    public void Increase(double amount = 1)
    {
        if (double.IsNaN(amount) || double.IsInfinity(amount) || amount < 0)
            throw new ArgumentOutOfRangeException(nameof(amount));
        Value = Math.Min(Maximum, value + amount);
    }

    internal void SetProgress(double progress, bool notify)
    {
        double adjusted = Math.Min(Maximum, Math.Max(0, progress));
        if (notify && adjusted != value)
            owner.ReportProgressChanged();
        value = adjusted;
        if (notify) owner.CheckCompleted();
    }

    internal void Dispatch(BunnyAchievementEvent source, int type, int amount)
    {
        if (IsCompleted || Source != source || acceptedIds == null || !acceptedIds.Contains(type))
            return;
        if (source == BunnyAchievementEvent.ItemCraft || source == BunnyAchievementEvent.ItemPickup)
            Increase(amount);
        else Complete();
    }
}

/// <summary>
/// API C# de conquistas inspirada em ModAchievement do ExampleModBunny.
/// O registro, a persistência e os eventos são gerenciados por BunnyAchievementRegistry.
/// A ponte com Terraria.Achievements e a interface Android pertencem ao loader nativo.
/// </summary>
public abstract class BunnyAchievement
{
    private readonly List<BunnyAchievementCondition> conditions = new();
    private readonly Dictionary<string, BunnyAchievementCondition> byKey = new(StringComparer.Ordinal);
    private bool registered;
    private bool completed;
    internal Action? ProgressChanged;

    public string Name { get; internal set; } = "";
    public string FriendlyName { get; protected set; } = "";
    public string Description { get; protected set; } = "";
    public string Texture { get; protected set; } = "";
    public int Index { get; protected set; }
    public virtual BunnyAchievementCategory Category => BunnyAchievementCategory.Slayer;
    public virtual bool Hidden => false;
    public bool IsCompleted => completed;
    public IReadOnlyList<BunnyAchievementCondition> Conditions => conditions;
    public double Progress => conditions.Count == 0 ? 0 :
        conditions.Count == 1 ? conditions[0].Value / conditions[0].Maximum :
        (double)conditions.Count(c => c.IsCompleted) / conditions.Count;

    public abstract void SetStaticDefaults();
    public virtual void OnCompleted() { }
    public virtual void OnNPCKilled(int npcType) { }
    public virtual void OnItemPickup(int itemType, int stack) { }
    public virtual void OnItemCraft(int itemType, int stack) { }
    public virtual void OnTileDestroyed(int tileType) { }

    protected BunnyAchievementCondition AddCondition(string key = "Condition")
        => Add(key, 1, BunnyAchievementEvent.Manual, null);

    protected BunnyAchievementCondition AddIntCondition(string key, int maximum)
        => Add(key, maximum, BunnyAchievementEvent.Manual, null);

    protected BunnyAchievementCondition AddFloatCondition(string key, float maximum)
        => Add(key, maximum, BunnyAchievementEvent.Manual, null);

    protected BunnyAchievementCondition AddNPCKilledCondition(params int[] ids)
        => Add("NPCKilled", 1, BunnyAchievementEvent.NpcKilled, ids);

    protected BunnyAchievementCondition AddItemPickupCondition(params int[] ids)
        => Add("ItemPickup", 1, BunnyAchievementEvent.ItemPickup, ids);

    protected BunnyAchievementCondition AddItemCraftCondition(params int[] ids)
        => Add("ItemCraft", 1, BunnyAchievementEvent.ItemCraft, ids);

    protected BunnyAchievementCondition AddTileDestroyedCondition(params int[] ids)
        => Add("TileDestroyed", 1, BunnyAchievementEvent.TileDestroyed, ids);

    private BunnyAchievementCondition Add(
        string key, double maximum, BunnyAchievementEvent source, int[]? ids)
    {
        if (registered)
            throw new InvalidOperationException("As condições só podem ser criadas em SetStaticDefaults.");
        if (string.IsNullOrWhiteSpace(key) || key.Length > 96 ||
            key.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-'))
            throw new ArgumentException("A chave da condição deve conter letras, números, _ ou -.", nameof(key));
        if (!(maximum > 0) || double.IsNaN(maximum) || double.IsInfinity(maximum))
            throw new ArgumentOutOfRangeException(nameof(maximum));
        if (conditions.Count >= 32)
            throw new InvalidOperationException("Limite de 32 condições por conquista.");
        if (byKey.ContainsKey(key))
            throw new InvalidOperationException("Condição repetida: " + key);
        if (source != BunnyAchievementEvent.Manual &&
            (ids == null || ids.Length == 0 || ids.Length > 256 ||
             ids.Any(id => id < (source == BunnyAchievementEvent.NpcKilled ? -32768 : 0)) ||
             ids.Distinct().Count() != ids.Length))
            throw new ArgumentException("Tipos do evento ausentes, inválidos ou repetidos.", nameof(ids));

        var condition = new BunnyAchievementCondition(this, key, maximum, source, ids);
        conditions.Add(condition);
        byKey.Add(key, condition);
        return condition;
    }

    internal void Initialize(string identity)
    {
        if (registered || conditions.Count != 0)
            throw new InvalidOperationException("Conquista já registrada.");
        Name = identity;
        SetStaticDefaults();
        if (conditions.Count == 0)
            throw new InvalidOperationException("A conquista precisa de pelo menos uma condição.");
        if (string.IsNullOrWhiteSpace(FriendlyName))
            FriendlyName = GetType().Name;
        if (string.IsNullOrWhiteSpace(Texture) || Texture.StartsWith("/", StringComparison.Ordinal) ||
            Texture.Contains("..", StringComparison.Ordinal) || Texture.Contains('\\'))
            throw new InvalidOperationException("A textura da conquista deve ser um caminho relativo válido.");
        if (Index < 0 || Index > 65535)
            throw new InvalidOperationException("Índice de textura inválido.");
        registered = true;
        CheckCompleted(notify: false);
    }

    internal void Dispatch(BunnyAchievementEvent source, int type, int stack)
    {
        if (completed) return;
        foreach (var condition in conditions)
            condition.Dispatch(source, type, stack);
        if (completed) return;
        switch (source)
        {
            case BunnyAchievementEvent.NpcKilled: OnNPCKilled(type); break;
            case BunnyAchievementEvent.ItemPickup: OnItemPickup(type, stack); break;
            case BunnyAchievementEvent.ItemCraft: OnItemCraft(type, stack); break;
            case BunnyAchievementEvent.TileDestroyed: OnTileDestroyed(type); break;
        }
    }

    internal void ReportProgressChanged() => ProgressChanged?.Invoke();

    internal void CheckCompleted(bool notify = true)
    {
        if (completed || conditions.Count == 0 || conditions.Any(c => !c.IsCompleted))
            return;
        completed = true;
        if (notify && registered) OnCompleted();
    }

    internal void Restore(Dictionary<string, double> progress)
    {
        foreach (var condition in conditions)
        {
            condition.SetProgress(progress.TryGetValue(condition.Key, out double value) ? value : 0, false);
        }
        completed = conditions.All(c => c.IsCompleted);
    }

    internal Dictionary<string, double> Snapshot()
        => conditions.ToDictionary(c => c.Key, c => c.Value, StringComparer.Ordinal);
}

/// <summary>
/// Núcleo determinístico do loader. A integração futura deve chamar Notify* na
/// thread de eventos e Load/Save no ciclo de persistência, sem depender de QuickJS.
/// </summary>
public sealed class BunnyAchievementRegistry
{
    private const int MaxAchievements = 256;
    private const int MaxSaveLength = 1024 * 1024;
    private readonly SortedDictionary<string, BunnyAchievement> registered = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Dictionary<string, double>> stored = new(StringComparer.Ordinal);

    public IReadOnlyCollection<BunnyAchievement> Achievements => registered.Values;
    public long Notifications { get; private set; }
    public long ProgressChanges { get; private set; }
    public long CallbackFaults { get; private set; }
    public string LastError { get; private set; } = "";

    public T Register<T>(string modUid, T achievement) where T : BunnyAchievement
    {
        ArgumentNullException.ThrowIfNull(achievement);
        ValidateUid(modUid);
        string key = modUid + "/" + achievement.GetType().Name;
        if (registered.Count >= MaxAchievements || registered.ContainsKey(key))
            throw new InvalidOperationException("Conquista duplicada ou limite de 256 atingido: " + key);
        achievement.Initialize(key);
        achievement.ProgressChanged = () => ProgressChanges++;
        if (stored.TryGetValue(key, out var progress))
            achievement.Restore(progress);
        registered.Add(key, achievement);
        return achievement;
    }

    public bool Unregister(string identity)
    {
        if (!registered.Remove(identity, out var achievement)) return false;
        stored[identity] = achievement.Snapshot();
        return true;
    }

    public bool TryGet(string identity, out BunnyAchievement? achievement)
        => registered.TryGetValue(identity, out achievement);

    public void NotifyNPCKilled(int npcType, bool localPlayer = true)
        => Notify(BunnyAchievementEvent.NpcKilled, npcType, 1, localPlayer);
    public void NotifyItemPickup(int itemType, int stack, bool localPlayer = true)
        => Notify(BunnyAchievementEvent.ItemPickup, itemType, stack, localPlayer);
    public void NotifyItemCraft(int itemType, int stack, bool localPlayer = true)
        => Notify(BunnyAchievementEvent.ItemCraft, itemType, stack, localPlayer);
    public void NotifyTileDestroyed(int tileType, bool localPlayer = true)
        => Notify(BunnyAchievementEvent.TileDestroyed, tileType, 1, localPlayer);

    private void Notify(BunnyAchievementEvent source, int type, int count, bool localPlayer)
    {
        if (!localPlayer || type < (source == BunnyAchievementEvent.NpcKilled ? -32768 : 0) ||
            count <= 0) return;
        Notifications++;
        foreach (var achievement in registered.Values.ToArray())
        {
            if (achievement.IsCompleted) continue;
            try
            {
                achievement.Dispatch(source, type, count);
            }
            catch (Exception exception)
            {
                CallbackFaults++;
                LastError = achievement.Name + " " + source + " " +
                    exception.GetType().Name + ": " + exception.Message;
            }
        }
    }

    public string Save()
    {
        var snapshot = new SortedDictionary<string, Dictionary<string, double>>(stored, StringComparer.Ordinal);
        foreach (var (key, achievement) in registered)
            snapshot[key] = achievement.Snapshot();
        var output = new StringBuilder("BACH1\n");
        foreach (var (key, conditions) in snapshot)
        {
            foreach (var (conditionKey, value) in conditions.OrderBy(e => e.Key, StringComparer.Ordinal))
            {
                if (value <= 0 || !double.IsFinite(value)) continue;
                output.Append(Convert.ToHexString(Encoding.UTF8.GetBytes(key))).Append('|')
                    .Append(Convert.ToHexString(Encoding.UTF8.GetBytes(conditionKey))).Append('|')
                    .Append(value.ToString("R", CultureInfo.InvariantCulture)).Append('\n');
                if (output.Length > MaxSaveLength)
                    throw new InvalidOperationException("Dados de conquistas excedem 1 MiB.");
            }
        }
        return output.ToString();
    }

    public void Load(string? data)
    {
        if (string.IsNullOrEmpty(data))
        {
            stored.Clear();
            foreach (var achievement in registered.Values)
                achievement.Restore(new Dictionary<string, double>());
            return;
        }
        if (data.Length > MaxSaveLength || !data.StartsWith("BACH1\n", StringComparison.Ordinal))
            throw new FormatException("Formato de conquistas inválido.");

        var parsed = new Dictionary<string, Dictionary<string, double>>(StringComparer.Ordinal);
        var lines = data.Split('\n');
        int entries = 0;
        foreach (string line in lines.Skip(1))
        {
            if (line.Length == 0) continue;
            string[] fields = line.Split('|');
            if (fields.Length != 3 || ++entries > 8192)
                throw new FormatException("Registro de conquista inválido.");
            string key, condition;
            try
            {
                key = Encoding.UTF8.GetString(Convert.FromHexString(fields[0]));
                condition = Encoding.UTF8.GetString(Convert.FromHexString(fields[1]));
            }
            catch (Exception error) when (error is FormatException || error is ArgumentException)
            {
                throw new FormatException("Identificador de conquista inválido.", error);
            }
            if (!double.TryParse(fields[2], NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
                !double.IsFinite(value) || value < 0 ||
                key.Length is < 3 or > 200 || condition.Length is < 1 or > 96)
                throw new FormatException("Progresso de conquista inválido.");
            if (!parsed.TryGetValue(key, out var values))
                parsed[key] = values = new Dictionary<string, double>(StringComparer.Ordinal);
            if (!values.TryAdd(condition, value))
                throw new FormatException("Condição duplicada no salvamento.");
        }
        stored.Clear();
        foreach (var (key, conditions) in parsed)
            stored.Add(key, conditions);
        foreach (var (key, achievement) in registered)
            achievement.Restore(stored.TryGetValue(key, out var conditions)
                ? conditions : new Dictionary<string, double>());
    }

    public void Clear()
    {
        registered.Clear();
        stored.Clear();
        Notifications = 0;
        ProgressChanges = 0;
        CallbackFaults = 0;
        LastError = "";
    }

    private static void ValidateUid(string uid)
    {
        if (string.IsNullOrWhiteSpace(uid) || uid.Length > 96 ||
            uid.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '.'))
            throw new ArgumentException("UID do mod inválido.", nameof(uid));
    }
}
