using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

public static class BunnyRarityRuntime
{
    private sealed class ModRarities
    {
        public readonly string AssemblyPath;
        public readonly IReadOnlyDictionary<string, BunnyRarity> ByName;
        public readonly IReadOnlyDictionary<int, BunnyRarity> ById;

        public ModRarities(string assemblyPath, Dictionary<string, BunnyRarity> byName,
            Dictionary<int, BunnyRarity> byId)
        {
            AssemblyPath = assemblyPath;
            ByName = byName;
            ById = byId;
        }
    }

    private static readonly Dictionary<string, ModRarities> Mods = new(StringComparer.Ordinal);
    public static BunnyColor DiscoColor { get; private set; } = new(255, 255, 255);

    public static int TypeOf<T>(string modId) where T : BunnyRarity
    {
        if (!Mods.TryGetValue(modId, out ModRarities? mod) ||
            !mod.ByName.TryGetValue(typeof(T).Name, out BunnyRarity? rarity) ||
            !string.Equals(rarity.GetType().FullName, typeof(T).FullName, StringComparison.Ordinal))
            throw new KeyNotFoundException("Rarity not registered for mod " + modId);
        return rarity.Type;
    }

    public static string Dispatch(string modId, string assemblyPath, string owner, int index,
        int kind, string snapshot)
    {
        try
        {
            if (string.IsNullOrEmpty(modId) || modId.Length > 96 ||
                string.IsNullOrEmpty(assemblyPath) || assemblyPath.Length > 4096 ||
                owner == null || owner.Length > 72 || snapshot == null ||
                snapshot.Length > 32768 || kind < 0 || kind > 3 || index != 0)
                return "ERROR|Stage=ValidateRarity Reason=InvalidTransport";

            if (kind == 2)
            {
                if (owner.Length != 0 || snapshot.Length == 0)
                    return "ERROR|Stage=BindRarity Reason=ExpectedAllRarities";
                var entries = new Dictionary<string, int>(StringComparer.Ordinal);
                foreach (string field in snapshot.Split(';'))
                {
                    int delimiter = field.IndexOf(':');
                    if (delimiter < 1 || delimiter >= field.Length - 1)
                        return "ERROR|Stage=BindRarity Reason=InvalidIdentity";
                    string name = field[..delimiter];
                    if (name.Length > 72 || !name.All(c => char.IsAsciiLetterOrDigit(c) || c == '_') ||
                        !int.TryParse(field[(delimiter + 1)..], NumberStyles.None,
                            CultureInfo.InvariantCulture, out int type) ||
                        type < BunnyRarityRegistry.TerrariaVanillaRarityCount ||
                        type >= BunnyRarityRegistry.TerrariaVanillaRarityCount +
                            BunnyRarityRegistry.MaxRegisteredRarities ||
                        !entries.TryAdd(name, type) || entries.Count > BunnyRarityRegistry.MaxRegisteredRarities)
                        return "ERROR|Stage=BindRarity Reason=InvalidOrDuplicateIdentity";
                }
                if (entries.Count == 0 || Mods.ContainsKey(modId))
                    return "ERROR|Stage=BindRarity Reason=EmptyOrAlreadyBound";
                Assembly assembly = Assembly.Load(File.ReadAllBytes(assemblyPath));
                var byName = new Dictionary<string, BunnyRarity>(StringComparer.Ordinal);
                var byId = new Dictionary<int, BunnyRarity>();
                Type[] classes = assembly.GetTypes()
                    .Where(type => type.IsClass && !type.IsAbstract &&
                        !type.ContainsGenericParameters && typeof(BunnyRarity).IsAssignableFrom(type))
                    .ToArray();
                if (classes.Length != entries.Count)
                    return "ERROR|Stage=BindRarity Reason=ClassCountMismatch";
                foreach (Type type in classes)
                {
                    if (!entries.TryGetValue(type.Name, out int id) ||
                        byName.ContainsKey(type.Name) || byId.ContainsKey(id))
                        return "ERROR|Stage=BindRarity Reason=AmbiguousClassOrId";
                    var instance = (BunnyRarity)(Activator.CreateInstance(type) ??
                        throw new InvalidOperationException("Cannot instantiate rarity " + type.FullName));
                    instance.Type = id;
                    instance.ModId = modId;
                    byName.Add(type.Name, instance);
                    byId.Add(id, instance);
                }
                Mods.Add(modId, new ModRarities(assemblyPath, byName, byId));
                try
                {
                    foreach (var instance in byName.OrderBy(pair => pair.Value.Type).Select(pair => pair.Value))
                        instance.SetStaticDefaults();
                }
                catch
                {
                    Mods.Remove(modId);
                    throw;
                }
                return "READY|" + byName.Count.ToString(CultureInfo.InvariantCulture);
            }

            if (kind == 3)
            {
                if (owner.Length != 0 || snapshot.Length != 0)
                    return "ERROR|Stage=ResetRarity Reason=InvalidTransport";
                return Mods.Remove(modId) ? "CLEARED" : "NOT_BOUND";
            }

            if (!Mods.TryGetValue(modId, out var mod) || mod.AssemblyPath != assemblyPath ||
                !mod.ByName.TryGetValue(owner, out BunnyRarity? rarity))
                return "ERROR|Stage=ResolveRarity Reason=OwnerNotBound";

            if (kind == 0)
            {
                string[] channels = snapshot.Split(',');
                if (channels.Length != 3 ||
                    !byte.TryParse(channels[0], NumberStyles.None, CultureInfo.InvariantCulture, out byte r) ||
                    !byte.TryParse(channels[1], NumberStyles.None, CultureInfo.InvariantCulture, out byte g) ||
                    !byte.TryParse(channels[2], NumberStyles.None, CultureInfo.InvariantCulture, out byte b))
                    return "ERROR|Stage=ReadRarityColor Reason=InvalidDiscoChannels";
                DiscoColor = new BunnyColor(r, g, b);
                BunnyColor color = rarity.RarityColor;
                return "COLOR|" + color.R.ToString(CultureInfo.InvariantCulture) + "|" +
                    color.G.ToString(CultureInfo.InvariantCulture) + "|" +
                    color.B.ToString(CultureInfo.InvariantCulture) + "|" +
                    color.A.ToString(CultureInfo.InvariantCulture);
            }

            string[] values = snapshot.Split(',');
            if (values.Length != 2 ||
                !float.TryParse(values[0], NumberStyles.Float, CultureInfo.InvariantCulture,
                    out float multiplier) || !float.IsFinite(multiplier) || multiplier < 0f ||
                !int.TryParse(values[1], NumberStyles.Integer, CultureInfo.InvariantCulture,
                    out int vanillaResult))
                return "ERROR|Stage=PrefixRarity Reason=InvalidPrefixData";

            int offset = BunnyRarityRegistry.PrefixOffset(multiplier);
            int next = rarity.GetPrefixedRarity(offset, multiplier);
            if (next > -11 && (next < -1 || next >= BunnyRarityRegistry.TerrariaVanillaRarityCount +
                BunnyRarityRegistry.MaxRegisteredRarities))
                return "ERROR|Stage=PrefixRarity Reason=ReturnedTypeOutOfBounds";
            return "RARITY|" + next.ToString(CultureInfo.InvariantCulture);
        }
        catch (Exception exception)
        {
            return "ERROR|Stage=RarityWorker Reason=" + exception.GetType().Name;
        }
    }
}
