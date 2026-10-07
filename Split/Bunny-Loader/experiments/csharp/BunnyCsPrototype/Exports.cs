using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace BunnyCsPrototype;

public static class Exports
{
    private const int Version = 1;

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int GetVersion()
    {
        return Version;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_add", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int Add(int a, int b)
    {
        return a + b;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_checksum", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static long Checksum(int iterations)
    {
        if (iterations < 0) iterations = 0;
        long value = 0x13579BDF;
        for (int i = 0; i < iterations; i++)
        {
            value ^= (long)i * 0x9E3779B1;
            value = (value << 7) | ((long)((ulong)value >> 57));
            value += 0x632BE59BD9B4E019L;
        }
        return value;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_player_sample", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static long PlayerSample(float x, float y, int life, int maxLife)
    {
        long a = BitConverter.SingleToInt32Bits(x);
        long b = BitConverter.SingleToInt32Bits(y);
        return (a << 32) ^ (uint)b ^ ((long)life << 16) ^ (uint)maxLife;
    }
}
