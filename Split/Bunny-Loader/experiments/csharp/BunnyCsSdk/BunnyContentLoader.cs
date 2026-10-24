// API design adapted from tModLoader's ModType, ModTypeLookup and ContentInstance.
// Copyright 2019 tModLoader Team. MIT; see THIRD_PARTY_NOTICES.md.
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;

namespace BunnyLoader.Source;

/// <summary>Stable identity of managed content. Names are case sensitive, never native numeric IDs.</summary>
public interface IModType
{
    BunnyContentMod? Mod { get; }
    string Name { get; }
    string FullName { get; }
}

/// <summary>tModLoader load sides. NoSync/Both content is loaded on either execution side.</summary>
public enum ModSide { Both, Client, Server, NoSync }

/// <summary>Former local names, for lookups after renaming a managed class.</summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
public sealed class LegacyNameAttribute : Attribute
{
    public readonly string[] Names;
    public LegacyNameAttribute(params string[] names) => Names = names ?? throw new ArgumentNullException(nameof(names));
    public static IEnumerable<string> GetLegacyNamesOfType(Type type) => type
        .GetCustomAttributes<LegacyNameAttribute>(false).SelectMany(attribute => attribute.Names);
}

/// <summary>tModLoader-compatible autoload flag, inheritance, side and GetValue lookup.</summary>
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Interface, AllowMultiple = true, Inherited = true)]
public sealed class AutoloadAttribute : Attribute
{
    private static readonly AutoloadAttribute Default = new();
    public readonly bool Value;
    public ModSide Side { get; set; } = ModSide.Both;
    public bool NeedsAutoloading => Value && (Side is ModSide.Both or ModSide.NoSync || Side == BunnyContentLoader.ExecutionSide);
    public AutoloadAttribute(bool value = true) => Value = value;
    public static AutoloadAttribute GetValue(Type type) =>
        (AutoloadAttribute?)type.GetCustomAttributes(typeof(AutoloadAttribute), true).FirstOrDefault() ?? Default;
}

/// <summary>
/// Base for managed service/data content, modeled after tModLoader.ModType.
/// It does not register Terraria items, allocate game IDs or install native hooks.
/// Use Load for local resources and SetupContent/SetStaticDefaults for cross-content references.
/// </summary>
public abstract class ModType : IModType
{
    public BunnyContentMod? Mod { get; internal set; }
    public virtual string Name => GetType().Name;
    public string FullName => (Mod?.Name ?? "Terraria") + "/" + Name;
    public virtual bool IsLoadingEnabled(BunnyContentMod mod) => true;
    public virtual void Load() { }
    protected abstract void Register();
    internal void RegisterContent() => Register();
    public virtual void SetupContent() { }
    public virtual void SetStaticDefaults() { }
    public virtual void Unload() { }
    public string PrettyPrintName() => Regex.Replace(Name, "([A-Z])", " $1").Trim();
}

/// <summary>A managed content scope. Name should be the mod's stable Bunny UID.</summary>
public sealed class BunnyContentMod
{
    internal readonly List<ModType> Content = new();
    internal bool AcceptingContent;
    internal Exception? ContentFailure;
    public string Name { get; }
    public Assembly Assembly { get; }
    public bool IsLoaded { get; internal set; }
    internal BunnyContentMod(string name, Assembly assembly) { Name = name; Assembly = assembly; }

    /// <summary>Add during the Load callback or loader configure callback, before SetupContent.</summary>
    public bool AddContent(ModType instance)
    {
        ArgumentNullException.ThrowIfNull(instance);
        lock (ContentCatalog.Gate)
        {
            if (!AcceptingContent) throw new InvalidOperationException("AddContent is only valid during Load.");
            try
            {
                if (instance.Mod != null) throw new InvalidOperationException("Content already belongs to a mod.");
                // IsLoadingEnabled may execute user code, but cannot publish partial registrations.
                if (!instance.IsLoadingEnabled(this)) return false;
                ContentCatalog.ValidateSegment(instance.Name, nameof(instance.Name));
                instance.Mod = this;
                try { ContentCatalog.Add(instance); }
                catch { instance.Mod = null; throw; }
                Content.Add(instance); // Include the current object in rollback even when Load throws.
                instance.Load();
                instance.RegisterContent();
                return true;
            }
            catch (Exception error) { ContentFailure ??= error; throw; }
        }
    }

    /// <summary>Snapshot in registration order; aliases do not duplicate entries.</summary>
    public IReadOnlyList<T> GetContent<T>() where T : class
    {
        lock (ContentCatalog.Gate) return Array.AsReadOnly(Content.OfType<T>().ToArray());
    }
}

