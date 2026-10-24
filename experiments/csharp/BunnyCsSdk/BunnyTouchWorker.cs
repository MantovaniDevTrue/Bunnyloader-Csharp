using System;
using System.Collections.Generic;
using System.Reflection;

namespace BunnyLoader.Source;

internal static class BunnyTouchWorker
{
    private sealed class Registration
    {
        internal readonly BunnyModAssemblyLease Lease;
        internal readonly Action Process;
        internal int Failures;

        internal Registration(BunnyModAssemblyLease lease, Action process)
        {
            Lease = lease;
            Process = process;
        }
    }

    private static readonly object Gate = new();
    private static readonly Dictionary<string, Registration> Entries = new(StringComparer.Ordinal);

    public static int RegisterAssembly(string modId, string path)
    {
        lock (Gate)
        {
            if (Entries.TryGetValue(modId, out Registration? existing))
            {
                if (!string.Equals(existing.Lease.Path, System.IO.Path.GetFullPath(path),
                                   StringComparison.Ordinal))
                    throw new InvalidOperationException("Touch mod assembly changed: " + modId);
                return 1;
            }

            BunnyModAssemblyLease lease = BunnyModAssemblyRegistry.Acquire(modId, path);
            try
            {
                Type? type = lease.Assembly.GetType("BunnyLoader.Source.BunnyTouchEntry");
                if (type is null)
                {
                    lease.Dispose();
                    return 0;
                }
                const BindingFlags flags = BindingFlags.Public | BindingFlags.Static;
                MethodInfo? load = type.GetMethod("Load", flags, null, Type.EmptyTypes, null);
                MethodInfo? process = type.GetMethod("ProcessTriggers", flags, null, Type.EmptyTypes, null);
                if (load?.ReturnType != typeof(void) || process?.ReturnType != typeof(void))
                    throw new InvalidOperationException("BunnyTouchEntry requires void Load() and ProcessTriggers()");
                Action callback = (Action)Delegate.CreateDelegate(typeof(Action), process);
                load.Invoke(null, null);
                Entries.Add(modId, new Registration(lease, callback));
                return 1;
            }
            catch
            {
                lease.Dispose();
                throw;
            }
        }
    }

    public static int ProcessTriggers()
    {
        lock (Gate)
        {
            if (Entries.Count == 0) return 0;
            BunnyTouchButtonLoader.BeginTriggerFrame();
            try
            {
                int executed = 0;
                foreach (Registration entry in Entries.Values)
                {
                    if (entry.Failures >= 3) continue;
                    try
                    {
                        entry.Process();
                        executed++;
                    }
                    catch
                    {
                        entry.Failures++;
                    }
                }
                return executed;
            }
            finally
            {
                BunnyTouchButtonLoader.EndTriggerFrame();
            }
        }
    }

    public static void Clear()
    {
        lock (Gate)
        {
            foreach (var pair in Entries)
            {
                BunnyTouchButtonLoader.UnregisterMod(pair.Key);
                pair.Value.Lease.Dispose();
            }
            Entries.Clear();
        }
    }
}
