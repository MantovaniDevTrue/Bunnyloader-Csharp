using System;
using System.Buffers.Binary;
using System.Text;

namespace BunnyLoader.Source;

public sealed class BunnyNetWriter
{
    public const int MaxBytes = 96;
    private readonly byte[] buffer = new byte[MaxBytes];
    private int length;

    public int Length => length;

    public void WriteFlags(params bool[] flags)
    {
        ArgumentNullException.ThrowIfNull(flags);
        if (flags.Length < 1 || flags.Length > 16)
            throw new ArgumentOutOfRangeException(nameof(flags));
        int count = (flags.Length + 7) / 8;
        Ensure(1 + count);
        buffer[length++] = (byte)flags.Length;
        for (int offset = 0; offset < count; offset++)
        {
            byte packed = 0;
            for (int bit = 0; bit < 8 && offset * 8 + bit < flags.Length; bit++)
                if (flags[offset * 8 + bit])
                    packed |= (byte)(1 << bit);
            buffer[length++] = packed;
        }
    }

    public void WriteByte(byte value)
    {
        Ensure(1);
        buffer[length++] = value;
    }

    public void WriteInt32(int value)
    {
        Ensure(4);
        BinaryPrimitives.WriteInt32LittleEndian(buffer.AsSpan(length, 4), value);
        length += 4;
    }

    public void WriteString(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        byte[] bytes = new UTF8Encoding(false, true).GetBytes(value);
        if (bytes.Length > 64)
            throw new ArgumentOutOfRangeException(nameof(value));
        Ensure(1 + bytes.Length);
        buffer[length++] = (byte)bytes.Length;
        bytes.AsSpan().CopyTo(buffer.AsSpan(length));
        length += bytes.Length;
    }

    private void Ensure(int count)
    {
        if (length + count > MaxBytes)
            throw new InvalidOperationException("Bunny network payload exceeds 96 bytes");
    }

    public byte[] ToArray() => buffer.AsSpan(0, length).ToArray();
}

public sealed class BunnyNetReader
{
    private readonly byte[] buffer;
    private int offset;
    public int Remaining => buffer.Length - offset;

    public BunnyNetReader(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (data.Length > BunnyNetWriter.MaxBytes)
            throw new ArgumentOutOfRangeException(nameof(data));
        buffer = data;
    }

    public bool[] ReadFlags()
    {
        int count = ReadByte();
        if (count < 1 || count > 16)
            throw new FormatException("Invalid flags count");
        int width = (count + 7) / 8;
        Require(width);
        var flags = new bool[count];
        for (int i = 0; i < count; i++)
            flags[i] = (buffer[offset + i / 8] & (1 << (i % 8))) != 0;
        if (count % 8 != 0 &&
            (buffer[offset + width - 1] >> (count % 8)) != 0)
            throw new FormatException("Unused flag bits must be zero");
        offset += width;
        return flags;
    }

    public byte ReadByte()
    {
        Require(1);
        return buffer[offset++];
    }

    public int ReadInt32()
    {
        Require(4);
        int value = BinaryPrimitives.ReadInt32LittleEndian(buffer.AsSpan(offset, 4));
        offset += 4;
        return value;
    }

    public string ReadString()
    {
        int count = ReadByte();
        if (count > 64) throw new FormatException("Invalid network string length");
        Require(count);
        string result = new UTF8Encoding(false, true).GetString(buffer, offset, count);
        offset += count;
        return result;
    }

    private void Require(int count)
    {
        if (count > Remaining)
            throw new FormatException("Truncated Bunny network packet");
    }
}
