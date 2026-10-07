using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace BunnyCsPrototype;

public static class Exports
{
    private const int Version = 2;

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
    [UnmanagedCallersOnly(EntryPoint = "bunnycs_math_batch", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static uint MathBatch(int iterations, uint seed)
    {
        if (iterations < 0) iterations = 0;
        uint x = seed ^ 0xA5A5A5A5u;
        unchecked
        {
            for (int i = 0; i < iterations; i++)
            {
                x += (uint)i * 0x9E3779B9u;
                x = (x << 7) | (x >> 25);
                x ^= x >> 13;
                x *= 0x85EBCA6Bu;
                x ^= x << 5;
            }
        }
        return x;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_projectile_batch", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static double ProjectileBatch(int count, int steps)
    {
        if (count < 1) count = 1;
        if (count > 16384) count = 16384;
        if (steps < 1) steps = 1;
        if (steps > 2000) steps = 2000;

        var x = new double[count];
        var y = new double[count];
        var vx = new double[count];
        var vy = new double[count];

        for (int i = 0; i < count; i++)
        {
            x[i] = (i % 64) * 3.125 - 100.0;
            y[i] = (i % 37) * 1.75 + 40.0;
            vx[i] = ((i % 17) - 8) * 0.03125;
            vy[i] = ((i % 13) - 6) * 0.0275;
        }

        double checksum = 0.0;
        for (int step = 0; step < steps; step++)
        {
            for (int i = 0; i < count; i++)
            {
                double px = x[i] + vx[i];
                double py = y[i] + vy[i];
                double dx = vx[i] * 0.9995;
                double dy = vy[i] + 0.015;

                if (py > 900.0)
                {
                    py = 900.0;
                    dy = -Math.Abs(dy) * 0.72;
                }
                if (px < -1200.0 || px > 1200.0)
                    dx = -dx;

                x[i] = px;
                y[i] = py;
                vx[i] = dx;
                vy[i] = dy;

                checksum += px * 0.0001 + py * 0.0002 + dx * 0.3 + dy * 0.2;
            }
        }

        return checksum;
    }

}
