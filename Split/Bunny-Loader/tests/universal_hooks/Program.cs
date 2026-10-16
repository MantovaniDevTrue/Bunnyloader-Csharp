using System.Runtime.InteropServices;
using BunnyLoader.Source;

static class Program
{
    [DllImport("bunny")] static extern int test_slot();
    [DllImport("bunny")] static extern int test_removes();
    [DllImport("bunny")] static extern int test_originals();
    [DllImport("bunny")] static extern void test_begin(long ticket);
    static void Check(bool value) { if (!value) throw new Exception("Universal hook assertion failed"); }
    static void Throws(Action body) { try { body(); } catch (InvalidOperationException) { return; } throw new Exception("Expected invalid original"); }
    static void Main()
    {
        using var scope = new BunnyHookScope();
        BunnyOriginalVoid escaped = default;
        long ticket = 1;
        int calls = 0;
        var handle = BunnyHooks.Hook(scope, "Terraria.Player", "ResetEffects", (original, self) =>
        {
            Check(self == (nint)123);
            original.Invoke();
            escaped = original;
            calls++;
        });
        int slot = test_slot();
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(slot, ticket++, 123) == 1 && calls == 1 && test_originals() == 1);
        Throws(escaped.Invoke);
        handle.Dispose();
        Check(handle.IsDisposed && BunnyHooks.Dispatch(slot, ticket++, 123) == 0);
        int removed = test_removes();
        handle.Dispose();
        Check(test_removes() == removed);
        var failing = scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) => throw new Exception("before"));
        Check(BunnyHooks.Dispatch(test_slot(), ticket++, 123) == 0 && failing.IsDisposed && BunnyHooks.Faults == 1);
        var duplicate = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) => { original.Invoke(); original.Invoke(); });
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(test_slot(), ticket++, 123) == 0 && test_originals() == 1 && duplicate.IsDisposed);
        var after = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) => { original.Invoke(); throw new Exception("after"); });
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(test_slot(), ticket++, 123) == 0 && after.IsDisposed && test_originals() == 1);
        var suppressed = scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) => { });
        test_begin(ticket);
        BunnyHooks.Invoke(test_slot(), ticket++, 123, out int result);
        Check(result == 1 && test_originals() == 0);
        scope.Dispose();
        Check(suppressed.IsDisposed);
        try { scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) => { }); throw new Exception("Scope reused"); }
        catch (ObjectDisposedException) { }
        using (var frequency = new BunnyHookScope()) {
            frequency.HookVoid("Terraria.Player", "ResetEffects", (original, _) => original.Invoke());
            int frequencySlot = test_slot();
            for (int i = 0; i < 10; ++i) { test_begin(ticket); Check(BunnyHooks.Dispatch(frequencySlot, ticket++, 123) == 1); }
            long allocated = GC.GetAllocatedBytesForCurrentThread();
            for (int i = 0; i < 10000; ++i) { test_begin(ticket); Check(BunnyHooks.Dispatch(frequencySlot, ticket++, 123) == 1); }
            Check(GC.GetAllocatedBytesForCurrentThread() == allocated);
        }
        string path = typeof(Program).Assembly.Location;
        int registered = BunnyUniversalHookDispatcher.Register("example", path);
        Check(registered == 1);
        Check(BunnyUniversalHookDispatcher.Register("example", path) == registered);
        BunnyUniversalHookDispatcher.UnloadAll();
        Check(BunnyHooks.Dispatch(test_slot(), ticket++, 123) == 0);
        for (int i = 0; i < 100; ++i) {
            using var reused = new BunnyHookScope();
            reused.HookVoid("Terraria.Player", "ResetEffects", (_, _) => { });
        }
        Check(BunnyHooks.GetStatus().Contains("void()"));
        Console.WriteLine("C# SDK: delegate/original, exceptions, expired original, dispose, module auto-unload, slot reuse PASS");
    }
}
