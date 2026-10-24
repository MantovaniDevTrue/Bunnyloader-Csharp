using System;
using System.Threading;
using BunnyLoader.Source;

namespace FiveLoadersCSharp;

// Este mod de QA habilita deliberadamente os 19 callbacks. Cada contador
// prova que o hook chamou o C#, sem varrer entidades ou escrever no log por frame.
public sealed class QaUpdates : BunnyWorldUpdateLoader
{
    public static readonly long[] Counts = new long[19];
    private static void Tick(int index) => Interlocked.Increment(ref Counts[index]);
    public static void Reset() { for (int i = 0; i < Counts.Length; i++) Interlocked.Exchange(ref Counts[i], 0); }
    public static int Observed { get { int n = 0; for (int i = 0; i < Counts.Length; i++) if (Interlocked.Read(ref Counts[i]) > 0) n++; return n; } }
    public static string Missing()
    {
        string missing = "";
        if (Interlocked.Read(ref Counts[0]) == 0) missing += " PreUpdateEntities";
        if (Interlocked.Read(ref Counts[1]) == 0) missing += " PostUpdateEverything";
        if (Interlocked.Read(ref Counts[2]) == 0) missing += " PreUpdatePlayers";
        if (Interlocked.Read(ref Counts[3]) == 0) missing += " PostUpdatePlayers";
        if (Interlocked.Read(ref Counts[4]) == 0) missing += " PreUpdateNPCs";
        if (Interlocked.Read(ref Counts[5]) == 0) missing += " PostUpdateNPCs";
        if (Interlocked.Read(ref Counts[6]) == 0) missing += " PreUpdateGores";
        if (Interlocked.Read(ref Counts[7]) == 0) missing += " PostUpdateGores";
        if (Interlocked.Read(ref Counts[8]) == 0) missing += " PreUpdateProjectiles";
        if (Interlocked.Read(ref Counts[9]) == 0) missing += " PostUpdateProjectiles";
        if (Interlocked.Read(ref Counts[10]) == 0) missing += " PreUpdateItems";
        if (Interlocked.Read(ref Counts[11]) == 0) missing += " PostUpdateItems";
        if (Interlocked.Read(ref Counts[12]) == 0) missing += " PreUpdateDusts";
        if (Interlocked.Read(ref Counts[13]) == 0) missing += " PostUpdateDusts";
        if (Interlocked.Read(ref Counts[14]) == 0) missing += " PreUpdateInvasions";
        if (Interlocked.Read(ref Counts[15]) == 0) missing += " PostUpdateInvasions";
        if (Interlocked.Read(ref Counts[16]) == 0) missing += " PostUpdateInput";
        if (Interlocked.Read(ref Counts[17]) == 0) missing += " UpdateUI";
        if (Interlocked.Read(ref Counts[18]) == 0) missing += " ModifyTimeRate";
        return missing.Length == 0 ? "nenhum" : missing;
    }
    protected override void PreUpdateEntities() => Tick(0);
    protected override void PostUpdateEverything() => Tick(1);
    protected override void PreUpdatePlayers() => Tick(2);
    protected override void PostUpdatePlayers() => Tick(3);
    protected override void PreUpdateNPCs() => Tick(4);
    protected override void PostUpdateNPCs() => Tick(5);
    protected override void PreUpdateGores() => Tick(6);
    protected override void PostUpdateGores() => Tick(7);
    protected override void PreUpdateProjectiles() => Tick(8);
    protected override void PostUpdateProjectiles() => Tick(9);
    protected override void PreUpdateItems() => Tick(10);
    protected override void PostUpdateItems() => Tick(11);
    protected override void PreUpdateDusts() => Tick(12);
    protected override void PostUpdateDusts() => Tick(13);
    protected override void PreUpdateInvasions() => Tick(14);
    protected override void PostUpdateInvasions() => Tick(15);
    protected override void PostUpdateInput() => Tick(16);
    protected override void UpdateUI(nint gameTime) => Tick(17);
    protected override void ModifyTimeRate(ref BunnyTimeRateValues rates)
    {
        Tick(18);
        // /lote tempo 2 acelera somente o relógio; blocos e eventos mantêm a taxa.
        // O host aplica valores na thread do jogo e preserva frações.
        rates.TimeRate *= Volatile.Read(ref QaWorld.ClockMultiplier);
    }
}
