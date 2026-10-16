using System;
using BunnyLoader.Source;

static class Test
{
    static void Check(bool ok, string message)
    {
        if (!ok) throw new Exception(message);
    }

    static void Main()
    {
        BunnyVisual.ClearPending();
        Check(BunnyVisual.PendingCount == 0, "empty queue");
        for (int i = 0; i < BunnyVisual.MaxCommandsPerFrame; i++)
            Check(BunnyVisual.SpawnParticle(0, i, "sprite.png", i, 0, 1, 0xFFFFFFFF, 4), "enqueue");
        Check(!BunnyVisual.SpawnParticle(0, 100, "", 0, 0, 1, 0, 1), "capacity bound");
        for (int i = 0; i < 30; i++)
        {
            Check(BunnyVisual.TryTake(out var cmd), "take first batch");
            Check(cmd.EffectId == i, "FIFO first batch");
        }
        for (int i = 64; i < 94; i++)
            Check(BunnyVisual.SetPlayerOverlay(0, i, "shield.png", 1, 0.5f, 0xFF99CCFF, 4), "wrap enqueue");
        for (int i = 30; i < 94; i++)
        {
            Check(BunnyVisual.TryTake(out var cmd), "take second batch");
            Check(cmd.EffectId == i, "FIFO after wrap");
        }
        Check(BunnyVisual.PendingCount == 0, "drained");
        Check(!BunnyVisual.SetPlayerOverlay(-1, 1, "", 1, 1, 0, 1), "invalid player");
        Check(!BunnyVisual.SetPlayerOverlay(0, 1, "", float.NaN, 1, 0, 1), "invalid scale");
        Check(!BunnyVisual.SetPlayerOverlay(0, 1, "", 1, 2f, 0, 1), "invalid alpha");
        Check(BunnyVisual.ClearPlayerOverlay(0, 1), "clear command");
        BunnyVisual.ClearPending();
        Check(BunnyVisual.PendingCount == 0, "clear pending");
        Check(BunnyVisual.SetPlayerOverlay(0, 7, "shield.png", 1, 0.2f, 0xFF99CCFF, 3), "first overlay");
        Check(BunnyVisual.SetPlayerOverlay(0, 7, "shield.png", 1, 0.9f, 0xFF99CCFF, 3), "replace overlay");
        Check(BunnyVisual.PendingCount == 1, "overlay coalesced");
        Check(BunnyVisual.TryTake(out var overlay) && overlay.Opacity == 0.9f, "latest overlay retained");
        Check(BunnyVisual.SetPlayerOverlay(0, 7, "shield.png", 1, 0.2f, 0xFF99CCFF, 3), "overlay before clearing");
        Check(BunnyVisual.ClearPlayerOverlay(0, 7), "overlay clear");
        Check(BunnyVisual.PendingCount == 1, "clear coalesced");
        Check(BunnyVisual.TryTake(out var clear) && clear.Kind == BunnyVisualKind.ClearOverlay, "clear takes precedence");
        Console.WriteLine("BunnyVisual queue tests passed");
    }
}
