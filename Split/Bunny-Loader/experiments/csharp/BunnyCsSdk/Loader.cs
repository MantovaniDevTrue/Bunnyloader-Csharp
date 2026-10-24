// Adapted from tModLoader 1.4.5 Loader.cs, commit fcafebc0b792b90e110bac7c1fc67cac81cb60ed.
// Copyright 2019 tModLoader Team. MIT; see THIRD_PARTY_NOTICES.md.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>Base for managed loaders. Reserved IDs belong to this loader, not Terraria's IL2CPP tables.</summary>
public abstract class Loader : ILoader
{
    private int vanillaCount;
    public int VanillaCount
    {
        get => vanillaCount;
        set => Initialize(value);
    }
    public int TotalCount { get; private set; }

    /// <summary>Initialize before registering any mod content.</summary>
    public void Initialize(int vanillaCount)
    {
        lock (ContentCatalog.Gate)
        {
            if (vanillaCount < 0) throw new ArgumentOutOfRangeException(nameof(vanillaCount));
            if (TotalCount != this.vanillaCount) throw new InvalidOperationException("Cannot reinitialize a populated loader.");
            this.vanillaCount = vanillaCount;
            TotalCount = vanillaCount;
        }
    }

    protected int Reserve()
    {
        if (TotalCount == int.MaxValue) throw new InvalidOperationException("Loader ID space exhausted.");
        return TotalCount++;
    }
    public virtual void ResizeArrays() { }
    public virtual void Unload() { lock (ContentCatalog.Gate) TotalCount = vanillaCount; }
}

/// <summary>
/// tModLoader's indexed Loader&lt;T&gt; with per-mod cleanup. Individual unload leaves holes so IDs
/// of other loaded mods never shift. Full Unload resets the count to VanillaCount.
/// </summary>
public abstract class Loader<T> : Loader where T : ModType
{
    private readonly List<T?> list = new();
    public int Register(T obj)
    {
        ArgumentNullException.ThrowIfNull(obj);
        lock (ContentCatalog.Gate)
        {
            if (TotalCount == int.MaxValue) throw new InvalidOperationException("Loader ID space exhausted.");
            // Validate names before reserving an ID: a failed registration consumes no ID.
            ModTypeLookup<T>.Register(obj);
            int type = Reserve();
            int index = list.Count;
            list.Add(obj);
            ContentCatalog.OnRemove(obj, () => {
                if (index < list.Count && ReferenceEquals(list[index], obj)) list[index] = null;
            });
            return type;
        }
    }
    public T? Get(int id)
    {
        lock (ContentCatalog.Gate)
        {
            if (id < VanillaCount || id >= TotalCount) return null;
            return list[id - VanillaCount];
        }
    }
    public override void Unload()
    {
        lock (ContentCatalog.Gate)
        {
            // Upstream calls Unload after content teardown. Refuse destructive reset of live content.
            if (list.Any(item => item != null)) throw new InvalidOperationException("Unload mod content before resetting its loader.");
            base.Unload();
            list.Clear();
        }
    }
}

public interface ILoader
{
    void ResizeArrays() { }
    void Unload() { }
}

/// <summary>
/// Manager from tModLoader.Loader.cs. Bunny exposes host lifecycle entrypoints and caches lazy
/// Get results (upstream returns a temporary instance before AutoLoad). No game hooks are installed.
/// </summary>
public static class LoaderManager
{
    private static readonly Dictionary<Type, ILoader> loadersByType = new();
    private static bool mutating;
    private static bool constructing;
    internal static bool HasLoaders => loadersByType.Count != 0;

