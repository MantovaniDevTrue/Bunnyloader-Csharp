// Same portable SHA-256 algorithm as BunnySourceCompiler/PortableSha256.cs.
// Kept managed to avoid introducing Android Crypto JNI initialization into the game process.
using System;

namespace BunnyLoader.Source;

internal sealed class ContentAssemblyHash
{
    private static readonly uint[] Seed =
    {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    private static readonly uint[] Constants =
    {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    private readonly uint[] state = new uint[8];
    private readonly uint[] schedule = new uint[64];
    private readonly byte[] block = new byte[64];
    private int buffered;
    private ulong count;

    public ContentAssemblyHash()
    {
        Reset();
    }

    public void AppendData(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        AppendData(data, 0, data.Length);
    }

    public void AppendData(byte[] data, int offset, int length)
    {
        ArgumentNullException.ThrowIfNull(data);
        ArgumentOutOfRangeException.ThrowIfNegative(offset);
        ArgumentOutOfRangeException.ThrowIfNegative(length);
        if (offset > data.Length - length)
            throw new ArgumentOutOfRangeException(nameof(length));

        count = unchecked(count + (ulong)length);
        while (length > 0)
        {
            if (buffered == 0 && length >= 64)
            {
                Transform(data, offset);
                offset += 64;
                length -= 64;
                continue;
            }

            int take = Math.Min(64 - buffered, length);
            Buffer.BlockCopy(data, offset, block, buffered, take);
            buffered += take;
            offset += take;
            length -= take;
            if (buffered == 64)
            {
                Transform(block, 0);
                buffered = 0;
            }
        }
    }

    public byte[] GetHashAndReset()
    {
        ulong bits = unchecked(count << 3);
        block[buffered++] = 0x80;
        if (buffered > 56)
        {
            Array.Clear(block, buffered, 64 - buffered);
            Transform(block, 0);
            buffered = 0;
        }

        Array.Clear(block, buffered, 56 - buffered);
        for (int i = 0; i < 8; ++i)
            block[56 + i] = (byte)(bits >> (56 - 8 * i));
        Transform(block, 0);

        byte[] result = new byte[32];
        for (int i = 0; i < 8; ++i)
        {
            result[i * 4] = (byte)(state[i] >> 24);
            result[i * 4 + 1] = (byte)(state[i] >> 16);
            result[i * 4 + 2] = (byte)(state[i] >> 8);
            result[i * 4 + 3] = (byte)state[i];
        }

        Reset();
        return result;
    }

    private void Reset()
    {
        Array.Copy(Seed, state, Seed.Length);
        count = 0;
        buffered = 0;
        Array.Clear(block);
    }

    private static uint RotateRight(uint value, int count) =>
        (value >> count) | (value << (32 - count));

    private void Transform(byte[] input, int offset)
    {
        for (int i = 0; i < 16; ++i)
        {
            int j = offset + i * 4;
            schedule[i] = ((uint)input[j] << 24) |
                          ((uint)input[j + 1] << 16) |
                          ((uint)input[j + 2] << 8) |
                          input[j + 3];
        }

        for (int i = 16; i < 64; ++i)
        {
            uint p = schedule[i - 15];
            uint q = schedule[i - 2];
            uint s0 = RotateRight(p, 7) ^ RotateRight(p, 18) ^ (p >> 3);
            uint s1 = RotateRight(q, 17) ^ RotateRight(q, 19) ^ (q >> 10);
            schedule[i] = unchecked(schedule[i - 16] + s0 + schedule[i - 7] + s1);
        }

        uint a = state[0], b = state[1], c = state[2], d = state[3];
        uint e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i)
        {
            uint t0 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
            uint choose = (e & f) ^ (~e & g);
            uint t1 = unchecked(h + t0 + choose + Constants[i] + schedule[i]);
            uint t2 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
            uint majority = (a & b) ^ (a & c) ^ (b & c);
            uint t3 = unchecked(t2 + majority);

            h = g;
            g = f;
            f = e;
            e = unchecked(d + t1);
            d = c;
            c = b;
            b = a;
            a = unchecked(t1 + t3);
        }

        state[0] = unchecked(state[0] + a);
        state[1] = unchecked(state[1] + b);
        state[2] = unchecked(state[2] + c);
        state[3] = unchecked(state[3] + d);
        state[4] = unchecked(state[4] + e);
        state[5] = unchecked(state[5] + f);
        state[6] = unchecked(state[6] + g);
        state[7] = unchecked(state[7] + h);
    }
}
