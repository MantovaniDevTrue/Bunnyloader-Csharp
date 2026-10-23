using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Threading;

namespace BunnyLoader.Source;

public sealed partial class BunnyLocalizationCatalog
{
    private const int MaxEntries = 8192;
    private const int MaxTextBytes = 8192;
    private const int MaxDepth = 32;
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
        public readonly Dictionary<string, TextEntry> Overrides;
        public readonly string Culture;
        public readonly int Revision;

        public State(Dictionary<string, TextEntry> entries, string culture, int revision,
            Dictionary<string, TextEntry>? overrides = null)
        {
            Entries = entries;
            Overrides = overrides ?? new Dictionary<string, TextEntry>(StringComparer.Ordinal);
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

    internal int WireRowCount
    {
        get
        {
            State snapshot = Volatile.Read(ref current);
            int count = 0;
            foreach (TextEntry entry in snapshot.Entries.Values)
                count += entry.Cultures.Count;
            return count;
        }
    }

    public void SetCulture(string culture)
    {
        ValidateCulture(culture);
        lock (gate)
        {
            var snapshot = current;
            if (snapshot.Culture == culture) return;
            Volatile.Write(ref current, new State(snapshot.Entries, culture, checked(snapshot.Revision + 1), snapshot.Overrides));
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
            foreach (var pair in checkedTexts)
                if (snapshot.Overrides.TryGetValue(pair.Key, out TextEntry? registered))
                    foreach (var cultureEntry in registered.Cultures)
                        next[pair.Key] = next[pair.Key].With(cultureEntry.Key, cultureEntry.Value);
            if (next.Count > MaxEntries) throw new InvalidOperationException("Localization catalog exceeds its entry limit");
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1), snapshot.Overrides));
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
            var registered = new Dictionary<string, TextEntry>(snapshot.Overrides, StringComparer.Ordinal);
            if (registered.TryGetValue(fullKey, out TextEntry? overrideEntry))
                registered[fullKey] = overrideEntry.With(culture, text);
            else
                registered[fullKey] = new TextEntry(new Dictionary<string, string>(StringComparer.Ordinal)
                {
                    [culture] = text
                }, culture);
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1), registered));
        }
    }

    public void Register(string fullKey, IReadOnlyDictionary<string, string> cultures)
    {
        ValidateKey(fullKey, false);
        if (cultures == null) throw new ArgumentNullException(nameof(cultures));
        if (cultures.Count == 0 || cultures.Count > 32)
            throw new ArgumentOutOfRangeException(nameof(cultures));
        var checkedCultures = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var culture in cultures)
        {
            ValidateCulture(culture.Key);
            ValidateText(culture.Value);
            if (!checkedCultures.TryAdd(culture.Key, culture.Value))
                throw new ArgumentException("Duplicate localization culture", nameof(cultures));
        }
        lock (gate)
        {
            var snapshot = current;
            var next = new Dictionary<string, TextEntry>(snapshot.Entries, StringComparer.Ordinal);
            if (next.TryGetValue(fullKey, out TextEntry? entry))
            {
                foreach (var culture in checkedCultures)
                    entry = entry.With(culture.Key, culture.Value);
                next[fullKey] = entry;
            }
            else
            {
                string first = "";
                foreach (string name in checkedCultures.Keys) { first = name; break; }
                next[fullKey] = new TextEntry(checkedCultures, first);
            }
            if (next.Count > MaxEntries)
                throw new InvalidOperationException("Localization catalog exceeds its entry limit");
            var registered = new Dictionary<string, TextEntry>(snapshot.Overrides, StringComparer.Ordinal);
            if (registered.TryGetValue(fullKey, out TextEntry? overrideEntry))
            {
                foreach (var culture in checkedCultures)
                    overrideEntry = overrideEntry.With(culture.Key, culture.Value);
                registered[fullKey] = overrideEntry;
            }
            else
            {
                string firstCulture = "";
                foreach (string name in checkedCultures.Keys) { firstCulture = name; break; }
                registered[fullKey] = new TextEntry(new Dictionary<string, string>(checkedCultures, StringComparer.Ordinal), firstCulture);
            }
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1), registered));
        }
    }

    public string Key(string path, string callerModId)
    {
        ValidateModId(callerModId);
        if (path == null) throw new ArgumentNullException(nameof(path));
        string prefix = "Mods." + callerModId + ".";
        string relative = path.StartsWith(prefix, StringComparison.Ordinal)
            ? path.Substring(prefix.Length) : path;
        State snapshot = Volatile.Read(ref current);
        string full = prefix + relative;
        return snapshot.Entries.ContainsKey(full) ? full : path;
    }

    public void ReplaceModCultures(string modId, IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> cultures)
    {
        ValidateModId(modId);
        if (cultures == null) throw new ArgumentNullException(nameof(cultures));
        if (cultures.Count == 0 || cultures.Count > 32)
            throw new ArgumentOutOfRangeException(nameof(cultures));
        var validated = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        foreach (var culture in cultures)
        {
            ValidateCulture(culture.Key);
            if (culture.Value == null || culture.Value.Count > MaxEntries)
                throw new ArgumentException("Invalid localization culture content", nameof(cultures));
            var texts = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var text in culture.Value)
            {
                ValidateKey(text.Key, true);
                ValidateText(text.Value);
                if (!texts.TryAdd(text.Key, text.Value))
                    throw new ArgumentException("Duplicate localization key", nameof(cultures));
            }
            validated.Add(culture.Key, texts);
        }

        string prefix = "Mods." + modId + ".";
        lock (gate)
        {
            State snapshot = current;
            var next = new Dictionary<string, TextEntry>(snapshot.Entries, StringComparer.Ordinal);
            foreach (string key in snapshot.Entries.Keys)
                if (key.StartsWith(prefix, StringComparison.Ordinal)) next.Remove(key);
            foreach (var culture in validated)
            {
                foreach (var text in culture.Value)
                {
                    string key = prefix + text.Key;
                    if (next.TryGetValue(key, out TextEntry? previous))
                        next[key] = previous.With(culture.Key, text.Value);
                    else
                        next.Add(key, new TextEntry(new Dictionary<string, string>(StringComparer.Ordinal)
                        {
                            [culture.Key] = text.Value
                        }, culture.Key));
                    if (next.Count > MaxEntries)
                        throw new InvalidOperationException("Localization catalog exceeds its entry limit");
                }
            }
            foreach (var registered in snapshot.Overrides)
            {
                if (!registered.Key.StartsWith(prefix, StringComparison.Ordinal)) continue;
                if (next.TryGetValue(registered.Key, out TextEntry? existing))
                {
                    foreach (var culture in registered.Value.Cultures)
                        existing = existing.With(culture.Key, culture.Value);
                    next[registered.Key] = existing;
                }
                else next[registered.Key] = registered.Value;
            }
            if (next.Count > MaxEntries)
                throw new InvalidOperationException("Localization catalog exceeds its entry limit");
            Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1), snapshot.Overrides));
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
            var registered = new Dictionary<string, TextEntry>(snapshot.Overrides, StringComparer.Ordinal);
            int removed = 0;
            foreach (string key in snapshot.Entries.Keys)
            {
                if (!key.StartsWith(prefix, StringComparison.Ordinal)) continue;
                if (next.Remove(key)) removed++;
                registered.Remove(key);
            }
            if (removed != 0)
                Volatile.Write(ref current, new State(next, snapshot.Culture, checked(snapshot.Revision + 1), registered));
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

    public IReadOnlyDictionary<string, string>? Texts(string path, string? callerModId = null)
    {
        State snapshot = Volatile.Read(ref current);
        if (!TryResolveKey(snapshot, path, callerModId, out string key))
            return null;
        TextEntry entry = snapshot.Entries[key];
        var result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (string culture in entry.Cultures.Keys)
        {
            var cultureSnapshot = new State(snapshot.Entries, culture, snapshot.Revision, snapshot.Overrides);
            result[culture] = Expand(cultureSnapshot, key,
                new HashSet<string>(StringComparer.Ordinal), 0);
        }
        return result;
    }

    public IReadOnlyList<string> ExportWireRows()
    {
        State snapshot = Volatile.Read(ref current);
        var rows = new List<string>(snapshot.Entries.Count);
        int wireBytes = 0;
        foreach (string key in new SortedSet<string>(snapshot.Entries.Keys, StringComparer.Ordinal))
        {
            TextEntry entry = snapshot.Entries[key];
            foreach (string culture in new SortedSet<string>(entry.Cultures.Keys, StringComparer.Ordinal))
            {
                string path = Convert.ToHexString(Encoding.UTF8.GetBytes(key));
                string value = Convert.ToHexString(Encoding.UTF8.GetBytes(entry.Cultures[culture]));
                wireBytes = checked(wireBytes + 10 + culture.Length + path.Length + value.Length);
                if (wireBytes > 4 * 1024 * 1024 || rows.Count >= 16384)
                    throw new InvalidOperationException("Localization wire capacity exceeded");
                rows.Add("LOCALE\t" + culture + "\t" + path + "\t" + value);
            }
        }
        return rows;
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

    private static bool ReferenceChar(char c)
        => char.IsAsciiLetterOrDigit(c) || c == '_' || c == '.' || c == '/' || c == '-';

    private static bool ReadReference(string value, int start, out int end, out string key, out int offset)
    {
        end = start;
        key = "";
        offset = 0;
        if (start + 3 > value.Length || value[start] != '{' || value[start + 1] != '$') return false;
        int cursor = start + 2;
        while (cursor < value.Length && ReferenceChar(value[cursor])) cursor++;
        if (cursor == start + 2) return false;
        key = value.Substring(start + 2, cursor - start - 2);
        if (cursor < value.Length && value[cursor] == '@')
        {
            int first = ++cursor;
            while (cursor < value.Length && char.IsAsciiDigit(value[cursor])) cursor++;
            if (cursor == first) return false;
            if (!int.TryParse(value.Substring(first, cursor - first), NumberStyles.None,
                CultureInfo.InvariantCulture, out offset)) offset = -1;
        }
        if (cursor >= value.Length || value[cursor] != '}') return false;
        end = cursor + 1;
        return true;
    }

    private static string Expand(State snapshot, string key, HashSet<string> visiting, int depth)
    {
        if (!snapshot.Entries.TryGetValue(key, out TextEntry? entry)) return key;
        if (depth >= MaxDepth || !visiting.Add(key)) return key;
        string value = entry.Pick(snapshot.Culture);
        int marker = value.IndexOf("{$", StringComparison.Ordinal);
        if (marker >= 0)
        {
            var output = new StringBuilder(value.Length);
            int position = 0;
            while (marker >= 0)
            {
                output.Append(value, position, marker - position);
                if (ReadReference(value, marker, out int end, out string reference, out int shift))
                {
                    string target = ResolveReferenceKey(snapshot, key, reference);
                    string resolved = Expand(snapshot, target, visiting, depth + 1);
                    if (shift > 0) resolved = ShiftArguments(resolved, Math.Min(shift, 10000));
                    output.Append(resolved);
                    position = end;
                }
                else
                {
                    output.Append(value, marker, 2);
                    position = marker + 2;
                }
                marker = value.IndexOf("{$", position, StringComparison.Ordinal);
            }
            output.Append(value, position, value.Length - position);
            value = output.ToString();
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
        var output = new StringBuilder(value.Length);
        int cursor = 0;
        while (cursor < value.Length)
        {
            int open = value.IndexOf('{', cursor);
            if (open < 0)
            {
                output.Append(value, cursor, value.Length - cursor);
                break;
            }
            output.Append(value, cursor, open - cursor);
            int digit = open + 1;
            bool caret = digit < value.Length && value[digit] == '^';
            if (caret) digit++;
            int last = digit;
            while (last < value.Length && char.IsAsciiDigit(value[last])) last++;
            int close = last;
            if (last < value.Length && value[last] == ':')
            {
                close = last + 1;
                while (close < value.Length && value[close] != '{' && value[close] != '}' &&
                    value[close] != '\n' && value[close] != '\r') close++;
            }
            if (last > digit && close < value.Length && value[close] == '}' &&
                int.TryParse(value.Substring(digit, last - digit), NumberStyles.None,
                    CultureInfo.InvariantCulture, out int number) && number <= int.MaxValue - offset)
            {
                output.Append('{');
                if (caret) output.Append('^');
                output.Append((number + offset).ToString(CultureInfo.InvariantCulture));
                output.Append(value, last, close + 1 - last);
                cursor = close + 1;
            }
            else
            {
                output.Append('{');
                cursor = open + 1;
            }
        }
        return output.ToString();
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
            if (!char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' &&
                c != '.' && c != '/' && c != '$')
                throw new ArgumentException("Invalid localization key", nameof(key));
        if (key.Contains("..", StringComparison.Ordinal) || (!relative && !key.Contains('.')))
            throw new ArgumentException("Invalid localization key", nameof(key));
        int variant = key.IndexOf('$');
        if (variant >= 0 &&
            (variant == 0 || variant == key.Length - 1 ||
             key[variant - 1] == '.' || key[variant - 1] == '/' ||
             key.IndexOf('$', variant + 1) >= 0 ||
             key.IndexOfAny(new[] { '.', '/' }, variant + 1) >= 0))
            throw new ArgumentException("Invalid localization variant", nameof(key));
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