/// <summary>
/// Complete synchronous loader for managed ModType services/data. All mutations are serialized.
/// Call explicitly from a mod/host bootstrap; there is no implicit Android startup hook.
/// A successful Load returns only after every SetupContent callback completes.
/// </summary>
public static class BunnyContentLoader
{
    private static readonly Dictionary<string, BunnyContentMod> Mods = new(StringComparer.Ordinal);
    private static readonly List<BunnyContentMod> LoadOrder = new();
    private static bool mutating;
    private static ModSide executionSide = ModSide.Client;
    /// <summary>Configure once before loading content/loaders; this is host context, not Terraria netMode.</summary>
    public static ModSide ExecutionSide
    {
        get { lock (ContentCatalog.Gate) return executionSide; }
        set
        {
            if (value is not (ModSide.Client or ModSide.Server)) throw new ArgumentOutOfRangeException(nameof(value));
            lock (ContentCatalog.Gate)
            {
                if (mutating || Mods.Count != 0 || LoaderManager.HasLoaders)
                    throw new InvalidOperationException("Clear content and loaders before changing execution side.");
                executionSide = value;
            }
        }
    }

    public static BunnyContentMod Load(string modName, string assemblyPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(assemblyPath);
        return Load(modName, Assembly.LoadFrom(assemblyPath));
    }

    public static BunnyContentMod Load(string modName, Assembly assembly) => Load(modName, assembly, null);

    /// <summary>
    /// Discovers concrete closed ModType classes in FullName order, then calls configure to add
    /// manual instances. Publish/register all content before running SetupContent in that order.
    /// On any constructor, Load or SetupContent error, roll back only this mod in reverse order.
    /// </summary>
    public static BunnyContentMod Load(string modName, Assembly assembly, Action<BunnyContentMod>? configure)
    {
        ContentCatalog.ValidateSegment(modName, nameof(modName));
        ArgumentNullException.ThrowIfNull(assembly);
        lock (ContentCatalog.Gate)
        {
            BeginMutation();
            try
            {
                if (Mods.ContainsKey(modName)) throw new InvalidOperationException("Mod already loaded: " + modName);
                var mod = new BunnyContentMod(modName, assembly) { AcceptingContent = true };
                Mods.Add(modName, mod);
                try
                {
                    // GetTypes failure (including a missing dependency) is fatal, never a partial load.
                    Type[] types = assembly.GetTypes().Where(t => t.IsClass && !t.IsAbstract &&
                        !t.ContainsGenericParameters && typeof(ModType).IsAssignableFrom(t) &&
                        t.GetConstructor(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance,
                            null, Type.EmptyTypes, null) != null && AutoloadAttribute.GetValue(t).NeedsAutoloading)
                        .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
                    foreach (Type type in types)
                    {
                        var instance = (ModType)(Activator.CreateInstance(type, nonPublic: true)
                            ?? throw new InvalidOperationException("Cannot construct " + type.FullName));
                        mod.AddContent(instance);
                    }
                    configure?.Invoke(mod);
                    if (mod.ContentFailure != null) throw new InvalidOperationException("A content load failed, even though its callback caught the error.", mod.ContentFailure);
                    mod.AcceptingContent = false;
                    LoaderManager.ResizeArrays();
                    foreach (ModType instance in mod.Content) instance.SetupContent();
                    mod.IsLoaded = true;
                    LoadOrder.Add(mod);
                    return mod;
                }
                catch (Exception loadError)
                {
                    List<Exception> errors = Release(mod);
                    Mods.Remove(modName);
                    if (errors.Count == 0) throw;
                    errors.Insert(0, loadError);
                    throw new AggregateException("Content load failed; rollback also reported errors.", errors);
                }
            }
            finally { mutating = false; }
        }
    }

    public static bool IsLoaded(string modName)
    {
        ContentCatalog.ValidateSegment(modName, nameof(modName));
        lock (ContentCatalog.Gate) return Mods.TryGetValue(modName, out var mod) && mod.IsLoaded;
    }

    /// <summary>Remove one mod and every alias/singleton reference, even if Unload throws.</summary>
    public static bool Unload(string modName)
    {
        ContentCatalog.ValidateSegment(modName, nameof(modName));
        lock (ContentCatalog.Gate)
        {
            BeginMutation();
            try
            {
                if (!Mods.TryGetValue(modName, out var mod)) return false;
                List<Exception> errors = Release(mod);
                Mods.Remove(modName);
                LoadOrder.Remove(mod);
                if (errors.Count != 0) throw new AggregateException("Content unload errors.", errors);
                return true;
            }
            finally { mutating = false; }
        }
    }

    /// <summary>Unload in reverse mod load order; a faulty callback never prevents other cleanup.</summary>
    public static void UnloadAll()
    {
        lock (ContentCatalog.Gate)
        {
            BeginMutation();
            try
            {
                var errors = new List<Exception>();
                for (int i = LoadOrder.Count - 1; i >= 0; --i) errors.AddRange(Release(LoadOrder[i]));
                Mods.Clear();
                LoadOrder.Clear();
                if (errors.Count != 0) throw new AggregateException("Content unload errors.", errors);
            }
            finally { mutating = false; }
        }
    }

    private static void BeginMutation()
    {
        // Hooks may query content or AddContent during Load, but cannot recursively load/unload scopes.
        if (mutating) throw new InvalidOperationException("Recursive content load/unload is not allowed.");
        mutating = true;
    }

