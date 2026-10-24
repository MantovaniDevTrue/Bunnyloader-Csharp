using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

public static class BunnyDustSourceDispatcher
{
    public const int Accepted = 1;
    public const int VanillaUpdate = 2;
    public const int KeepHorizontalSpeed = 4;
    public const int OverrideAlpha = 8;
    public const int Draw = 16;
    public const int Active = 32;

    private sealed class Context
    {
        public required string ModId;
        public required string AssemblyPath;
        public required BunnyDustRegistry Registry;
        public BunnyModAssemblyLease? Lease;
        public readonly int[] Types = new int[BunnyDustRegistry.MaxDustSlots];
        public readonly int[] Generations = new int[BunnyDustRegistry.MaxDustSlots];
    }

    private static readonly List<Context> Contexts = new(16);
    public static string LastRegistrationError { get; private set; } = "";

    internal static string ModForAssembly(Assembly assembly)
    {
        string path = assembly.Location;
        if (path.Length == 0 && BunnyModAssemblyRegistry.TryResolve(assembly, out _, out string resolved))
            path = resolved;
        foreach (Context context in Contexts)
            if (context.AssemblyPath == path) return context.ModId;
        if (BunnyEntitySourceDispatcher.TryRegisterDustAssembly(assembly))
            foreach (Context context in Contexts)
                if (context.AssemblyPath == path) return context.ModId;
        throw new InvalidOperationException("BunnyDust assembly has not been registered in MonoVM: " +
            path + " RegistrationError=" + LastRegistrationError);
    }

