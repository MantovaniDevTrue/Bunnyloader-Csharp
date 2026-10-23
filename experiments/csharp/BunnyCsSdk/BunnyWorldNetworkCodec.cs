using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Pacote BWN1 limitado ao tamanho seguro de um pacote de mod. Não é save,
/// não inclui caminhos e nunca deve ser recebido de um cliente pelo servidor.
/// A instalação da ponte de rede do Terraria pertence ao host C++.
/// </summary>
public static class BunnyWorldNetworkCodec
{
    public const int MaxPacketBytes = 60000;
    public const int MaxSystemBytes = 8192;
    private static readonly byte[] Magic = { 66, 87, 78, 49 };
    private static readonly UTF8Encoding Utf8 = new(false, true);

    private static string Key(string key)
    {
        int split = key?.IndexOf('/') ?? -1;
        if (split <= 0 || split == key!.Length - 1 ||
            split != key.LastIndexOf('/') ||
            BunnyWorldDocument.MakeKey(key.Substring(0, split),
                key.Substring(split + 1)) != key)
            throw new ArgumentException("WorldSystem identity invalid");
        return key;
    }

    public static byte[] Encode(IReadOnlyDictionary<string, BunnyWorldSystem> systems,
                                uint revision)
    {
        if (systems == null || revision == 0 ||
            systems.Count > BunnyWorldDocument.MaxSystems)
            throw new ArgumentException("World network state invalid");
        using var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream, Utf8, true))
        {
            writer.Write(Magic);
            writer.Write(revision);
            writer.Write(checked((ushort)systems.Count));
            var ordered = new SortedDictionary<string, BunnyWorldSystem>(StringComparer.Ordinal);
            foreach (var item in systems) ordered.Add(item.Key, item.Value);
            foreach (var entry in ordered)
            {
                byte[] key = Utf8.GetBytes(Key(entry.Key));
                if (key.Length > BunnyWorldDocument.MaxKeyBytes)
                    throw new InvalidDataException("Network key limit");
                using var sub = new MemoryStream();
                using (var subWriter = new BinaryWriter(sub, Utf8, true))
                    entry.Value.NetSend(subWriter);
                byte[] body = sub.ToArray();
                if (body.Length > MaxSystemBytes)
                    throw new InvalidDataException("Network system limit");
                writer.Write(checked((ushort)key.Length));
                writer.Write(key);
                writer.Write(checked((ushort)body.Length));
                writer.Write(body);
            }
        }
        byte[] payload = stream.ToArray();
        if (payload.Length + 4 > MaxPacketBytes)
            throw new InvalidDataException("Network packet limit");
        byte[] complete = new byte[payload.Length + 4];
        Buffer.BlockCopy(payload, 0, complete, 0, payload.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(
            complete.AsSpan(payload.Length), Crc(payload));
        return complete;
    }

    /// <summary>Valida tudo antes de executar um único NetReceive.</summary>
    public static bool TryDecode(ReadOnlySpan<byte> bytes, out uint revision,
        out SortedDictionary<string, byte[]> systems)
    {
        revision = 0;
        systems = new SortedDictionary<string, byte[]>(StringComparer.Ordinal);
        if (bytes.Length < 14 || bytes.Length > MaxPacketBytes) return false;
        var payload = bytes.Slice(0, bytes.Length - 4);
        if (BinaryPrimitives.ReadUInt32LittleEndian(bytes.Slice(payload.Length)) !=
            Crc(payload)) return false;
        try
        {
            using var stream = new MemoryStream(payload.ToArray(), false);
            using var reader = new BinaryReader(stream, Utf8, false);
            foreach (byte magic in Magic)
                if (reader.ReadByte() != magic) return false;
            uint sequence = reader.ReadUInt32();
            int count = reader.ReadUInt16();
            if (sequence == 0 || count > BunnyWorldDocument.MaxSystems)
                return false;
            var found = new SortedDictionary<string, byte[]>(StringComparer.Ordinal);
            string last = "";
            for (int i = 0; i < count; ++i)
            {
                int keyLength = reader.ReadUInt16();
                if (keyLength < 1 || keyLength > BunnyWorldDocument.MaxKeyBytes ||
                    keyLength > stream.Length - stream.Position) return false;
                byte[] keyBytes = reader.ReadBytes(keyLength);
                string key = Key(Utf8.GetString(keyBytes));
                if (string.CompareOrdinal(key, last) <= 0) return false;
                last = key;
                int valueLength = reader.ReadUInt16();
                if (valueLength > MaxSystemBytes ||
                    valueLength > stream.Length - stream.Position) return false;
                byte[] value = reader.ReadBytes(valueLength);
                if (value.Length != valueLength) return false;
                found.Add(key, value);
            }
            if (stream.Position != stream.Length) return false;
            revision = sequence;
            systems = found;
            return true;
        }
        catch (Exception ex) when (
            ex is IOException || ex is ArgumentException ||
            ex is OverflowException)
        {
            return false;
        }
    }

    private static uint Crc(ReadOnlySpan<byte> source)
    {
        uint sum = 0xFFFFFFFFu;
        foreach (byte b in source)
        {
            sum ^= b;
            for (int i = 0; i < 8; ++i)
                sum = (sum >> 1) ^ ((sum & 1) != 0 ? 0xEDB88320u : 0u);
        }
        return ~sum;
    }
}