    private static List<Exception> Release(BunnyContentMod mod)
    {
        mod.AcceptingContent = false;
        mod.IsLoaded = false;
        var errors = new List<Exception>();
        for (int i = mod.Content.Count - 1; i >= 0; --i)
        {
            ModType instance = mod.Content[i];
            try { instance.Unload(); }
            catch (Exception error) { errors.Add(error); }
            finally { ContentCatalog.Remove(instance); instance.Mod = null; }
        }
        mod.Content.Clear();
        return errors;
    }
}

/// <summary>
/// Exact CLR type singleton semantics from tModLoader: one registration sets Instance;
/// two or more set Instances and make Instance null. No base-type singleton is synthesized.
/// Properties query the live catalog; generic statics never retain unloaded mod objects.
/// </summary>
public static class ContentInstance<T> where T : class
{
    public static T? Instance
    {
        get { lock (ContentCatalog.Gate) return ContentCatalog.Singleton(typeof(T)) as T; }
    }
    public static IReadOnlyList<T>? Instances
    {
        get { lock (ContentCatalog.Gate) return ContentCatalog.Instances<T>(); }
    }
}

/// <summary>Managed content API with tModLoader-style names, under the Bunny SDK namespace.</summary>
public static class ModContent
{
    public static T? GetInstance<T>() where T : class => ContentInstance<T>.Instance;
    public static T Find<T>(string fullName) where T : class, IModType => TryFind<T>(fullName, out var value)
        ? value : throw new KeyNotFoundException("Managed content not found: " + fullName + " (" + typeof(T).FullName + ")");
    public static T Find<T>(string modName, string contentName) where T : class, IModType => Find<T>(ContentCatalog.Key(modName, contentName));
    public static bool TryFind<T>(string modName, string contentName, [NotNullWhen(true)] out T? value) where T : class, IModType =>
        TryFind(ContentCatalog.Key(modName, contentName), out value);
    public static bool TryFind<T>(string fullName, [NotNullWhen(true)] out T? value) where T : class, IModType
    {
        ContentCatalog.ValidateKey(fullName);
        lock (ContentCatalog.Gate)
        {
            return ModTypeLookup<T>.TryGetValue(fullName, out value);
        }
    }
    /// <summary>Immutable snapshot in registration order, including derived types, excluding aliases.</summary>
    public static IReadOnlyList<T> GetContent<T>() where T : class
    {
        lock (ContentCatalog.Gate) return Array.AsReadOnly(ContentCatalog.All.OfType<T>().ToArray());
    }
}

internal static class ContentCatalog
{
    internal static readonly object Gate = new();
    internal static readonly List<ModType> All = new();
    private static readonly Dictionary<IModType, List<Action>> Cleanup = new(ReferenceEqualityComparer.Instance);
    private sealed class Bucket
    {
        internal readonly List<ModType> Items = new();
        internal object? Snapshot;
    }
    private static readonly Dictionary<Type, Bucket> ByType = new();

    internal static void ValidateSegment(string value, string parameter)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > 160 ||
            value.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-' || c == '.')))
            throw new ArgumentException("Content identity must be 1..160 ASCII letters/digits, '_', '-' or '.'.", parameter);
    }
    internal static string Key(string mod, string name)
    { ValidateSegment(mod, nameof(mod)); ValidateSegment(name, nameof(name)); return mod + "/" + name; }
    internal static void ValidateKey(string fullName)
    {
        ArgumentNullException.ThrowIfNull(fullName);
        int slash = fullName.IndexOf('/');
        if (slash <= 0 || slash != fullName.LastIndexOf('/')) throw new ArgumentException("Expected Mod/Name.", nameof(fullName));
        Key(fullName[..slash], fullName[(slash + 1)..]);
    }
    internal static bool Contains(IModType instance) => Cleanup.ContainsKey(instance);
    internal static void OnRemove(IModType instance, Action cleanup) => Cleanup[instance].Add(cleanup);
    internal static void Add(ModType instance)
    {
        Cleanup.Add(instance, new());
        All.Add(instance);
        Type type = instance.GetType();
        if (!ByType.TryGetValue(type, out var typed)) ByType.Add(type, typed = new Bucket());
        typed.Items.Add(instance);
        typed.Snapshot = null;
    }
    internal static object? Singleton(Type type) => ByType.TryGetValue(type, out var bucket) && bucket.Items.Count == 1
        ? bucket.Items[0] : null;
    internal static IReadOnlyList<T>? Instances<T>() where T : class
    {
        if (!ByType.TryGetValue(typeof(T), out var bucket) || bucket.Items.Count < 2) return null;
        return (IReadOnlyList<T>)(bucket.Snapshot ??= Array.AsReadOnly(bucket.Items.Cast<T>().ToArray()));
    }
    internal static void Remove(ModType instance)
    {
        if (!Cleanup.Remove(instance, out var cleanup)) return;
        foreach (Action action in cleanup) action();
        All.RemoveAll(i => ReferenceEquals(i, instance));
        var typed = ByType[instance.GetType()];
        typed.Items.RemoveAll(i => ReferenceEquals(i, instance));
        typed.Snapshot = null;
        if (typed.Items.Count == 0) ByType.Remove(instance.GetType());
    }
}