    public static int Register(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(modId) || string.IsNullOrWhiteSpace(assemblyPath))
            return -1;
        BunnyModAssemblyLease? lease = null;
        try
        {
            lease = BunnyModAssemblyRegistry.Acquire(modId, assemblyPath);
            int slot = RegisterLoaded(modId, lease.Assembly);
            if (slot >= 0 && Contexts[slot].Lease == null)
            {
                Contexts[slot].Lease = lease;
                lease = null;
            }
            return slot;
        }
        catch (Exception error)
        {
            LastRegistrationError = error.GetType().Name + ": " + error.Message;
            return -1;
        }
        finally { lease?.Dispose(); }
    }

    internal static int RegisterLoaded(string modId, Assembly assembly)
    {
        string assemblyPath = assembly.Location;
        if (assemblyPath.Length == 0 && BunnyModAssemblyRegistry.TryResolve(assembly, out _, out string resolved))
            assemblyPath = resolved;
        if (string.IsNullOrWhiteSpace(modId) || string.IsNullOrWhiteSpace(assemblyPath))
            return -1;
        for (int i = 0; i < Contexts.Count; i++)
            if (Contexts[i].ModId == modId)
                return Contexts[i].AssemblyPath == assemblyPath ? i : -1;
        if (Contexts.Count >= 16)
            return -1;
        try
        {
            var registry = new BunnyDustRegistry(modId);
            foreach (Type type in assembly.GetTypes().OrderBy(type => type.FullName, StringComparer.Ordinal))
            {
                if (!type.IsClass || type.IsAbstract || type.ContainsGenericParameters ||
                    !typeof(BunnyDust).IsAssignableFrom(type))
                    continue;
                BunnyDust instance = (BunnyDust)(Activator.CreateInstance(type) ??
                    throw new InvalidOperationException("Cannot instantiate " + type.FullName));
                registry.Register(type.Name, instance);
            }
            if (registry.RegisteredCount == 0)
                throw new InvalidOperationException("No BunnyDust content in source assembly");
            var context = new Context {
                ModId = modId,
                AssemblyPath = assemblyPath,
                Registry = registry
            };
            Array.Fill(context.Types, -1);
            Contexts.Add(context);
            LastRegistrationError = "";
            return Contexts.Count - 1;
        }
        catch (Exception error)
        {
            LastRegistrationError = error.GetType().Name + ": " + error.Message;
            return -1;
        }
    }

    public static bool Bind(int modSlot, int nativeType, string className)
    {
        if ((uint)modSlot >= (uint)Contexts.Count)
            return false;
        return Contexts[modSlot].Registry.Bind(nativeType, className);
    }

    public const int Spawn = 0;
    public const int Update = 1;
    public const int MidUpdate = 2;
    public const int Alpha = 3;
    public const int PreDraw = 4;
    public const int Release = 5;

    public static unsafe int Invoke(int modSlot, int phase, IntPtr statePointer)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || statePointer == IntPtr.Zero ||
            (uint)phase > Release)
            return 0;
        ref BunnyDustState state = ref *(BunnyDustState*)statePointer.ToPointer();
        if ((uint)state.Slot >= BunnyDustRegistry.MaxDustSlots ||
            state.Generation <= 0 || state.Type < BunnyDustRegistry.VanillaDustCount ||
            state.Type >= 32768 ||
            (state.Active != 0 && state.Active != 1))
            return 0;
        Context context = Contexts[modSlot];
        int slot = state.Slot;
        if (phase == Release)
        {
            context.Registry.Release(slot, state.Type, state.Generation);
            if (context.Types[slot] == state.Type &&
                context.Generations[slot] == state.Generation)
            {
                context.Types[slot] = -1;
                context.Generations[slot] = 0;
            }
            return Accepted;
        }

        if (state.Active == 0)
            return 0;

        bool fresh = context.Types[slot] != state.Type ||
            context.Generations[slot] != state.Generation;
        if (fresh)
        {
            bool spawned = context.Registry.OnSpawn(ref state);
            if (state.Active != 0)
            {
                context.Types[slot] = state.Type;
                context.Generations[slot] = state.Generation;
            }
            if (!spawned)
                return Accepted | (phase == Spawn ? 0 : VanillaUpdate | Draw) |
                    (state.Active != 0 ? Active : 0);
        }
        if (phase == Spawn)
            return Accepted | (state.Active != 0 ? Active : 0);

        bool vanilla = true;
        bool keep = false;
        bool draw = true;
        bool alpha = false;
        switch (phase)
        {
            case Update:
                context.Registry.Update(ref state, out vanilla);
                break;
            case MidUpdate:
                context.Registry.MidUpdate(ref state, out keep);
                break;
            case Alpha:
                uint color = state.Color;
                alpha = context.Registry.GetAlpha(ref state, ref color);
                state.Color = color;
                break;
            case PreDraw:
                context.Registry.PreDraw(ref state, out draw);
                break;
        }
        if (state.Active == 0)
        {
            context.Types[slot] = -1;
            context.Generations[slot] = 0;
        }
        return Accepted |
            (vanilla ? VanillaUpdate : 0) |
            (keep ? KeepHorizontalSpeed : 0) |
            (alpha ? OverrideAlpha : 0) |
            (draw ? Draw : 0) |
            (state.Active != 0 ? Active : 0);
    }

    public static unsafe int Tick(int modSlot, IntPtr statePointer)
    {
        int flags = Invoke(modSlot, Spawn, statePointer);
        if ((flags & Active) == 0)
            return flags;
        flags = Invoke(modSlot, Update, statePointer);
        if ((flags & Active) == 0)
            return flags;
        int combined = flags;
        flags = Invoke(modSlot, MidUpdate, statePointer);
        combined = (combined & ~KeepHorizontalSpeed) | (flags & KeepHorizontalSpeed);
        if ((flags & Active) == 0)
            return combined & ~Active;
        flags = Invoke(modSlot, Alpha, statePointer);
        combined = (combined & ~OverrideAlpha) | (flags & OverrideAlpha);
        if ((flags & Active) == 0)
            return combined & ~Active;
        flags = Invoke(modSlot, PreDraw, statePointer);
        return (combined & ~Draw & ~Active) | (flags & (Draw | Active));
    }

    public static void ResetWorld()
    {
        foreach (Context context in Contexts)
        {
            context.Registry.ResetWorld();
            Array.Fill(context.Types, -1);
            Array.Clear(context.Generations);
        }
    }

    public static void Unload()
    {
        foreach (Context context in Contexts)
        {
            context.Registry.Unload();
            context.Lease?.Dispose();
        }
        Contexts.Clear();
        LastRegistrationError = "";
    }
}
