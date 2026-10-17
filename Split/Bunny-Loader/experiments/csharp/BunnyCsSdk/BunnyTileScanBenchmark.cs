using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyTileScanBenchmark
{
    private const int Side = 512;
    private const int SmallTotal = Side * Side;
    private const int MaxCells = 24 * 1024 * 1024;
    private const ulong Offset = 14695981039346656037UL;
    private const ulong Prime = 1099511628211UL;

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_start_mode", CallingConvention = CallingConvention.Cdecl)]
    private static extern int BeginNative(int mode);

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_progress", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ProgressNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_total", CallingConvention = CallingConvention.Cdecl)]
    private static extern int TotalNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_width", CallingConvention = CallingConvention.Cdecl)]
    private static extern int WidthNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_height", CallingConvention = CallingConvention.Cdecl)]
    private static extern int HeightNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_frames", CallingConvention = CallingConvention.Cdecl)]
    private static extern int FramesNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_read", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadNative([Out] ushort[] values, int capacity);

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_hash", CallingConvention = CallingConvention.Cdecl)]
    private static extern ulong HashNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_capture_us", CallingConvention = CallingConvention.Cdecl)]
    private static extern long CaptureUsNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_wall_us", CallingConvention = CallingConvention.Cdecl)]
    private static extern long WallUsNative();

    [DllImport("bunny", EntryPoint = "bunny_tile_scan_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void StopNative();

    private static ulong Scan(ushort[] values, out int active, out int tileZero)
    {
        ulong hash = Offset;
        int filled = 0;
        int found = 0;
        unchecked
        {
            for (int i = 0; i < values.Length; ++i)
            {
                ushort value = values[i];
                if (value != ushort.MaxValue) filled++;
                if (value == 0) found++;
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
        int result = BeginNative(0);
        return result == SmallTotal
            ? "Captura de 512x512 iniciada. Feche o editor para avancar os frames."
            : "Captura nao iniciada, status " + result + ".";
    }

    public static string StartFull()
    {
        int result = BeginNative(1);
        return result == 1
            ? "Varredura do mundo inteiro iniciada. Feche o editor, jogue normalmente e acompanhe o FPS."
            : "Captura do mundo inteiro nao iniciada, status " + result + ".";
    }

    public static string Progress()
    {
        int current = ProgressNative();
        if (current == -1) return "Nenhuma captura ativa.";
        if (current == -2) return "Captura falhou: memoria, dimensoes ou dados do mundo alterados.";
        int total = TotalNative();
        if (total <= 0) return "Aguardando o primeiro frame com mundo carregado.";
        int width = WidthNative();
        int height = HeightNative();
        if (current < total)
            return string.Format(CultureInfo.InvariantCulture,
                "Captura {0}x{1}: {2}/{3} tiles ({4:F1}%) em {5} frames. Feche o editor.",
                width, height, current, total, (double)current * 100.0 / total, FramesNative());
        return string.Format(CultureInfo.InvariantCulture,
            "Captura concluida: {0}x{1}, {2} tiles em {3} frames, {4:F3} s no jogo. Execute FinishFull().",
            width, height, total, FramesNative(), WallUsNative() / 1000000.0);
    }

    private static string Complete(int passes)
    {
        int count = TotalNative();
        if (count <= 0 || count > MaxCells || ProgressNative() != count)
            return "Captura incompleta. Use Progress() e feche o editor.";

        int width = WidthNative();
        int height = HeightNative();
        if (width <= 0 || height <= 0 || (long)width * height != count)
            return "Dimensoes do snapshot inconsistentes.";

        long copyStart = Stopwatch.GetTimestamp();
        ushort[] tiles = new ushort[count];
        int copied = ReadNative(tiles, count);
        long copyTicks = Stopwatch.GetTimestamp() - copyStart;
        if (copied != count) return "Falha ao copiar tiles nativos: " + copied + ".";

        ulong nativeHash = HashNative();
        ulong warmup = Scan(tiles, out _, out _);
        if (nativeHash != warmup)
            return "ERRO: checksum C# e checksum C++ divergentes.";

        ulong hash = 0;
        int active = 0;
        int zero = 0;
        long started = Stopwatch.GetTimestamp();
        for (int p = 0; p < passes; ++p)
        {
            hash = Scan(tiles, out active, out zero);
            if (hash != nativeHash)
                return "ERRO: checksum divergente na passada " + (p + 1) + ".";
        }
        long elapsedTicks = Stopwatch.GetTimestamp() - started;

        double managedMs = elapsedTicks * 1000.0 / Stopwatch.Frequency;
        double copyMs = copyTicks * 1000.0 / Stopwatch.Frequency;
        double wallMs = WallUsNative() / 1000.0;
        double captureCpuMs = CaptureUsNative() / 1000.0;
        double rate = managedMs <= 0 ? 0 : (double)count * passes / (managedMs * 1000.0);
        double pipeline = wallMs + copyMs + managedMs;
        return string.Format(CultureInfo.InvariantCulture,
            "Bunny Tile Scan OK | mundo={0}x{1} | tiles={2} | ativos={3} | tipo0={4} | passadas={5} | captura real={6:F3} s | CPU nativa={7:F3} ms | frames={8} | copia={9:F3} ms | C#={10:F3} ms | taxa={11:F2} M tiles/s | soma etapas={12:F3} s | checksum={13:X16}",
            width, height, count, active, zero, passes,
            wallMs / 1000.0, captureCpuMs, FramesNative(), copyMs,
            managedMs, rate, pipeline / 1000.0, hash);
    }

    public static string Finish() => Complete(10);
    public static string FinishFull() => Complete(1);
    public static string FinishFull10() => Complete(10);

    public static string Stop()
    {
        StopNative();
        return "Snapshot descartado e memoria nativa liberada.";
    }
}
