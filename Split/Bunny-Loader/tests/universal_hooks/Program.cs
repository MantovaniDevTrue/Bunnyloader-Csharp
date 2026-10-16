using System.Runtime.InteropServices;
using BunnyLoader.Source;

static class Program
{
    [DllImport("bunny")] static extern int test_slot();
    [DllImport("bunny")] static extern int test_removes();
    [DllImport("bunny")] static extern int test_originals();
    [DllImport("bunny")] static extern void test_begin(long ticket);
    [DllImport("bunny")] static extern void test_int_argument(int value);
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
        int failingSlot = test_slot();
        Check(BunnyHooks.Dispatch(failingSlot, ticket++, 123) == 0 && !failing.IsDisposed && BunnyHooks.Faults == 1);
        BunnyHooks.RemoveSlot(failingSlot);
        Check(failing.IsDisposed);
        var duplicate = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) => { original.Invoke(); original.Invoke(); });
        test_begin(ticket);
        int duplicateSlot = test_slot();
        Check(BunnyHooks.Dispatch(duplicateSlot, ticket++, 123) == 0 && test_originals() == 1 && !duplicate.IsDisposed);
        BunnyHooks.RemoveSlot(duplicateSlot);
        Check(duplicate.IsDisposed);
        var after = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) => { original.Invoke(); throw new Exception("after"); });
        test_begin(ticket);
        int afterSlot = test_slot();
        Check(BunnyHooks.Dispatch(afterSlot, ticket++, 123) == 0 && !after.IsDisposed && test_originals() == 1);
        BunnyHooks.RemoveSlot(afterSlot);
        Check(after.IsDisposed);
        long beforeTransient = BunnyHooks.NativeAccessRecoveries;
        long beforeFaults = BunnyHooks.Faults;
        foreach (BunnyAccessStatus status in new[] { BunnyAccessStatus.Busy, BunnyAccessStatus.TimedOut })
        {
            var transient = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) =>
            {
                original.Invoke();
                throw new BunnyAccessException(status, "Lote nativo");
            });
            int transientSlot = test_slot();
            test_begin(ticket);
            Check(BunnyHooks.Dispatch(transientSlot, ticket++, 123) == 1 && !transient.IsDisposed && test_originals() == 1);
            transient.Dispose();
        }
        Check(BunnyHooks.NativeAccessRecoveries == beforeTransient + 2 && BunnyHooks.Faults == beforeFaults);
        var fatalAccess = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) =>
        {
            original.Invoke();
            throw new BunnyAccessException(BunnyAccessStatus.TypeMismatch, "Tipo incorreto");
        });
        int fatalAccessSlot = test_slot();
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(fatalAccessSlot, ticket++, 123) == 0 && BunnyHooks.Faults == beforeFaults + 1);
        fatalAccess.Dispose();
        var beforeOriginal = scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) =>
            throw new BunnyAccessException(BunnyAccessStatus.TimedOut, "Sem original"));
        int beforeOriginalSlot = test_slot();
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(beforeOriginalSlot, ticket++, 123) == 0 && BunnyHooks.Faults == beforeFaults + 2);
        beforeOriginal.Dispose();
        var stale = scope.HookVoid("Terraria.Player", "ResetEffects", (original, _) => original.Invoke());
        int staleSlot = test_slot();
        test_begin(ticket + 1);
        Check(BunnyHooks.Dispatch(staleSlot, ticket++, 123) == 0 && !stale.IsDisposed);
        test_begin(ticket);
        Check(BunnyHooks.Dispatch(staleSlot, ticket++, 123) == 1 && !stale.IsDisposed);
        stale.Dispose();
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
        using (var typed = new BunnyHookScope())
        {
            int seen = 0;
            BunnyOriginalInt32 expired = default;
            var typedHandle = typed.HookInt32("Terraria.Player", "Int32Transform", (original, self, value) =>
            {
                Check(self == (nint)123 && value == 10);
                expired = original;
                seen++;
                return original.Invoke() + 20;
            });
            int typedSlot = test_slot();
            test_begin(ticket);
            test_int_argument(10);
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 10, out int ok, out int value);
            Check(ok == 1 && value == 37 && test_originals() == 1 && seen == 1);
            Throws(() => expired.Invoke());
            typedHandle.Dispose();
            var replace = typed.HookInt32("Terraria.Player", "Int32Transform", (_, _, value) => value * 3);
            typedSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 9, out ok, out value);
            Check(ok == 1 && value == 27 && test_originals() == 0);
            replace.Dispose();
            long beforeTypedRecoveries = BunnyHooks.NativeAccessRecoveries;
            var typedTransient = typed.HookInt32("Terraria.Player", "Int32Transform", (original, _, value) =>
            {
                _ = original.Invoke();
                throw new BunnyAccessException(BunnyAccessStatus.TimedOut, "Metodo nativo");
            });
            typedSlot = test_slot();
            test_begin(ticket);
            test_int_argument(8);
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 8, out ok, out value);
            Check(ok == 1 && value == 15 && test_originals() == 1);
            Check(BunnyHooks.NativeAccessRecoveries == beforeTypedRecoveries + 1 && !typedTransient.IsDisposed);
            typedTransient.Dispose();
            var invalid = typed.HookInt32("Terraria.Player", "Int32Transform", (original, _, _) =>
            {
                original.Invoke();
                original.Invoke();
                return 0;
            });
            typedSlot = test_slot();
            test_begin(ticket);
            test_int_argument(2);
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 2, out ok, out value);
            Check(ok == 0 && test_originals() == 1 && !invalid.IsDisposed);
            BunnyHooks.RemoveSlot(typedSlot);
            Check(invalid.IsDisposed);
        }
        Check(BunnyHooks.GetStatus().Contains("void()"));
        Console.WriteLine("C# SDK: transient native Busy/TimedOut preserved after original, fatal errors remain terminal, typed original return, 10000 allocations PASS");
    }
}
