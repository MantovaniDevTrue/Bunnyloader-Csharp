using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyTileScanBenchmark
{
    private const int Side = 512;
    private const int Total = Side * Side;
    private const int Passes = 10;
    private const ulong Offset = 14695981039346656037UL;
    private const ulong Prime = 1099511628211UL;

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_start", CallingConvention = CallingConvention.Cdecl)]
    private static extern int BeginNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_progress", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ProgressNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_read", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadNative([Out] ushort[] values, int capacity);

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_hash", CallingConvention = CallingConvention.Cdecl)]
    private static extern ulong HashNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_capture_us", CallingConvention = CallingConvention.Cdecl)]
    private static extern long CaptureUsNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void StopNative();

    private static ulong Scan(ushort[] values, out int active, out int tileZero)
    {
        ulong hash = Offset;
        int found = 0;
        int filled = 0;
        unchecked
        {
            for (int i = 0; i < values.Length; i++)
            {
                ushort value = values[i];
                if (value != ushort.MaxValue)
                    filled++;
                if (value == 0)
                    found++;
                hash ^= value;
                hash *= Prime;
            }
        }
        active = filled;
        tileZero = found;
        return hash;
    }

    public static string Start()
    {
        int result = BeginNative();
        return result == Total
            ? "Varredura iniciada: 512x512 tiles reais, captura em 32 lotes."
            : "Nao foi possivel iniciar captura (status " + result + ").";
    }

    public static string Progress()
    {
        int result = ProgressNative();
        if (result == -1) return "Varredura ainda nao iniciada.";
        if (result == -2) return "Captura falhou: mundo indisponivel ou realocado.";
        if (result < Total)
            return "Captura: " + result.ToString(CultureInfo.InvariantCulture) +
                " / " + Total.ToString(CultureInfo.InvariantCulture) + " tiles.";
        return "Captura pronta: 262144 tiles. Execute Finish().";
    }

    public static string Finish()
    {
        if (ProgressNative() != Total)
            return "Captura ainda nao concluida. Use Progress() e feche o editor enquanto captura.";
        ushort[] values = new ushort[Total];
        int copied = ReadNative(values, values.Length);
        if (copied != Total) return "Falha ao copiar snapshot (status " + copied + ").";

        ulong expected = HashNative();
        long captureUs = CaptureUsNative();
        ulong warmup = Scan(values, out _, out _);
        if (warmup != expected)
            return "ERRO: checksum C# diverge do checksum C++ do mundo.";

        int active = 0;
        int zero = 0;
        ulong actual = 0;
        long start = Stopwatch.GetTimestamp();
        for (int i = 0; i < Passes; i++)
        {
            actual = Scan(values, out active, out zero);
            if (actual != expected)
                return "ERRO: checksum inconsistente na passada " + (i + 1) + ".";
        }
        long ticks = Stopwatch.GetTimestamp() - start;
        double elapsedMs = ticks * 1000.0 / Stopwatch.Frequency;
        double millionsPerSecond = elapsedMs > 0
            ? Total * Passes / (elapsedMs * 1000.0) : 0;

        return string.Format(CultureInfo.InvariantCulture,
            "Bunny Tile Scan OK | 512x512 | {0} passadas | tiles={1} | ativos={2} | tipo0={3} | C#={4:F3} ms | {5:F2} M tiles/s | captura nativa={6:F3} ms | checksum={7:X16}",
            Passes, Total, active, zero, elapsedMs, millionsPerSecond, captureUs / 1000.0, actual);
    }

    public static string Stop()
    {
        StopNative();
        return "Snapshot descartado.";
    }
}
