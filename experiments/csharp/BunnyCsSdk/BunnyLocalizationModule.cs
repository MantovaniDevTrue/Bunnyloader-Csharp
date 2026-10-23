using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;

namespace BunnyLoader.Source;

public abstract class BunnyLocalizationModule
{
    public abstract void RegisterTranslations(BunnyLocalizationCatalog catalog);

    public virtual void OnLocalizationsLoaded() { }
}

public static class BunnyLocalizationRuntime
{
    private sealed class ModState
    {
        public string AssemblyPath = "";
        public Assembly Assembly = null!;
        public BunnyLocalizationModule[] Modules = Array.Empty<BunnyLocalizationModule>();
        public BunnyLocalizationCatalog Callback = new();
        public BunnyLocalizationCatalog Live = new();
        public string Culture = "";
        public int Revision;
        public int DeliveredRevision;
        public int PendingRevision;
        public string PendingWire = "";
        public HashSet<string> ActiveRows = new(StringComparer.Ordinal);
    }

    private static readonly object Gate = new();
    private static readonly Dictionary<string, ModState> Mods = new(StringComparer.Ordinal);

    [ThreadStatic]
    private static string? activeCulture;
    [ThreadStatic]
    private static string? activeModId;
    [ThreadStatic]
    private static BunnyLocalizationCatalog? activeRegistrations;

    public static string ActiveCultureName
    {
        [MethodImpl(MethodImplOptions.NoInlining)]
        get
        {
            if (activeCulture != null) return activeCulture;
            Assembly caller = Assembly.GetCallingAssembly();
            lock (Gate)
            {
                string? culture = null;
                foreach (ModState state in Mods.Values)
                {
                    if (!ReferenceEquals(state.Assembly, caller)) continue;
                    if (culture != null) return "";
                    culture = state.Culture;
                }
                return culture ?? "";
            }
        }
    }

    /// <summary>The registered mod UID during a callback, or the calling module's UID.</summary>
    public static string ActiveModId
    {
        [MethodImpl(MethodImplOptions.NoInlining)]
        get
        {
            if (activeModId != null) return activeModId;
            Assembly caller = Assembly.GetCallingAssembly();
            lock (Gate)
            {
                string? owner = null;
                foreach (var pair in Mods)
                {
                    if (!ReferenceEquals(pair.Value.Assembly, caller)) continue;
                    if (owner != null) return "";
                    owner = pair.Key;
                }
                return owner ?? "";
            }
        }
    }

    private static void ValidateRegistrationOwner(string key, string modId)
    {
        if (string.IsNullOrEmpty(key) ||
            (key.StartsWith("Mods.", StringComparison.Ordinal) &&
             !key.StartsWith("Mods." + modId + ".", StringComparison.Ordinal)))
            throw new ArgumentException("Cannot register a key owned by another mod", nameof(key));
    }

    private static BunnyLocalizationCatalog ResolveCatalog(string key, Assembly caller, out ModState? state)
    {
        if (activeRegistrations != null && activeModId != null)
        {
            ValidateRegistrationOwner(key, activeModId);
            state = null;
            return activeRegistrations;
        }

        state = null;
        string? owner = null;
        foreach (var pair in Mods)
        {
            if (!ReferenceEquals(pair.Value.Assembly, caller)) continue;
            if (state != null)
                throw new InvalidOperationException("Ambiguous localization module assembly");
            state = pair.Value;
            owner = pair.Key;
        }
        if (state == null || owner == null)
            throw new InvalidOperationException("Localization registration requires a loaded module assembly");
        ValidateRegistrationOwner(key, owner);
        return state.Live;
    }

    private static string RowIdentity(string key, string culture)
        => culture + "\t" + Convert.ToHexString(Encoding.UTF8.GetBytes(key));

