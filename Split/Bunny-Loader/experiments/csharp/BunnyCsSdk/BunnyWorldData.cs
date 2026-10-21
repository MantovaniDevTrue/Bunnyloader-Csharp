using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Estado persistente de um sistema de mundo. A ponte nativa chamará estes
/// métodos quando o mundo for limpo, carregado, salvo ou descarregado.
/// Esta classe não instala hooks de Terraria por conta própria.
/// </summary>
public abstract class BunnyWorldSystem
{
    public virtual void ClearWorld() { }
    public virtual void OnWorldLoad() { }
    public virtual void LoadWorldData(BunnyWorldTag tag) { }
    public virtual void PostWorldLoad() { }
    public virtual void SaveWorldData(BunnyWorldTag tag) { }
    public virtual void OnWorldUnload() { }
}

/// <summary>
/// Dicionário tipado sem dependência de JavaScript ou do TagCompound do jogo.
/// Campos ausentes retornam valores padrão, inclusive em mundos novos.
/// </summary>
public sealed class BunnyWorldTag
{
    private readonly SortedDictionary<string, BunnyWorldField> _fields =
        new(StringComparer.Ordinal);

    public int Count => _fields.Count;
    public bool ContainsKey(string key) => _fields.ContainsKey(ValidateKey(key));

    public void SetBoolean(string key, bool value) =>
        SetField(key, new BunnyWorldField(BunnyWorldFieldKind.Boolean, value));
    public void SetInt32(string key, int value) =>
        SetField(key, new BunnyWorldField(BunnyWorldFieldKind.Int32, value));
    public void SetString(string key, string value)
    {
        if (value == null) throw new ArgumentNullException(nameof(value));
        if (Encoding.UTF8.GetByteCount(value) > BunnyWorldDocument.MaxTextBytes)
            throw new ArgumentOutOfRangeException(nameof(value));
        SetField(key, new BunnyWorldField(BunnyWorldFieldKind.String, value));
    }

    public bool GetBoolean(string key) =>
        Get(key, BunnyWorldFieldKind.Boolean) is bool value && value;
    public int GetInt32(string key) =>
        Get(key, BunnyWorldFieldKind.Int32) is int value ? value : 0;
    public string GetString(string key) =>
        Get(key, BunnyWorldFieldKind.String) as string ?? "";

    public bool Remove(string key) => _fields.Remove(ValidateKey(key));
    public void Clear() => _fields.Clear();

    private object? Get(string key, BunnyWorldFieldKind kind) =>
        _fields.TryGetValue(ValidateKey(key), out var field) && field.Kind == kind
            ? field.Value : null;

    private void SetField(string key, BunnyWorldField field)
    {
        key = ValidateKey(key);
        if (!_fields.ContainsKey(key) && _fields.Count >= BunnyWorldDocument.MaxFieldsPerSystem)
            throw new InvalidOperationException("World data field limit exceeded");
        _fields[key] = field;
    }

    internal static string ValidateKey(string key)
    {
        if (string.IsNullOrWhiteSpace(key) ||
            Encoding.UTF8.GetByteCount(key) > BunnyWorldDocument.MaxKeyBytes ||
            key.IndexOfAny(new[] { '\0', '\r', '\n', '/', '\\' }) >= 0)
            throw new ArgumentException("Invalid world data key", nameof(key));
        return key;
    }

    internal BunnyWorldTag Copy()
    {
        var copy = new BunnyWorldTag();
        foreach (var field in _fields)
            copy._fields.Add(field.Key, field.Value);
        return copy;
    }

    internal IEnumerable<KeyValuePair<string, BunnyWorldField>> Fields => _fields;
}

internal enum BunnyWorldFieldKind : byte
{
    Boolean = 1,
    Int32 = 2,
    String = 3
}

internal readonly struct BunnyWorldField
{
    internal readonly BunnyWorldFieldKind Kind;
    internal readonly object Value;

    internal BunnyWorldField(BunnyWorldFieldKind kind, object value)
    {
        Kind = kind;
        Value = value;
    }
}

