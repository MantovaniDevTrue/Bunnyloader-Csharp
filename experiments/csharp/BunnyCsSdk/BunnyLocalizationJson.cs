using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace BunnyLoader.Source;

public sealed partial class BunnyLocalizationCatalog
{
    public int LoadFromContent(string modId, string contentDirectory)
    {
        if (contentDirectory == null) throw new ArgumentNullException(nameof(contentDirectory));
        ValidateModId(modId);
        string fullPath = Path.GetFullPath(contentDirectory);
        string directory = Path.Combine(fullPath, "Localization");
        if (!Directory.Exists(directory))
        {
            RemoveMod(modId);
            return 0;
        }

        if ((File.GetAttributes(directory) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException("Localization directory cannot be a symbolic link");

        var cultures = new Dictionary<string, IReadOnlyDictionary<string, string>>(StringComparer.Ordinal);
        string[] files = Directory.GetFiles(directory, "*.json", SearchOption.TopDirectoryOnly);
        Array.Sort(files, StringComparer.Ordinal);
        foreach (string path in files)
        {
            if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Localization file cannot be a symbolic link");
            string culture = Path.GetFileNameWithoutExtension(path);
            ValidateCulture(culture);
            if (cultures.Count >= 32)
                throw new InvalidDataException("Too many localization cultures");
            var info = new FileInfo(path);
            if (info.Length > 262144)
                throw new InvalidDataException("Localization file exceeds 256 KiB");
            byte[] bytes = File.ReadAllBytes(path);
            if (bytes.Length != info.Length)
                throw new InvalidDataException("Localization file changed during read");
            string json = new UTF8Encoding(false, true).GetString(bytes);
            if (json.Length != 0 && json[0] == '\uFEFF') json = json.Substring(1);
            cultures.Add(culture, BunnyLocalizationJson.Flatten(json));
        }

        if (cultures.Count == 0)
        {
            RemoveMod(modId);
            return 0;
        }

        ReplaceModCultures(modId, cultures);
        return cultures.Count;
    }
}

internal sealed class BunnyLocalizationJson
{
    private readonly string json;
    private readonly Dictionary<string, string> texts = new(StringComparer.Ordinal);
    private int offset;

    private BunnyLocalizationJson(string json) => this.json = json;

    public static IReadOnlyDictionary<string, string> Flatten(string json)
    {
        if (json == null) throw new ArgumentNullException(nameof(json));
        if (Encoding.UTF8.GetByteCount(json) > 262144)
            throw new InvalidDataException("Localization JSON exceeds 256 KiB");
        var parser = new BunnyLocalizationJson(json);
        parser.White();
        parser.Object("", 0, true);
        parser.White();
        if (parser.offset != json.Length)
            throw new InvalidDataException("Unexpected content after localization JSON");
        return parser.texts;
    }

    private void Object(string prefix, int depth, bool include)
    {
        if (depth >= 32) throw new InvalidDataException("Localization JSON nesting limit exceeded");
        Take('{');
        White();
        if (Next('}')) return;
        var seen = new HashSet<string>(StringComparer.Ordinal);
        while (true)
        {
            White();
            string name = String();
            if (!seen.Add(name)) throw new InvalidDataException("Duplicate JSON key");
            White();
            Take(':');
            White();
            string key = prefix + name;
            if (offset < json.Length && json[offset] == '"')
            {
                string value = String();
                if (include)
                {
                    if (!texts.TryAdd(key, value)) throw new InvalidDataException("Duplicate localization path");
                    if (texts.Count > 8192) throw new InvalidDataException("Localization entries limit exceeded");
                }
            }
            else if (offset < json.Length && json[offset] == '{')
            {
                Object(key + ".", depth + 1, include);
            }
            else
            {
                SkipValue(depth + 1);
            }
            White();
            if (Next('}')) return;
            Take(',');
        }
    }

    private void SkipValue(int depth)
    {
        if (depth >= 32) throw new InvalidDataException("Localization JSON nesting limit exceeded");
        White();
        if (offset == json.Length) throw new InvalidDataException("Unexpected end of JSON");
        switch (json[offset])
        {
            case '{':
                Object("", depth, false);
                return;
            case '[':
                offset++;
                White();
                if (Next(']')) return;
                while (true)
                {
                    SkipValue(depth + 1);
                    White();
                    if (Next(']')) return;
                    Take(',');
                }
            case '"':
                String();
                return;
            case 't':
                Literal("true");
                return;
            case 'f':
                Literal("false");
                return;
            case 'n':
                Literal("null");
                return;
            default:
                Number();
                return;
        }
    }

    private string String()
    {
        Take('"');
        var result = new StringBuilder();
        while (offset < json.Length)
        {
            char c = json[offset++];
            if (c == '"') return result.ToString();
            if (c == '\\')
            {
                if (offset == json.Length) throw new InvalidDataException("Incomplete JSON escape");
                c = json[offset++];
                switch (c)
                {
                    case '"': result.Append('"'); break;
                    case '\\': result.Append('\\'); break;
                    case '/': result.Append('/'); break;
                    case 'b': result.Append('\b'); break;
                    case 'f': result.Append('\f'); break;
                    case 'n': result.Append('\n'); break;
                    case 'r': result.Append('\r'); break;
                    case 't': result.Append('\t'); break;
                    case 'u':
                        char decoded = Hex4();
                        if (char.IsHighSurrogate(decoded))
                        {
                            if (offset + 2 > json.Length || json[offset] != '\\' || json[offset + 1] != 'u')
                                throw new InvalidDataException("Unpaired JSON high surrogate");
                            offset += 2;
                            char low = Hex4();
                            if (!char.IsLowSurrogate(low)) throw new InvalidDataException("Unpaired JSON high surrogate");
                            result.Append(decoded).Append(low);
                        }
                        else
                        {
                            if (char.IsLowSurrogate(decoded)) throw new InvalidDataException("Unpaired JSON low surrogate");
                            result.Append(decoded);
                        }
                        break;
                    default: throw new InvalidDataException("Unknown JSON escape");
                }
            }
            else
            {
                if (c < 32) throw new InvalidDataException("Unescaped JSON control character");
                if (char.IsHighSurrogate(c))
                {
                    if (offset >= json.Length || !char.IsLowSurrogate(json[offset]))
                        throw new InvalidDataException("Unpaired JSON high surrogate");
                    result.Append(c).Append(json[offset++]);
                }
                else
                {
                    if (char.IsLowSurrogate(c)) throw new InvalidDataException("Unpaired JSON low surrogate");
                    result.Append(c);
                }
            }
        }
        throw new InvalidDataException("Unterminated JSON string");
    }

    private char Hex4()
    {
        if (offset + 4 > json.Length) throw new InvalidDataException("Incomplete JSON Unicode escape");
        int codepoint = 0;
        for (int j = 0; j < 4; j++)
        {
            char c = json[offset++];
            int hex = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (hex < 0) throw new InvalidDataException("Invalid JSON Unicode escape");
            codepoint = codepoint * 16 + hex;
        }
        return (char)codepoint;
    }

    private void Number()
    {
        Next('-');
        if (Next('0'))
        {
            if (offset < json.Length && json[offset] >= '0' && json[offset] <= '9')
                throw new InvalidDataException("Leading zero in JSON number");
        }
        else
        {
            if (offset >= json.Length || json[offset] < '1' || json[offset] > '9')
                throw new InvalidDataException("Invalid JSON number");
            Digits(true);
        }
        if (Next('.')) Digits(true);
        if (Next('e') || Next('E'))
        {
            if (!Next('+')) Next('-');
            Digits(true);
        }
        if (offset < json.Length && json[offset] != ',' && json[offset] != '}' &&
            json[offset] != ']' && json[offset] != ' ' && json[offset] != '\r' &&
            json[offset] != '\n' && json[offset] != '\t')
            throw new InvalidDataException("Unexpected JSON number suffix");
    }

    private void Digits(bool firstRequired)
    {
        int first = offset;
        while (offset < json.Length && json[offset] >= '0' && json[offset] <= '9') offset++;
        if (firstRequired && first == offset) throw new InvalidDataException("Invalid JSON number");
    }

    private void Literal(string literal)
    {
        if (offset + literal.Length > json.Length ||
            string.CompareOrdinal(json, offset, literal, 0, literal.Length) != 0)
            throw new InvalidDataException("Invalid JSON token");
        offset += literal.Length;
    }

    private void White()
    {
        while (offset < json.Length && json[offset] is (' ' or '\n' or '\r' or '\t'))
            offset++;
    }

    private bool Next(char value)
    {
        if (offset >= json.Length || json[offset] != value) return false;
        offset++;
        return true;
    }

    private void Take(char value)
    {
        if (!Next(value)) throw new InvalidDataException("Unexpected JSON character");
    }
}
