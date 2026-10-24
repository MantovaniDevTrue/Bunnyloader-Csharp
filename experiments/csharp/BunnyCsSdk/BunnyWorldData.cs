using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace BunnyLoader.Source;

public sealed class BunnyWorldData
{
    private const int MaxEntries = 64;
    private const int MaxPayload = 16384;
    private readonly SortedDictionary<string, string> values = new(StringComparer.Ordinal);

    public int Count => values.Count;

    public bool ContainsKey(string key) => values.ContainsKey(ValidateKey(key));

    public void SetBool(string key, bool value) => Set(key, value ? "b1" : "b0");

    public bool GetBool(string key, bool defaultValue = false)
    {
        return values.TryGetValue(ValidateKey(key), out string? value)
            ? value == "b1" ? true : value == "b0" ? false : defaultValue
            : defaultValue;
    }

    public void SetInt(string key, int value) =>
        Set(key, "i" + value.ToString(CultureInfo.InvariantCulture));

    public int GetInt(string key, int defaultValue = 0)
    {
        return values.TryGetValue(ValidateKey(key), out string? value) &&
            value.StartsWith('i') &&
            int.TryParse(value.AsSpan(1), NumberStyles.AllowLeadingSign,
                CultureInfo.InvariantCulture, out int result) ? result : defaultValue;
    }

    public void SetString(string key, string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (Encoding.UTF8.GetByteCount(value) > 2048)
            throw new ArgumentOutOfRangeException(nameof(value));
        Set(key, "s" + Convert.ToHexString(Encoding.UTF8.GetBytes(value)));
    }

    public string GetString(string key, string defaultValue = "")
    {
        if (!values.TryGetValue(ValidateKey(key), out string? value) ||
            !value.StartsWith('s')) return defaultValue;
        try
        {
            return new UTF8Encoding(false, true).GetString(
                Convert.FromHexString(value.AsSpan(1)));
        }
        catch (FormatException) { return defaultValue; }
        catch (DecoderFallbackException) { return defaultValue; }
    }

    private static string ValidateKey(string key)
    {
        if (string.IsNullOrEmpty(key) || key.Length > 64)
            throw new ArgumentException("Invalid world data key", nameof(key));
        foreach (char c in key)
            if (!char.IsAsciiLetterOrDigit(c) || c > 127)
                if (c != '_' && c != '-')
                    throw new ArgumentException("Invalid world data key", nameof(key));
        return key;
    }

    private void Set(string key, string value)
    {
        key = ValidateKey(key);
        if (values.Count >= MaxEntries && !values.ContainsKey(key))
            throw new InvalidOperationException("World data capacity exceeded");
        values[key] = value;
    }

    internal string Encode()
    {
        var result = new StringBuilder("bunny-world-data 1\n");
        foreach (var entry in values)
            result.Append(entry.Key).Append('\t').Append(entry.Value).Append('\n');
        if (Encoding.UTF8.GetByteCount(result.ToString()) > MaxPayload)
            throw new InvalidOperationException("World data payload exceeded");
        return result.ToString();
    }

    internal static bool TryDecode(string serialized, out BunnyWorldData data)
    {
        data = new BunnyWorldData();
        if (serialized.Length > MaxPayload ||
            !serialized.StartsWith("bunny-world-data 1\n", StringComparison.Ordinal))
            return false;
        string[] lines = serialized.Split('\n');
        if (lines.Length < 2 || lines[^1].Length != 0) return false;
        for (int i = 1; i < lines.Length - 1; i++)
        {
            string line = lines[i];
            int split = line.IndexOf('\t');
            if (split <= 0 || line.IndexOf('\t', split + 1) >= 0) return false;
            string key = line[..split], value = line[(split + 1)..];
            try { ValidateKey(key); }
            catch (ArgumentException) { return false; }
            if (data.values.Count >= MaxEntries || data.values.ContainsKey(key) ||
                value.Length < 2) return false;
            if (value[0] == 'b')
            {
                if (value != "b1" && value != "b0") return false;
            }
            else if (value[0] == 'i')
            {
                if (!int.TryParse(value.AsSpan(1), NumberStyles.AllowLeadingSign,
                    CultureInfo.InvariantCulture, out _)) return false;
            }
            else if (value[0] == 's')
            {
                if (value.Length > 4097 || value.Length % 2 != 1) return false;
                try { _ = new UTF8Encoding(false, true).GetString(
                    Convert.FromHexString(value.AsSpan(1))); }
                catch (FormatException) { return false; }
                catch (DecoderFallbackException) { return false; }
            }
            else return false;
            data.values.Add(key, value);
        }
        return true;
    }
}
