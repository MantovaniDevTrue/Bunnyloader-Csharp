using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;

namespace BunnyLoader.Source;

public static class BunnyAchievementWorker
{
    private sealed class Binding
    {
        public string Path = "";
        public Assembly Assembly = null!;
        public string Revision = "";
    }

    private static readonly object Gate = new();
    private static readonly Dictionary<string, Binding> Bindings = new(StringComparer.Ordinal);
    private static readonly BunnyAchievementRegistry Registry = new();

    internal static void Evict(string modId, string assemblyPath)
    {
        lock (Gate)
        {
            if (!Bindings.TryGetValue(modId, out var binding) ||
                binding.Path != assemblyPath) return;
            Registry.Unload(modId, binding.Revision);
            Bindings.Remove(modId);
        }
    }

    public static string Dispatch(string modId, string assemblyPath, int kind, string snapshot)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 96 ||
            modId.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '.') ||
            string.IsNullOrEmpty(assemblyPath) || assemblyPath.Length > 4096 ||
            snapshot == null || snapshot.Length > 512 || kind is < 0 or > 4)
            return "ERROR|Stage=AchievementTransport Reason=InvalidArguments";
        try
        {
            lock (Gate)
            {
                if (kind == 4)
                {
                    if (snapshot.Length != 0) return "ERROR|Stage=AchievementUnload Reason=InvalidPayload";
                    if (!Bindings.TryGetValue(modId, out var active) ||
                        active.Path != assemblyPath)
                        return "NOT_BOUND";
                    Evict(modId, assemblyPath);
                    return "CLEARED";
                }

                if (kind == 0)
                {
                    if (snapshot.Length != 0)
                        return "ERROR|Stage=AchievementBind Reason=UnexpectedPayload";
                    Assembly assembly = BunnyRecipeLoader.LoadAssembly(modId, assemblyPath);
                    string revision = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(assemblyPath)));
                    int count = Registry.Register(modId, revision, assembly);
                    Bindings[modId] = new Binding { Path = assemblyPath, Revision = revision, Assembly = assembly };
                    return "READY|" + count.ToString(CultureInfo.InvariantCulture);
                }

                if (!Bindings.TryGetValue(modId, out var binding) ||
                    binding.Path != assemblyPath)
                    return "PENDING|Stage=AchievementInvoke Reason=RevisionNotBound";

                if (kind == 1)
                {
                    string[] values = snapshot.Split(',');
                    if (values.Length != 4 ||
                        !int.TryParse(values[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out int type) ||
                        !int.TryParse(values[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int id) ||
                        !int.TryParse(values[2], NumberStyles.None, CultureInfo.InvariantCulture, out int stack) ||
                        (values[3] != "0" && values[3] != "1") ||
                        !Enum.IsDefined(typeof(BunnyAchievementEventKind), type))
                        return "ERROR|Stage=AchievementEvent Reason=InvalidPayload";
                    int completed = Registry.Dispatch(modId,
                        new BunnyAchievementEvent((BunnyAchievementEventKind)type, id, stack, values[3] == "1"));
                    return "EVENT|" + completed.ToString(CultureInfo.InvariantCulture);
                }

                if (kind == 2)
                {
                    if (snapshot.Length != 0)
                        return "ERROR|Stage=AchievementSnapshot Reason=InvalidPayload";
                    string wire = Encode(Registry.Snapshot(modId));
                    return wire.Length > 12000
                        ? "ERROR|Stage=AchievementSnapshot Reason=SnapshotTooLarge"
                        : wire;
                }

                if (kind == 3)
                {
                    if (!Decode(snapshot, out var records))
                        return "ERROR|Stage=AchievementRestore Reason=InvalidPayload";
                    int restored = Registry.Restore(modId, records);
                    return "RESTORED|" + restored.ToString(CultureInfo.InvariantCulture);
                }
                return "ERROR|Stage=AchievementDispatch Reason=InvalidKind";
            }
        }
        catch (Exception error)
        {
            return "ERROR|Stage=AchievementWorker Reason=" + error.GetType().Name;
        }
    }

    private static string Encode(IReadOnlyList<BunnyAchievementSnapshot> records)
    {
        var result = new StringBuilder("ACH1");
        foreach (var record in records)
        {
            result.Append('\n').Append(Hex(record.Name));
            for (int i = 0; i < record.Keys.Length; i++)
            {
                result.Append('|').Append(Hex(record.Keys[i])).Append(',')
                    .Append((int)record.Kinds[i]).Append(',')
                    .Append(record.Values[i].ToString("R", CultureInfo.InvariantCulture));
            }
        }
        return result.ToString();
    }

    private static bool Decode(string wire, out IReadOnlyList<BunnyAchievementSnapshot> results)
    {
        results = Array.Empty<BunnyAchievementSnapshot>();
        if (wire.Length > 512 || !wire.StartsWith("ACH1", StringComparison.Ordinal) ||
            (wire.Length > 4 && wire[4] != '\n')) return false;
        var list = new List<BunnyAchievementSnapshot>();
        foreach (string row in wire.Split('\n').Skip(1))
        {
            if (string.IsNullOrEmpty(row) || list.Count >= 128) return false;
            string[] parts = row.Split('|');
            if (parts.Length < 2 || parts.Length > 33 ||
                !TryUnhex(parts[0], out string name)) return false;
            var keys = new string[parts.Length - 1];
            var kinds = new BunnyAchievementConditionKind[keys.Length];
            var values = new float[keys.Length];
            for (int i = 0; i < keys.Length; i++)
            {
                var tokens = parts[i + 1].Split(',');
                if (tokens.Length != 3 || !TryUnhex(tokens[0], out keys[i]) ||
                    !int.TryParse(tokens[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int kind) ||
                    !Enum.IsDefined(typeof(BunnyAchievementConditionKind), kind) ||
                    !float.TryParse(tokens[2], NumberStyles.Float, CultureInfo.InvariantCulture, out values[i]) ||
                    !float.IsFinite(values[i]) || values[i] < 0f)
                    return false;
                kinds[i] = (BunnyAchievementConditionKind)kind;
            }
            list.Add(new BunnyAchievementSnapshot(name, keys, kinds, values));
        }
        results = list;
        return true;
    }

    private static string Hex(string text) => Convert.ToHexString(Encoding.UTF8.GetBytes(text));

    private static bool TryUnhex(string encoded, out string value)
    {
        value = "";
        if (encoded.Length == 0 || encoded.Length > 512 || encoded.Length % 2 != 0)
            return false;
        try
        {
            value = new UTF8Encoding(false, true).GetString(Convert.FromHexString(encoded));
            return value.Length > 0 && !value.Any(c => c < 32);
        }
        catch (FormatException) { return false; }
        catch (DecoderFallbackException) { return false; }
    }
}