/// <summary>
/// Documento de dados para todos os sistemas C# de um mesmo mundo.
/// O formato BWC1 é determinístico, limitado e possui CRC32. A gravação
/// atômica do arquivo físico pertence ao host nativo, não a este codec.
/// </summary>
public sealed class BunnyWorldDocument
{
    public const int MaxDocumentBytes = 1024 * 1024;
    public const int MaxSystems = 512;
    public const int MaxFieldsPerSystem = 128;
    public const int MaxKeyBytes = 160;
    public const int MaxTextBytes = 4096;

    private static readonly UTF8Encoding Utf8 = new(false, true);
    private static readonly byte[] Magic = { (byte)'B', (byte)'W', (byte)'C', (byte)'1' };
    private readonly SortedDictionary<string, BunnyWorldTag> _systems =
        new(StringComparer.Ordinal);

    public int SystemCount => _systems.Count;

    /// <summary>
    /// Identidade estável: UUID do manifest + nome completo da classe.
    /// Nunca usar o índice de registro, que muda ao instalar outros mods.
    /// </summary>
    public static string MakeKey(string modUid, string className)
    {
        if (!Guid.TryParse(modUid, out var uid))
            throw new ArgumentException("Invalid mod UUID", nameof(modUid));
        if (string.IsNullOrWhiteSpace(className) ||
            className.IndexOfAny(new[] { '\0', '/', '\\', '\r', '\n' }) >= 0)
            throw new ArgumentException("Invalid world system class", nameof(className));
        var key = uid.ToString("D") + "/" + className;
        if (Utf8.GetByteCount(key) > MaxKeyBytes)
            throw new ArgumentOutOfRangeException(nameof(className));
        return key;
    }

    /// <summary>
    /// Cópia defensiva: o mod não consegue modificar um snapshot já salvo.
    /// Mod ausente permanece no documento quando outro sistema é atualizado.
    /// </summary>
    public BunnyWorldTag Read(string modUid, string className)
    {
        var key = MakeKey(modUid, className);
        return _systems.TryGetValue(key, out var tag) ? tag.Copy() : new BunnyWorldTag();
    }

    public void Replace(string modUid, string className, BunnyWorldTag tag)
    {
        if (tag == null) throw new ArgumentNullException(nameof(tag));
        var key = MakeKey(modUid, className);
        if (tag.Count == 0)
        {
            _systems.Remove(key);
            return;
        }
        if (!_systems.ContainsKey(key) && _systems.Count >= MaxSystems)
            throw new InvalidOperationException("World system limit exceeded");
        _systems[key] = tag.Copy();
    }

    /// <summary>Usar somente com dados de um mundo novo ou após unload.</summary>
    public void Clear() => _systems.Clear();

    public byte[] Encode()
    {
        using var buffer = new MemoryStream();
        using (var writer = new BinaryWriter(buffer, Utf8, true))
        {
            writer.Write(Magic);
            writer.Write(checked((ushort)_systems.Count));
            foreach (var system in _systems)
            {
                WriteText(writer, system.Key, MaxKeyBytes);
                writer.Write(checked((ushort)system.Value.Count));
                foreach (var entry in system.Value.Fields)
                {
                    WriteText(writer, entry.Key, MaxKeyBytes);
                    writer.Write((byte)entry.Value.Kind);
                    switch (entry.Value.Kind)
                    {
                        case BunnyWorldFieldKind.Boolean:
                            writer.Write((byte)((bool)entry.Value.Value ? 1 : 0));
                            break;
                        case BunnyWorldFieldKind.Int32:
                            writer.Write((int)entry.Value.Value);
                            break;
                        case BunnyWorldFieldKind.String:
                            WriteText(writer, (string)entry.Value.Value, MaxTextBytes);
                            break;
                        default:
                            throw new InvalidDataException("Unsupported world field kind");
                    }
                }
            }
        }

        var payload = buffer.ToArray();
        if (payload.Length + sizeof(uint) > MaxDocumentBytes)
            throw new InvalidOperationException("World data document limit exceeded");
        var result = new byte[payload.Length + sizeof(uint)];
        Buffer.BlockCopy(payload, 0, result, 0, payload.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(payload.Length), Crc32(payload));
        return result;
    }

