using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace BunnyCsPrototype;

public static class Exports
{
    private const int Version = 5;

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int GetVersion()
    {
        return Version;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_host_api_version", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int GetHostApiVersion()
    {
        return BunnyHost.ApiVersion;
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

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct NpcSample
    {
        public int Index;
        public int Type;
        public int Life;
        public int LifeMax;
        public float X;
        public float Y;
        public float Vx;
        public float Vy;
        public int Flags;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct ProjectileSample
    {
        public int Index;
        public int Type;
        public int Owner;
        public int TimeLeft;
        public int Damage;
        public float X;
        public float Y;
        public float Vx;
        public float Vy;
        public int Flags;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_npc_scan", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe double NpcScan(NpcSample* samples, int count, float playerX, float playerY, int repeats)
    {
        if (samples == null || count <= 0 || repeats <= 0) return 0.0;
        if (count > 4096) count = 4096;
        if (repeats > 1000000) repeats = 1000000;

        double checksum = 0.0;
        for (int repeat = 0; repeat < repeats; repeat++)
        {
            int bestIndex = -1;
            double bestDistance = double.MaxValue;
            for (int i = 0; i < count; i++)
            {
                NpcSample sample = samples[i];
                if ((sample.Flags & 1) == 0 || (sample.Flags & 6) != 0 || sample.Life <= 0) continue;

                double dx = (double)sample.X - (double)playerX;
                double dy = (double)sample.Y - (double)playerY;
                double distance = dx * dx + dy * dy;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = sample.Index;
                }
            }

            checksum += bestIndex;
            if (bestIndex >= 0) checksum += bestDistance * 0.000000001;
        }
        return checksum;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_projectile_scan", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe double ProjectileScan(ProjectileSample* samples, int count, int repeats)
    {
        if (samples == null || count <= 0 || repeats <= 0) return 0.0;
        if (count > 32768) count = 32768;
        if (repeats > 1000000) repeats = 1000000;

        double checksum = 0.0;
        for (int repeat = 0; repeat < repeats; repeat++)
        {
            for (int i = 0; i < count; i++)
            {
                ProjectileSample sample = samples[i];
                if ((sample.Flags & 1) == 0) continue;

                checksum += sample.Index * 0.00001;
                checksum += sample.Type * 0.00002;
                checksum += sample.Owner * 0.00003;
                checksum += sample.TimeLeft * 0.00004;
                checksum += sample.Damage * 0.00005;
                checksum += (double)sample.X * 0.000001;
                checksum += (double)sample.Y * 0.000002;
                checksum += (double)sample.Vx * 0.0003;
                checksum += (double)sample.Vy * 0.0004;
            }
        }
        return checksum;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct HostProjectileState
    {
        public int Slot;
        public int Type;
        public int Owner;
        public int Generation;
        public int TargetIndex;
        public float X;
        public float Y;
        public float Vx;
        public float Vy;
        public float Rotation;
    }

    [UnmanagedCallersOnly(EntryPoint = "bunnycs_host_projectile_tick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int HostProjectileTick(
        HostProjectileState* state,
        NpcSample* samples,
        int count,
        float playerX,
        float playerY)
    {
        return BunnyHost.DispatchProjectile(state, samples, count, playerX, playerY) ? 1 : 0;
    }

}
