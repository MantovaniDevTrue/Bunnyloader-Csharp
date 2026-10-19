using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace BunnyLoader.Source;

public sealed class BunnyLocalizationCatalog
{
    private const int MaxEntries = 8192;
    private const int MaxTextBytes = 8192;
    private const int MaxDepth = 32;
    private static readonly Regex Reference = new(@"\{\$([\w./-]+)(?:@(\d+))?\}", RegexOptions.CultureInvariant);
    private static readonly Regex FormatArgument = new(@"\{(\^?)(\d+)(:[^{}\r\n]*)?\}", RegexOptions.CultureInvariant);
    private readonly object gate = new();
    private State current;

    private sealed class TextEntry
    {
        public readonly Dictionary<string, string> Cultures;
        public readonly string FirstCulture;

        public TextEntry(Dictionary<string, string> cultures, string firstCulture)
        {
            Cultures = cultures;
            FirstCulture = firstCulture;
        }

        public string Pick(string culture)
        {
            if (Cultures.TryGetValue(culture, out string? value)) return value;
            if (Cultures.TryGetValue("en-US", out value)) return value;
            return Cultures[FirstCulture];
        }

        public TextEntry With(string culture, string value)
        {
            var copy = new Dictionary<string, string>(Cultures, StringComparer.Ordinal);
            copy[culture] = value;
            return new TextEntry(copy, FirstCulture);
        }
    }

    private sealed class State
    {
        public readonly Dictionary<string, TextEntry> Entries;
        public readonly string Culture;
        public readonly int Revision;

        public State(Dictionary<string, TextEntry> entries, string culture, int revision)
        {
            Entries = entries;
            Culture = culture;
            Revision = revision;
        }
    }

    public BunnyLocalizationCatalog(string culture = "en-US")
    {
        ValidateCulture(culture);
        current = new State(new Dictionary<string, TextEntry>(StringComparer.Ordinal), culture, 0);
    }

    public string ActiveCultureName => Volatile.Read(ref current).Culture;
    public int Revision => Volatile.Read(ref current).Revision;
    public int Count => Volatile.Read(ref current).Entries.Count;

    public void SetCulture(string culture)
    {
        ValidateCulture(culture);
        lock (gate)
        {
            var snapshot = current;
            if (snapshot.Culture == culture) return;
            Volatile.Write(ref current, new State(snapshot.Entries, culture, checked(snapshot.Revision + 1)));
        }
    }

