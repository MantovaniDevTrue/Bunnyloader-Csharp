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
        var busy = scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) =>
            throw new BunnyAccessException(BunnyAccessStatus.Busy, "rpc"));
        int busySlot = test_slot();
        Check(BunnyHooks.Dispatch(busySlot, ticket++, 123) == 2 && !busy.IsDisposed && BunnyHooks.TransientRetries == 1);
        BunnyHooks.RemoveSlot(busySlot);
        Check(busy.IsDisposed);
        var timed = scope.HookVoid("Terraria.Player", "ResetEffects", (_, _) =>
            throw new BunnyAccessException(BunnyAccessStatus.TimedOut, "rpc"));
        int timedSlot = test_slot();
        Check(BunnyHooks.Dispatch(timedSlot, ticket++, 123) == 2 && !timed.IsDisposed && BunnyHooks.TransientRetries == 2);
        BunnyHooks.RemoveSlot(timedSlot);
        Check(timed.IsDisposed);
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
            var typedBusy = typed.HookInt32("Terraria.Player", "Int32Transform", (_, _, _) =>
                throw new BunnyAccessException(BunnyAccessStatus.Busy, "rpc"));
            typedSlot = test_slot();
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 10, out ok, out value);
            Check(ok == 2 && !typedBusy.IsDisposed && BunnyHooks.TransientRetries == 3);
            typedBusy.Dispose();
            var replace = typed.HookInt32("Terraria.Player", "Int32Transform", (_, _, value) => value * 3);
            typedSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeInt32(typedSlot, ticket++, 123, 9, out ok, out value);
            Check(ok == 1 && value == 27 && test_originals() == 0);
            replace.Dispose();
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
        using (var pair = new BunnyHookScope())
        {
            int seen = 0;
            BunnyOriginalInt32 expiredPair = default;
            var active = pair.HookInt32Pair("Terraria.Player", "PairTransform",
                (original, self, first, second) =>
                {
                    Check(self == (nint)123 && first == 4 && second == 9);
                    expiredPair = original;
                    seen++;
                    return original.Invoke() + second * 2;
                });
            int pairSlot = test_slot();
            test_begin(ticket);
            test_int_argument(4);
            BunnyHooks.InvokeInt32Pair(pairSlot, ticket++, 123, 4, 9, out int ok, out int output);
            Check(ok == 1 && output == 29 && test_originals() == 1 && seen == 1);
            Throws(() => expiredPair.Invoke());
            active.Dispose();
            var replacement = pair.HookInt32Pair("Terraria.Player", "PairTransform",
                (_, _, first, second) => first - second);
            pairSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeInt32Pair(pairSlot, ticket++, 123, 4, 9, out ok, out output);
            Check(ok == 1 && output == -5 && test_originals() == 0);
            replacement.Dispose();
            var busyPair = pair.HookInt32Pair("Terraria.Player", "PairTransform",
                (_, _, _, _) => throw new BunnyAccessException(BunnyAccessStatus.Busy, "rpc"));
            pairSlot = test_slot();
            BunnyHooks.InvokeInt32Pair(pairSlot, ticket++, 123, 4, 9, out ok, out output);
            Check(ok == 2 && !busyPair.IsDisposed);
            busyPair.Dispose();
            var failingPair = pair.HookInt32Pair("Terraria.Player", "PairTransform",
                (original, _, _, _) =>
                {
                    original.Invoke();
                    original.Invoke();
                    return 0;
                });
            pairSlot = test_slot();
            test_begin(ticket);
            test_int_argument(4);
            BunnyHooks.InvokeInt32Pair(pairSlot, ticket++, 123, 4, 9, out ok, out output);
            Check(ok == 0 && test_originals() == 1 && !failingPair.IsDisposed);
            BunnyHooks.RemoveSlot(pairSlot);
            Check(failingPair.IsDisposed);
        }

        using (var extended = new BunnyHookScope())
        {
            BunnyOriginalInt32 expiredZero = default;
            var zero = extended.HookInt32Zero("Terraria.Utilities.UnifiedRandom", "Next", (original, self) =>
            {
                Check(self == (nint)123);
                expiredZero = original;
                return original.Invoke() + 2;
            });
            int extraSlot = test_slot();
            test_begin(ticket);
            test_int_argument(7);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 0, 0, 0, 1, out int ok, out int output);
            Check(ok == 1 && output == 16 && test_originals() == 1);
            Throws(() => expiredZero.Invoke());
            zero.Dispose();
            Check(zero.IsDisposed);
            var single = extended.HookVoidInt32("Terraria.WorldGen", "QuickFindHome", (original, self, first) =>
            {
                Check(self == (nint)123 && first == 5);
                original.Invoke();
            }, isStatic: true);
            extraSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 5, 0, 0, 2, out ok, out output);
            Check(ok == 1 && test_originals() == 1);
            single.Dispose();
            var dual = extended.HookVoidInt32Pair("Terraria.WorldGen", "PlantCactus", (original, self, first, second) =>
            {
                Check(self == (nint)123 && first == 5 && second == 8);
                original.Invoke();
            }, isStatic: true);
            extraSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 5, 8, 0, 3, out ok, out output);
            Check(ok == 1 && test_originals() == 1);
            dual.Dispose();
            var triple = extended.HookInt32Triple("Terraria.WorldGen", "countWires", (original, self, first, second, third) =>
            {
                Check(self == (nint)123 && first == 5 && second == 8 && third == 3);
                return original.Invoke() + first;
            }, isStatic: true);
            extraSlot = test_slot();
            test_begin(ticket);
            test_int_argument(10);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 5, 8, 3, 4, out ok, out output);
            Check(ok == 1 && output == 22 && test_originals() == 1);
            triple.Dispose();
            var voidTriple = extended.HookVoidInt32Triple("Terraria.WorldGen", "ConvertTile", (original, self, first, second, third) =>
            {
                Check(self == (nint)123 && first == 5 && second == 8 && third == 3);
                original.Invoke();
            }, isStatic: true);
            extraSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 5, 8, 3, 5, out ok, out output);
            Check(ok == 1 && test_originals() == 1);
            voidTriple.Dispose();
            var busyExtended = extended.HookVoidInt32("Terraria.WorldGen", "QuickFindHome",
                (_, _, _) => throw new BunnyAccessException(BunnyAccessStatus.TimedOut, "rpc"), isStatic: true);
            extraSlot = test_slot();
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 3, 0, 0, 2, out ok, out output);
            Check(ok == 2 && !busyExtended.IsDisposed);
            busyExtended.Dispose();
            var replace = extended.HookInt32Triple("Terraria.WorldGen", "countWires",
                (_, _, first, second, third) => first * second * third, isStatic: true);
            extraSlot = test_slot();
            test_begin(ticket);
            BunnyHooks.InvokeExtended(extraSlot, ticket++, 123, 5, 8, 3, 4, out ok, out output);
            Check(ok == 1 && output == 120 && test_originals() == 0);
            replace.Dispose();
        }
        Check(BunnyHooks.GetStatus().Contains("void()"));
        Console.WriteLine("C# SDK: delegate/original, deferred terminal disposal, stale timeout recovery, expired original, module auto-unload, slot reuse PASS");
    }
}
