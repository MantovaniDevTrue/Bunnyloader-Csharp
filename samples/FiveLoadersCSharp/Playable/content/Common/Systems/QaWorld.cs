using System;
using System.IO;
using System.Threading;
using BunnyLoader.Source;

namespace FiveLoadersCSharp;

// Como Common/Systems no tMod: a organização é familiar, mas todos os hooks
// e os dados atravessam a ponte do Terraria Mobile, sem runtime JavaScript.
public sealed class QaWorld : BunnyWorldSystem
{
    public static int SavedPresses;
    public static int Saves;
    public static int ClockMultiplier = 1;
    public static bool Active;

    public override void ClearWorld()
    {
        // O dispatcher chama ClearWorld antes de carregar outro mundo.
        // Os contadores não podem vazar de um arquivo .wld para outro.
        Active = false;
        SavedPresses = 0;
        Saves = 0;
        ClockMultiplier = 1;
        QaUpdates.Reset();
    }
    public override void PostWorldLoad() => Active = true;
    public override void OnWorldUnload() => Active = false;
    public override void SaveWorldData(BunnyWorldTag tag)
    {
        tag.SetInt32("presses", Volatile.Read(ref SavedPresses));
        tag.SetInt32("saves", Interlocked.Increment(ref Saves));
    }
    public override void LoadWorldData(BunnyWorldTag tag)
    {
        SavedPresses = Math.Max(0, tag.GetInt32("presses"));
        Saves = Math.Max(0, tag.GetInt32("saves"));
    }
    public override void NetSend(BinaryWriter writer)
    {
        // Só o servidor envia. O cliente não grava esse estado em seu disco.
        writer.Write(Volatile.Read(ref SavedPresses));
        writer.Write(Volatile.Read(ref Saves));
    }
    public override void NetReceive(BinaryReader reader)
    {
        int presses = reader.ReadInt32();
        int saves = reader.ReadInt32();
        if (presses < 0 || saves < 0) throw new InvalidDataException("QA counter");
        SavedPresses = presses;
        Saves = saves;
    }
    public static void AddPress()
    {
        if (Active) Interlocked.Increment(ref SavedPresses);
    }
}