    public static T Get<T>() where T : class, ILoader
    {
        lock (ContentCatalog.Gate)
        {
            if (loadersByType.TryGetValue(typeof(T), out var result)) return (T)result;
            if (mutating || constructing) throw new InvalidOperationException("Cannot create a loader from a loader lifecycle callback/constructor.");
            constructing = true;
            try
            {
                var loader = (T)(Activator.CreateInstance(typeof(T), nonPublic: true)
                    ?? throw new InvalidOperationException("Cannot construct loader " + typeof(T).FullName));
                loadersByType.Add(typeof(T), loader);
                return loader;
            }
            finally { constructing = false; }
        }
    }

    public static void AutoLoad() => AutoLoad(typeof(LoaderManager).Assembly);
    public static void AutoLoad(Assembly assembly)
    {
        ArgumentNullException.ThrowIfNull(assembly);
        lock (ContentCatalog.Gate)
        {
            if (mutating || constructing) throw new InvalidOperationException("Recursive loader discovery.");
            constructing = true;
            var pending = new List<KeyValuePair<Type, ILoader>>();
            try
            {
                foreach (Type type in assembly.GetTypes().Where(type => type.IsClass && !type.IsAbstract &&
                    !type.ContainsGenericParameters && typeof(ILoader).IsAssignableFrom(type) &&
                    type.GetConstructor(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance,
                        null, Type.EmptyTypes, null) != null && AutoloadAttribute.GetValue(type).NeedsAutoloading)
                    .OrderBy(type => type.FullName, StringComparer.Ordinal))
                {
                    if (loadersByType.ContainsKey(type)) continue;
                    var loader = (ILoader)(Activator.CreateInstance(type, nonPublic: true)
                        ?? throw new InvalidOperationException("Cannot construct loader " + type.FullName));
                    pending.Add(new(type, loader));
                }
                foreach (var entry in pending) loadersByType.Add(entry.Key, entry.Value);
            }
            catch
            {
                // Constructors own their external resources; cleanup every completed construction.
                foreach (var entry in pending.AsEnumerable().Reverse())
                    try { entry.Value.Unload(); } catch { /* Preserve the discovery error. */ }
                throw;
            }
            finally { constructing = false; }
        }
    }

    /// <summary>Release loaders owned by an unloaded assembly; refuse to discard live registrations.</summary>
    public static void ReleaseAssembly(Assembly assembly)
    {
        ArgumentNullException.ThrowIfNull(assembly);
        lock (ContentCatalog.Gate)
        {
            if (mutating || constructing) throw new InvalidOperationException("Recursive loader lifecycle.");
            mutating = true;
            try
            {
                var errors = new List<Exception>();
                foreach (var entry in loadersByType.Where(entry => entry.Key.Assembly == assembly).Reverse().ToArray())
                {
                    try { entry.Value.Unload(); loadersByType.Remove(entry.Key); }
                    catch (Exception error) { errors.Add(error); }
                }
                if (errors.Count != 0) throw new AggregateException("Assembly loader unload errors.", errors);
            }
            finally { mutating = false; }
        }
    }

    public static void ResizeArrays() => ForEach(loader => loader.ResizeArrays());
    public static void Unload() => ForEach(loader => loader.Unload());

    /// <summary>Clear manager references after all mod content has been unloaded.</summary>
    public static void Clear()
    {
        lock (ContentCatalog.Gate)
        {
            if (mutating || constructing) throw new InvalidOperationException("Recursive loader lifecycle.");
            if (ContentCatalog.All.Count != 0) throw new InvalidOperationException("Unload content before clearing loaders.");
            try { Unload(); }
            finally { loadersByType.Clear(); }
        }
    }
    private static void ForEach(Action<ILoader> action)
    {
        lock (ContentCatalog.Gate)
        {
            if (mutating || constructing) throw new InvalidOperationException("Recursive loader lifecycle.");
            mutating = true;
            try
            {
                var errors = new List<Exception>();
                foreach (ILoader loader in loadersByType.Values)
                    try { action(loader); } catch (Exception error) { errors.Add(error); }
                if (errors.Count != 0) throw new AggregateException("Loader lifecycle errors.", errors);
            }
            finally { mutating = false; }
        }
    }
}
