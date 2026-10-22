// Test-only host scaffolding. Loader.cs, ModTypeLookup.cs, ContentInstance.cs,
// IModType.cs and LegacyNameAttribute.cs next to this file match upstream after removing the nullable directive.
#nullable disable
using System;
namespace Terraria.Localization
{
    internal static class Language
    {
        internal static string GetTextValue(string key, params object[] args) => key + ":" + string.Join(",", args);
    }
}
namespace Terraria.ModLoader.Core
{
    internal static class TypeCaching
    {
        internal static event Action OnClear;
        internal static void Clear() => OnClear?.Invoke();
    }
}
namespace Terraria.ModLoader
{
    public sealed class Mod { public string Name { get; set; } }
    public abstract class ModType : IModType
    {
        public Mod Mod { get; set; }
        public virtual string Name => GetType().Name;
        public string FullName => (Mod?.Name ?? "Terraria") + "/" + Name;
    }
    [AttributeUsage(AttributeTargets.Class)]
    internal sealed class AutoloadAttribute : Attribute
    {
        internal bool NeedsAutoloading => true;
        internal static AutoloadAttribute GetValue(Type type) => new();
    }
}
