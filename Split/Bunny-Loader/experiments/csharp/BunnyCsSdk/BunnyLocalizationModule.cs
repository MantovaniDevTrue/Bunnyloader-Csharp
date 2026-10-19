using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
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
        public BunnyLocalizationModule[] Modules = Array.Empty<BunnyLocalizationModule>();
    }

    private static readonly object Gate = new();
    private static readonly Dictionary<string, ModState> Mods = new(StringComparer.Ordinal);

    [ThreadStatic]
    private static string? activeCulture;
    [ThreadStatic]
    private static string? activeModId;
    [ThreadStatic]
    private static BunnyLocalizationCatalog? activeRegistrations;

    public static string ActiveCultureName => activeCulture ?? "";

    private static void ValidateRegistrationOwner(string key)
    {
        if (activeRegistrations == null || activeModId == null)
            throw new InvalidOperationException("Localization registration requires OnLocalizationsLoaded");
        if (string.IsNullOrEmpty(key) ||
            (key.StartsWith("Mods.", StringComparison.Ordinal) &&
             !key.StartsWith("Mods." + activeModId + ".", StringComparison.Ordinal)))
            throw new ArgumentException("Cannot register a key owned by another mod", nameof(key));
    }

    public static void Register(string key, string culture, string value)
    {
        ValidateRegistrationOwner(key);
        activeRegistrations!.Register(key, culture, value);
        if (activeRegistrations.ExportWireRows().Count > 256)
            throw new InvalidOperationException("Dynamic localization row limit exceeded");
    }

    public static void Register(string key, IReadOnlyDictionary<string, string> cultures)
    {
        ValidateRegistrationOwner(key);
        if (cultures == null || cultures.Count > 32)
            throw new ArgumentOutOfRangeException(nameof(cultures));
        var next = activeRegistrations.ExportWireRows().Count;
        if (next + cultures.Count > 256)
            throw new InvalidOperationException("Dynamic localization row limit exceeded");
        activeRegistrations.Register(key, cultures);
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
                    var file = new FileInfo(assemblyPath);
                    if (!file.Exists || file.Length <= 0 || file.Length > 32 * 1024 * 1024)
                        return "ERROR|InvalidAssembly";
                    Assembly assembly = Assembly.LoadFrom(assemblyPath);
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
                    state = new ModState { AssemblyPath = assemblyPath, Modules = modules };
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
                    IReadOnlyList<string> rows = registrations.ExportWireRows();
                    if (rows.Count > 256) return "ERROR|DynamicRowsExceeded";
                    var wire = new StringBuilder("OK|" + state.Modules.Length + "\nBUNNY_DYNAMIC_LOCALES_V1");
                    foreach (string row in rows)
                    {
                        if (wire.Length + 1 + row.Length > 262144) return "ERROR|DynamicWireExceeded";
                        wire.Append('\n').Append(row);
                    }
                    return wire.ToString();
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
