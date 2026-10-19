using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

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

    public static string ActiveCultureName => activeCulture ?? "";

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

                string? previous = activeCulture;
                activeCulture = culture;
                int faults = 0;
                try
                {
                    foreach (BunnyLocalizationModule module in state.Modules)
                    {
                        try { module.OnLocalizationsLoaded(); }
                        catch (Exception) { faults++; }
                    }
                }
                finally
                {
                    activeCulture = previous;
                }
                return faults == 0 ? "OK|" + state.Modules.Length : "ERROR|CallbackFaults=" + faults;
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