    /// <summary>
    /// Falha fechada: documento truncado, CRC inválido, tipo desconhecido,
    /// chave duplicada e bytes extras nunca substituem estado válido.
    /// O chamador deve manter o arquivo antigo se a leitura falhar.
    /// </summary>
    public static bool TryDecode(ReadOnlySpan<byte> bytes, out BunnyWorldDocument document)
    {
        document = new BunnyWorldDocument();
        if (bytes.Length < Magic.Length + sizeof(ushort) + sizeof(uint) ||
            bytes.Length > MaxDocumentBytes)
            return false;

        var content = bytes.Slice(0, bytes.Length - sizeof(uint));
        if (BinaryPrimitives.ReadUInt32LittleEndian(bytes.Slice(content.Length)) != Crc32(content))
            return false;

        try
        {
            using var stream = new MemoryStream(content.ToArray(), false);
            using var reader = new BinaryReader(stream, Utf8, false);
            for (var i = 0; i < Magic.Length; i++)
                if (reader.ReadByte() != Magic[i]) return false;

            var count = reader.ReadUInt16();
            if (count > MaxSystems) return false;
            var parsed = new BunnyWorldDocument();
            for (var i = 0; i < count; i++)
            {
                var key = ReadText(reader, MaxKeyBytes);
                var slash = key.IndexOf('/');
                if (slash < 0 || slash != key.LastIndexOf('/') ||
                    MakeKey(key.Substring(0, slash), key.Substring(slash + 1)) != key ||
                    parsed._systems.ContainsKey(key))
                    return false;

                var fieldCount = reader.ReadUInt16();
                if (fieldCount > MaxFieldsPerSystem) return false;
                var tag = new BunnyWorldTag();
                for (var j = 0; j < fieldCount; j++)
                {
                    var fieldKey = BunnyWorldTag.ValidateKey(ReadText(reader, MaxKeyBytes));
                    if (tag.ContainsKey(fieldKey)) return false;
                    switch ((BunnyWorldFieldKind)reader.ReadByte())
                    {
                        case BunnyWorldFieldKind.Boolean:
                            var boolean = reader.ReadByte();
                            if (boolean > 1) return false;
                            tag.SetBoolean(fieldKey, boolean == 1);
                            break;
                        case BunnyWorldFieldKind.Int32:
                            tag.SetInt32(fieldKey, reader.ReadInt32());
                            break;
                        case BunnyWorldFieldKind.String:
                            tag.SetString(fieldKey, ReadText(reader, MaxTextBytes));
                            break;
                        default:
                            return false;
                    }
                }
                if (tag.Count == 0) return false;
                parsed._systems.Add(key, tag);
            }
            if (stream.Position != stream.Length) return false;
            document = parsed;
            return true;
        }
        catch (Exception error) when (error is EndOfStreamException ||
                                      error is InvalidDataException ||
                                      error is ArgumentException ||
                                      error is DecoderFallbackException)
        {
            return false;
        }
    }

    private static void WriteText(BinaryWriter writer, string value, int limit)
    {
        var text = Utf8.GetBytes(value);
        if (text.Length > limit) throw new InvalidDataException("World string too large");
        writer.Write(checked((ushort)text.Length));
        writer.Write(text);
    }

    private static string ReadText(BinaryReader reader, int limit)
    {
        var length = reader.ReadUInt16();
        if (length > limit) throw new InvalidDataException("World string too large");
        var bytes = reader.ReadBytes(length);
        if (bytes.Length != length) throw new EndOfStreamException();
        return Utf8.GetString(bytes);
    }

    private static uint Crc32(ReadOnlySpan<byte> data)
    {
        uint crc = uint.MaxValue;
        foreach (var value in data)
        {
            crc ^= value;
            for (var i = 0; i < 8; i++)
                crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320u : 0u);
        }
        return ~crc;
    }
}
