using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

public static class BunnyWorldMapBenchmark
{
    private const int MaxCells = 24 * 1024 * 1024;
    private const ulong Offset = 14695981039346656037UL;
    private const ulong Prime = 1099511628211UL;

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_start", CallingConvention = CallingConvention.Cdecl)]
    private static extern int BeginNative();

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_progress", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ProgressNative();

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_stat", CallingConvention = CallingConvention.Cdecl)]
    private static extern int StatNative(int key);

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_time", CallingConvention = CallingConvention.Cdecl)]
    private static extern long TimeNative(int key);

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_read", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadNative([Out] uint[] destination, int capacity);

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_hash", CallingConvention = CallingConvention.Cdecl)]
    private static extern ulong HashNative();

    [DllImport("bunny", EntryPoint = "bunny_worldmap_bench_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void StopNative();

    private static ulong Scan(uint[] cells, out int explored, out int known, out int painted)
    {
        ulong hash = Offset;
        int lit = 0;
        int marked = 0;
        int paintCount = 0;
        unchecked
        {
            for (int i = 0; i < cells.Length; ++i)
            {
                uint cell = cells[i];
                if ((cell & 0x00FF0000u) != 0) ++lit;
                if ((cell & 0x0000FFFFu) != 0) ++marked;
                if ((cell & 0x1F000000u) != 0) ++paintCount;
                hash ^= cell;
                hash *= Prime;
            }
        }
        explored = lit;
        known = marked;
        painted = paintCount;
        return hash;
    }

    public static string Start()
    {
        int result = BeginNative();
        return result == 1
            ? "Captura read-only do mapa visual iniciada. Feche o editor e acompanhe o FPS."
            : "Captura do mapa visual nao iniciada: " + result + ".";
    }

    public static string Progress()
    {
        int done = ProgressNative();
        if (done == -1) return "Nenhuma captura do mapa visual ativa.";
        if (done < 0) return "Falha na captura do mapa visual, codigo " + done + ".";
        int total = StatNative(0);
        if (total == 0) return "Aguardando frame do jogo com WorldMap carregado.";
        int width = StatNative(1);
        int height = StatNative(2);
        if (StatNative(6) != 1)
            return string.Format(CultureInfo.InvariantCulture,
                "WorldMap {0}x{1}: {2}/{3} celulas ({4:F1}%), {5}/{6} chunks, {7} frames.",
                width, height, done, total, (double)done * 100.0 / total,
                StatNative(4), StatNative(5), StatNative(3));

        return string.Format(CultureInfo.InvariantCulture,
            "WorldMap capturado: {0}x{1}, {2} celulas, {3} chunks, {4} frames, {5:F3} s reais. Execute Finish().",
            width, height, total, StatNative(5), StatNative(3),
            TimeNative(0) / 1000000.0);
    }

    public static string Finish()
    {
        int total = StatNative(0);
        if (total <= 0 || total > MaxCells || StatNative(6) != 1)
            return "Mapa visual ainda nao capturado. Feche o editor, espere e execute Progress().";
        uint[] cells = new uint[total];
        long copyStart = Stopwatch.GetTimestamp();
        int copied = ReadNative(cells, cells.Length);
        long copyTicks = Stopwatch.GetTimestamp() - copyStart;
        if (copied != total) return "Falha copiando WorldMap: " + copied + ".";

        long checksumStart = Stopwatch.GetTimestamp();
        ulong expected = HashNative();
        double checksumMs = (Stopwatch.GetTimestamp() - checksumStart) *
            1000.0 / Stopwatch.Frequency;

        Scan(cells, out _, out _, out _);
        long started = Stopwatch.GetTimestamp();
        ulong hash = Scan(cells, out int explored, out int known, out int painted);
        double managedMs = (Stopwatch.GetTimestamp() - started) *
            1000.0 / Stopwatch.Frequency;
        if (hash != expected)
            return "ERRO: checksum divergente entre WorldMap nativo e MonoVM.";

        double wallMs = TimeNative(0) / 1000.0;
        double nativeWorkMs = TimeNative(1) / 1000.0;
        double copyMs = copyTicks * 1000.0 / Stopwatch.Frequency;
        double sumMs = wallMs + copyMs + managedMs;
        return string.Format(CultureInfo.InvariantCulture,
            "WorldMap C# OK | mapa={0}x{1} | celulas={2} | iluminadas={3} | comTipo={4} | comTinta={5} | chunks={6} | frames={7} | captura={8:F3} s | CPU nativa={9:F3} ms | copia={10:F3} ms | C#={11:F3} ms | verificacao nativa={12:F3} ms | etapas={13:F3} s | checksum={14:X16}",
            StatNative(1), StatNative(2), total, explored, known, painted,
            StatNative(5), StatNative(3), wallMs / 1000.0,
            nativeWorkMs, copyMs, managedMs, checksumMs, sumMs / 1000.0, hash);
    }

    public static string Stop()
    {
        StopNative();
        return "Snapshot do mapa visual descartado.";
    }
}