    [MethodImpl(MethodImplOptions.NoInlining)]
    public static void Register(string key, string culture, string value)
    {
        Assembly caller = Assembly.GetCallingAssembly();
        lock (Gate)
        {
            BunnyLocalizationCatalog catalog = ResolveCatalog(key, caller, out ModState? state);
            string identity = RowIdentity(key, culture);
            if (state != null) {
                if (!state.ActiveRows.Contains(identity) && state.ActiveRows.Count >= 256)
                    throw new InvalidOperationException("Dynamic localization row limit exceeded");
            } else if (catalog.WireRowCount >= 256 &&
                       !(catalog.Texts(key)?.ContainsKey(culture) ?? false)) {
                throw new InvalidOperationException("Dynamic localization row limit exceeded");
            }
            catalog.Register(key, culture, value);
            if (state != null) {
                state.ActiveRows.Add(identity);
                state.Revision++;
            }
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public static void Register(string key, IReadOnlyDictionary<string, string> cultures)
    {
        Assembly caller = Assembly.GetCallingAssembly();
        lock (Gate)
        {
            BunnyLocalizationCatalog catalog = ResolveCatalog(key, caller, out ModState? state);
            if (cultures == null || cultures.Count == 0 || cultures.Count > 32)
                throw new ArgumentOutOfRangeException(nameof(cultures));
            if (state != null) {
                int extra = cultures.Keys.Count(culture =>
                    !state.ActiveRows.Contains(RowIdentity(key, culture)));
                if (state.ActiveRows.Count + extra > 256)
                    throw new InvalidOperationException("Dynamic localization row limit exceeded");
            } else {
                int alreadyPresent = catalog.Texts(key)?.Keys.Intersect(cultures.Keys).Count() ?? 0;
                if (catalog.WireRowCount + cultures.Count - alreadyPresent > 256)
                    throw new InvalidOperationException("Dynamic localization row limit exceeded");
            }
            catalog.Register(key, cultures);
            if (state != null) {
                foreach (string culture in cultures.Keys)
                    state.ActiveRows.Add(RowIdentity(key, culture));
                state.Revision++;
            }
        }
    }

    private static string Serialize(ModState state, BunnyLocalizationCatalog callback)
    {
        var rows = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (string row in callback.ExportWireRows())
        {
            string[] parts = row.Split('\t');
            rows[parts[1] + "\t" + parts[2]] = row;
        }
        foreach (string row in state.Live.ExportWireRows())
        {
            string[] parts = row.Split('\t');
            rows[parts[1] + "\t" + parts[2]] = row;
        }
        if (rows.Count > 256) return "ERROR|DynamicRowsExceeded";
        var wire = new StringBuilder("OK|" + state.Modules.Length + "\nBUNNY_DYNAMIC_LOCALES_V1");
        foreach (string row in rows.Values)
        {
            if (wire.Length + row.Length + 1 > 262144) return "ERROR|DynamicWireExceeded";
            wire.Append('\n').Append(row);
        }
        return wire.ToString();
    }

    public static string Dispatch(string modId, string assemblyPath, string culture)
    {
        if (!ValidId(modId) || string.IsNullOrEmpty(assemblyPath) ||
            !Path.IsPathFullyQualified(assemblyPath) || !ValidCulture(culture))
            return "ERROR|InvalidArguments";

        try
        {
            lock (Gate)
            {
                if (!Mods.TryGetValue(modId, out ModState? state) ||
                    state.AssemblyPath != assemblyPath)
                {
                    string physicalPath=BunnyRecipeLoader.AssemblyFilePath(assemblyPath);
                    var file = new FileInfo(physicalPath);
                    if (!file.Exists || file.Length <= 0 || file.Length > 32 * 1024 * 1024)
                        return "ERROR|InvalidAssembly";
                    // Native sessions supply the same revision token as quests/recipes.
                    // Sharing that collectible context also preserves mod static state and
                    // lets live registrations identify their real calling assembly.
                    Assembly assembly = assemblyPath.Contains("|bunny-revision=",StringComparison.Ordinal)
                        ? BunnyRecipeLoader.LoadAssembly(modId,assemblyPath)
                        : Assembly.LoadFrom(physicalPath); // Legacy unversioned SDK calls.

                    Type[] types = assembly.GetTypes()
                        .Where(type => type.IsClass && !type.IsAbstract &&
                            !type.ContainsGenericParameters &&
                            type.BaseType == typeof(BunnyLocalizationModule))
                        .OrderBy(type => type.FullName, StringComparer.Ordinal)
                        .ToArray();
                    if (types.Length > 32) return "ERROR|ModuleLimit";
                    var modules = new BunnyLocalizationModule[types.Length];
                    for (int i = 0; i < types.Length; ++i)
                        modules[i] = (BunnyLocalizationModule)Activator.CreateInstance(types[i])!;
                    state = new ModState { AssemblyPath = assemblyPath, Assembly = assembly, Modules = modules };
                    Mods[modId] = state;
                }

                string? previousCulture = activeCulture;
                string? previousMod = activeModId;
                BunnyLocalizationCatalog? previousCatalog = activeRegistrations;
                var registrations = new BunnyLocalizationCatalog(culture);
                activeCulture = culture;
                activeModId = modId;
                activeRegistrations = registrations;
                int faults = 0;
                try
                {
                    foreach (BunnyLocalizationModule module in state.Modules)
                    {
                        try { module.OnLocalizationsLoaded(); }
                        catch (Exception) { faults++; }
                    }
                    if (faults != 0) return "ERROR|CallbackFaults=" + faults;
                    string wire = Serialize(state, registrations);
                    if (!wire.StartsWith("OK|", StringComparison.Ordinal)) return wire;
                    state.Callback = registrations;
                    state.Culture = culture;
                    state.ActiveRows.Clear();
                    foreach (string row in wire.Split('\n').Skip(2))
                    {
                        string[] fields = row.Split('\t');
                        state.ActiveRows.Add(fields[1] + "\t" + fields[2]);
                    }
                    state.Revision++;
                    state.PendingRevision = state.Revision;
                    state.PendingWire = wire;
                    return wire;
                }
                finally
                {
                    activeRegistrations = previousCatalog;
                    activeModId = previousMod;
                    activeCulture = previousCulture;
                }
            }
        }
        catch (Exception exception)
        {
            return "ERROR|" + exception.GetType().Name;
        }
    }

    public static string Drain(string modId, string assemblyPath, string culture)
    {
        if (!ValidId(modId) || string.IsNullOrEmpty(assemblyPath) || !ValidCulture(culture))
            return "ERROR|InvalidArguments";
        lock (Gate)
        {
            if (!Mods.TryGetValue(modId, out ModState? state) ||
                state.AssemblyPath != assemblyPath || state.Culture != culture)
                return "UNCHANGED";
            if (state.Revision == state.DeliveredRevision) return "UNCHANGED";
            if (state.PendingRevision == state.Revision && state.PendingWire.Length != 0)
                return state.PendingWire;
            string wire = Serialize(state, state.Callback);
            if (wire.StartsWith("OK|", StringComparison.Ordinal))
            {
                state.PendingRevision = state.Revision;
                state.PendingWire = wire;
            }
            return wire;
        }
    }

    public static bool Acknowledge(string modId, string assemblyPath,
        string culture, string wire)
    {
        if (!ValidId(modId) || !ValidCulture(culture) ||
            string.IsNullOrEmpty(assemblyPath) || string.IsNullOrEmpty(wire))
            return false;
        lock (Gate)
        {
            if (!Mods.TryGetValue(modId, out ModState? state) ||
                state.AssemblyPath != assemblyPath || state.Culture != culture ||
                state.PendingRevision <= state.DeliveredRevision ||
                !string.Equals(state.PendingWire, wire, StringComparison.Ordinal))
                return false;
            state.DeliveredRevision = state.PendingRevision;
            state.PendingWire = "";
            return true;
        }
    }

    public static bool Unload(string modId)
    {
        if (!ValidId(modId)) return false;
        lock (Gate) return Mods.Remove(modId);
    }

    private static bool ValidId(string value)
    {
        if (string.IsNullOrEmpty(value) || value.Length > 64) return false;
        foreach (char c in value)
            if (!char.IsAsciiLetterOrDigit(c) && c != '-' && c != '_') return false;
        return true;
    }

    private static bool ValidCulture(string value)
    {
        if (string.IsNullOrEmpty(value) || value.Length < 2 || value.Length > 20) return false;
        foreach (char c in value)
            if (!char.IsAsciiLetterOrDigit(c) && c != '-') return false;
        return true;
    }
}