    public void RegisterCulture(string modId, string culture, IReadOnlyDictionary<string, string> texts)
    {
        ValidateModId(modId);
        ValidateCulture(culture);
        if (texts == null) throw new ArgumentNullException(nameof(texts));
        if (texts.Count > MaxEntries) throw new ArgumentOutOfRangeException(nameof(texts));
        var checkedTexts = new List<KeyValuePair<string, string>>(texts.Count);
        foreach (var pair in texts)
        {
            ValidateKey(pair.Key, true);
            ValidateText(pair.Value);
            checkedTexts.Add(new KeyValuePair<string, string>("Mods." + modId + "." + pair.Key, pair.Value));
        }
        lock (gate)
        {
            var snapshot = current;
            var next = new Dictionary<string, TextEntry>(snapshot.Entries, StringComparer.Ordinal);
            foreach (var pair in checkedTexts)
            {
                if (next.TryGetValue(pair.Key, out TextEntry? existing))
                    next[pair.Key] = existing.With(culture, pair.Value);
                else
                    next[pair.Key] = new TextEntry(new Dictionary<string, string>(StringComparer.Ordinal)
                    {
                        [culture] = pair.Value
                    }, culture);
            }
            if (next.Count > MaxEntries) throw new InvalidOperationException("Localization catalog exceeds its entry limit");
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1)));
        }
    }

    public void Register(string fullKey, string culture, string text)
    {
        ValidateKey(fullKey, false);
        ValidateCulture(culture);
        ValidateText(text);
        lock (gate)
        {
            var snapshot = current;
            var next = new Dictionary<string, TextEntry>(snapshot.Entries, StringComparer.Ordinal);
            if (next.TryGetValue(fullKey, out TextEntry? existing))
                next[fullKey] = existing.With(culture, text);
            else
                next[fullKey] = new TextEntry(new Dictionary<string, string>(StringComparer.Ordinal)
                {
                    [culture] = text
                }, culture);
            if (next.Count > MaxEntries) throw new InvalidOperationException("Localization catalog exceeds its entry limit");
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1)));
        }
    }

    public void RemoveMod(string modId)
    {
        ValidateModId(modId);
        string prefix = "Mods." + modId + ".";
        lock (gate)
        {
            var snapshot = current;
            var next = new Dictionary<string, TextEntry>(snapshot.Entries, StringComparer.Ordinal);
            int removed = 0;
            foreach (string key in snapshot.Entries.Keys)
            {
                if (!key.StartsWith(prefix, StringComparison.Ordinal)) continue;
                if (next.Remove(key)) removed++;
            }
            if (removed != 0)
                Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1)));
        }
    }

    public bool Exists(string path, string? callerModId = null)
    {
        State snapshot = Volatile.Read(ref current);
        return TryResolveKey(snapshot, path, callerModId, out _);
    }

    public string Translate(string path, string? callerModId = null)
    {
        string text = TryTranslate(path, callerModId);
        return text.Length == 0 && !Exists(path, callerModId) ? path : text;
    }

    public string TryTranslate(string path, string? callerModId = null)
    {
        State snapshot = Volatile.Read(ref current);
        return TryResolveKey(snapshot, path, callerModId, out string key)
            ? Expand(snapshot, key, new HashSet<string>(StringComparer.Ordinal), 0)
            : string.Empty;
    }

    public IReadOnlyDictionary<string, string> SnapshotForGame()
    {
        State snapshot = Volatile.Read(ref current);
        var expanded = new Dictionary<string, string>(snapshot.Entries.Count, StringComparer.Ordinal);
        foreach (string key in snapshot.Entries.Keys)
            expanded[key] = Expand(snapshot, key, new HashSet<string>(StringComparer.Ordinal), 0);
        return expanded;
    }

    private static bool TryResolveKey(State snapshot, string path, string? callerModId, out string key)
    {
        if (path == null) throw new ArgumentNullException(nameof(path));
        if (!string.IsNullOrEmpty(callerModId))
        {
            ValidateModId(callerModId);
            string own = "Mods." + callerModId + ".";
            string relative = path.StartsWith(own, StringComparison.Ordinal) ? path : own + path;
            if (snapshot.Entries.ContainsKey(relative))
            {
                key = relative;
                return true;
            }
        }
        if (snapshot.Entries.ContainsKey(path))
        {
            key = path;
            return true;
        }
        if (callerModId != null)
        {
            key = string.Empty;
            return false;
        }
        string? only = null;
        foreach (string candidate in snapshot.Entries.Keys)
        {
            if (!candidate.StartsWith("Mods.", StringComparison.Ordinal) ||
                !candidate.EndsWith("." + path, StringComparison.Ordinal)) continue;
            string remainder = candidate.Substring(5);
            int dot = remainder.IndexOf('.');
            if (dot < 1 || !string.Equals(remainder.Substring(dot + 1), path, StringComparison.Ordinal)) continue;
            if (only != null)
            {
                key = string.Empty;
                return false;
            }
            only = candidate;
        }
        key = only ?? string.Empty;
        return only != null;
    }

    private static string Expand(State snapshot, string key, HashSet<string> visiting, int depth)
    {
        if (!snapshot.Entries.TryGetValue(key, out TextEntry? entry)) return key;
        if (depth >= MaxDepth || !visiting.Add(key)) return key;
        string value = entry.Pick(snapshot.Culture);
        if (value.IndexOf("{$", StringComparison.Ordinal) >= 0)
        {
            value = Reference.Replace(value, match =>
            {
                string reference = match.Groups[1].Value;
                string target = ResolveReferenceKey(snapshot, key, reference);
                string resolved = Expand(snapshot, target, visiting, depth + 1);
                if (match.Groups[2].Success &&
                    int.TryParse(match.Groups[2].Value, NumberStyles.None, CultureInfo.InvariantCulture, out int offset))
                    resolved = ShiftArguments(resolved, Math.Min(offset, 10000));
                return resolved;
            });
        }
        visiting.Remove(key);
        return value;
    }

    private static string ResolveReferenceKey(State snapshot, string scope, string reference)
    {
        if (snapshot.Entries.ContainsKey(reference)) return reference;
        int end = scope.Length;
        while ((end = scope.LastIndexOf('.', end - 1)) >= 0)
        {
            string candidate = scope.Substring(0, end + 1) + reference;
            if (snapshot.Entries.ContainsKey(candidate)) return candidate;
        }
        return reference;
    }

    private static string ShiftArguments(string value, int offset)
    {
        return FormatArgument.Replace(value, match =>
        {
            if (!int.TryParse(match.Groups[2].Value, NumberStyles.None, CultureInfo.InvariantCulture, out int n) ||
                n > int.MaxValue - offset) return match.Value;
            return "{" + match.Groups[1].Value + (n + offset).ToString(CultureInfo.InvariantCulture) + match.Groups[3].Value + "}";
        });
    }

    private static void ValidateModId(string modId)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 72) throw new ArgumentException("Invalid mod ID", nameof(modId));
        foreach (char c in modId)
            if (!char.IsAsciiLetterOrDigit(c) && c != '-' && c != '_')
                throw new ArgumentException("Invalid mod ID", nameof(modId));
    }

    private static void ValidateCulture(string culture)
    {
        if (string.IsNullOrWhiteSpace(culture) || culture.Length < 2 || culture.Length > 20)
            throw new ArgumentException("Invalid culture", nameof(culture));
        foreach (char c in culture)
            if (!char.IsAsciiLetterOrDigit(c) && c != '-') throw new ArgumentException("Invalid culture", nameof(culture));
    }

    private static void ValidateKey(string key, bool relative)
    {
        if (string.IsNullOrWhiteSpace(key) || key.Length > 256 || key[0] == '.' || key[key.Length - 1] == '.')
            throw new ArgumentException("Invalid localization key", nameof(key));
        foreach (char c in key)
            if (!char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '.' && c != '/')
                throw new ArgumentException("Invalid localization key", nameof(key));
        if (key.Contains("..", StringComparison.Ordinal) || (!relative && !key.Contains('.')))
            throw new ArgumentException("Invalid localization key", nameof(key));
    }

    private static void ValidateText(string text)
    {
        if (text == null) throw new ArgumentNullException(nameof(text));
        if (Encoding.UTF8.GetByteCount(text) > MaxTextBytes)
            throw new ArgumentOutOfRangeException(nameof(text));
        foreach (char c in text)
            if (c == '\0' || (c < ' ' && c != '\n' && c != '\r' && c != '\t'))
                throw new ArgumentException("Invalid localization text", nameof(text));
    }
}
