using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;

namespace BunnyLoader.Source;

public delegate void BunnyVoidHook(BunnyOriginalVoid original, nint self);
public delegate int BunnyInt32Hook(BunnyOriginalInt32 original, nint self, int value);
public delegate int BunnyInt32PairHook(BunnyOriginalInt32 original, nint self, int first, int second);
public delegate int BunnyInt32ZeroHook(BunnyOriginalInt32 original, nint self);
public delegate void BunnyVoidInt32Hook(BunnyOriginalVoid original, nint self, int first);
public delegate void BunnyVoidInt32PairHook(BunnyOriginalVoid original, nint self, int first, int second);
public delegate int BunnyInt32TripleHook(BunnyOriginalInt32 original, nint self, int first, int second, int third);
public delegate void BunnyVoidInt32TripleHook(BunnyOriginalVoid original, nint self, int first, int second, int third);

public readonly struct BunnyOriginalInt32
{
    private readonly int slot;
    private readonly long ticket;
    internal BunnyOriginalInt32(int slot, long ticket) { this.slot = slot; this.ticket = ticket; }
    public int Invoke()
    {
        if (!BunnyHooks.IsCurrent(slot, ticket) || BunnyHooks.NativeOriginalInt32(slot, ticket, out int result) == 0)
            throw new InvalidOperationException("Original Int32 expirado, repetido ou chamado fora do callback sincronizado");
        return result;
    }
}

public readonly struct BunnyOriginalVoid
{
    private readonly int slot;
    private readonly long ticket;
    internal BunnyOriginalVoid(int slot, long ticket) { this.slot = slot; this.ticket = ticket; }
    public void Invoke()
    {
        if (!BunnyHooks.IsCurrent(slot, ticket) || BunnyHooks.NativeOriginal(slot, ticket) == 0)
            throw new InvalidOperationException("Original expirado, repetido ou chamado fora do callback sincronizado");
    }
}

public sealed class BunnyMethodHook : IDisposable
{
    internal BunnyVoidHook? Callback;
    internal BunnyInt32Hook? Int32Callback;
    internal BunnyInt32PairHook? Int32PairCallback;
    internal BunnyInt32ZeroHook? Int32ZeroCallback;
    internal BunnyVoidInt32Hook? VoidInt32Callback;
    internal BunnyVoidInt32PairHook? VoidInt32PairCallback;
    internal BunnyInt32TripleHook? Int32TripleCallback;
    internal BunnyVoidInt32TripleHook? VoidInt32TripleCallback;
    internal int NativeHandle = -1;
    internal BunnyMethodHook(BunnyVoidHook callback) { Callback = callback; }
    internal BunnyMethodHook(BunnyInt32Hook callback) { Int32Callback = callback; }
    internal BunnyMethodHook(BunnyInt32PairHook callback) { Int32PairCallback = callback; }
    internal BunnyMethodHook(BunnyInt32ZeroHook callback) { Int32ZeroCallback = callback; }
    internal BunnyMethodHook(BunnyVoidInt32Hook callback) { VoidInt32Callback = callback; }
    internal BunnyMethodHook(BunnyVoidInt32PairHook callback) { VoidInt32PairCallback = callback; }
    internal BunnyMethodHook(BunnyInt32TripleHook callback) { Int32TripleCallback = callback; }
    internal BunnyMethodHook(BunnyVoidInt32TripleHook callback) { VoidInt32TripleCallback = callback; }
    public bool IsDisposed => Volatile.Read(ref Callback) is null &&
        Volatile.Read(ref Int32Callback) is null &&
        Volatile.Read(ref Int32PairCallback) is null &&
        Volatile.Read(ref Int32ZeroCallback) is null &&
        Volatile.Read(ref VoidInt32Callback) is null &&
        Volatile.Read(ref VoidInt32PairCallback) is null &&
        Volatile.Read(ref Int32TripleCallback) is null &&
        Volatile.Read(ref VoidInt32TripleCallback) is null;
    public void Dispose()
    {
        bool hadCallback = Interlocked.Exchange(ref Callback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref Int32Callback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref Int32PairCallback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref Int32ZeroCallback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref VoidInt32Callback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref VoidInt32PairCallback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref Int32TripleCallback, null) is not null;
        hadCallback |= Interlocked.Exchange(ref VoidInt32TripleCallback, null) is not null;
        if (hadCallback && NativeHandle >= 0) BunnyHooks.NativeRemove(NativeHandle);
    }
}

public sealed class BunnyHookScope : IDisposable
{
    private readonly object gate = new();
    private readonly List<BunnyMethodHook> handles = new();
    private bool disposed;
    private BunnyNativeScope? native;
    public BunnyNativeScope Native
    {
        get { lock(gate) { if(disposed)throw new ObjectDisposedException(nameof(BunnyHookScope));return native??=new BunnyNativeScope(); } }
    }
    public BunnyMethodHook HookVoid(string type, string method, BunnyVoidHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.Register(type, method, callback, isStatic);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookInt32(string type, string method, BunnyInt32Hook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterInt32(type, method, callback, isStatic);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookInt32Pair(string type, string method, BunnyInt32PairHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterInt32Pair(type, method, callback, isStatic);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookInt32Zero(string type, string method, BunnyInt32ZeroHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterExtended(type, method, callback, isStatic, 1);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookVoidInt32(string type, string method, BunnyVoidInt32Hook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterExtended(type, method, callback, isStatic, 2);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookVoidInt32Pair(string type, string method, BunnyVoidInt32PairHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterExtended(type, method, callback, isStatic, 3);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookInt32Triple(string type, string method, BunnyInt32TripleHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterExtended(type, method, callback, isStatic, 4);
            handles.Add(handle);
            return handle;
        }
    }
    public BunnyMethodHook HookVoidInt32Triple(string type, string method, BunnyVoidInt32TripleHook callback, bool isStatic = false)
    {
        lock (gate)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BunnyHookScope));
            BunnyMethodHook handle = BunnyHooks.RegisterExtended(type, method, callback, isStatic, 5);
            handles.Add(handle);
            return handle;
        }
    }
    public void Dispose()
    {
        lock (gate)
        {
            if (disposed) return;
            disposed = true;
            foreach (BunnyMethodHook handle in handles) handle.Dispose();
            handles.Clear();
            native?.Dispose();
            native=null;
        }
    }
}

public abstract class BunnyHookModule
{
    public abstract void Load(BunnyHookScope hooks);
    public virtual void Unload() { }
    public virtual string DiagnosticStatus() => "";
}

public static class BunnyHooks
{
    private static readonly object RegistrationLock = new();
    private static readonly BunnyMethodHook?[] Slots = new BunnyMethodHook?[64];
    [ThreadStatic] private static int currentSlot;
    [ThreadStatic] private static long currentTicket;
    [ThreadStatic] private static nint currentSelf;
    public static long Calls { get; private set; }
    public static long Faults { get; private set; }
    public static long TransientRetries { get; private set; }
    public static string LastError { get; private set; } = "";

    public static string GetStatus() => "Universal C# void()/int(int)/int(int,int) e mais 5 ABI: chamadas=" + NativeStat(0) +
        " concluidas=" + NativeStat(1) + " ocupadas=" + NativeStat(2) + " timeouts=" + NativeStat(3) +
        " falhasNativas=" + NativeStat(4) + " recuperados=" + NativeStat(5) +
        " atrasadosDescartados=" + NativeStat(6) + " suspensoes=" + NativeStat(7) + " retomadas=" + NativeStat(8) +
        " callbacks=" + Calls + " excecoes=" + Faults + " transitorias=" + TransientRetries + " ponteRecuperada=" + NativeStat(9) +
        (LastError.Length == 0 ? "" : " ultimo=" + LastError);

    public static BunnyMethodHook Hook(BunnyHookScope scope, string type, string method,
                                      BunnyVoidHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookVoid(type, method, callback, isStatic);
    }
    public static BunnyMethodHook HookInt32(BunnyHookScope scope, string type, string method,
                                           BunnyInt32Hook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookInt32(type, method, callback, isStatic);
    }

    public static BunnyMethodHook HookInt32Pair(BunnyHookScope scope, string type, string method,
                                               BunnyInt32PairHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookInt32Pair(type, method, callback, isStatic);
    }

    public static BunnyMethodHook HookInt32Zero(BunnyHookScope scope, string type, string method,
        BunnyInt32ZeroHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookInt32Zero(type, method, callback, isStatic);
    }
    public static BunnyMethodHook HookVoidInt32(BunnyHookScope scope, string type, string method,
        BunnyVoidInt32Hook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookVoidInt32(type, method, callback, isStatic);
    }
    public static BunnyMethodHook HookVoidInt32Pair(BunnyHookScope scope, string type, string method,
        BunnyVoidInt32PairHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookVoidInt32Pair(type, method, callback, isStatic);
    }
    public static BunnyMethodHook HookInt32Triple(BunnyHookScope scope, string type, string method,
        BunnyInt32TripleHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookInt32Triple(type, method, callback, isStatic);
    }
    public static BunnyMethodHook HookVoidInt32Triple(BunnyHookScope scope, string type, string method,
        BunnyVoidInt32TripleHook callback, bool isStatic = false)
    {
        if (scope is null) throw new ArgumentNullException(nameof(scope));
        return scope.HookVoidInt32Triple(type, method, callback, isStatic);
    }
    internal static BunnyMethodHook Register(string type, string method, BunnyVoidHook callback, bool isStatic)
    {
        if (string.IsNullOrEmpty(type) || !type.StartsWith("Terraria.", StringComparison.Ordinal) || type.Length > 256 || type.IndexOf('\0') >= 0)
            throw new ArgumentException("Classe Terraria invalida", nameof(type));
        if (string.IsNullOrWhiteSpace(method) || method.Length > 128 || method[0] == '.' || method.IndexOf('\0') >= 0)
            throw new ArgumentException("Metodo invalido", nameof(method));
        if (callback is null) throw new ArgumentNullException(nameof(callback));
        lock (RegistrationLock)
        {
            int slot = -1;
            for (int i = 0; i < Slots.Length; ++i)
                if (Slots[i] is null || Slots[i]!.IsDisposed) { slot = i; break; }
            if (slot < 0) throw new InvalidOperationException("Limite de 64 hooks ativos");
            var handle = new BunnyMethodHook(callback);
            Volatile.Write(ref Slots[slot], handle);
            try
            {
                int native = NativeInstall(type, method, slot, isStatic ? 1 : 0);
                if (native < 0) throw new NotSupportedException("Hook void() recusado pelo runtime (codigo " + native + ")");
                handle.NativeHandle = native;
                return handle;
            }
            catch
            {
                handle.Dispose();
                throw;
            }
        }
    }

    internal static BunnyMethodHook RegisterInt32(string type, string method, BunnyInt32Hook callback, bool isStatic)
    {
        if (string.IsNullOrEmpty(type) || !type.StartsWith("Terraria.", StringComparison.Ordinal) || type.Length > 256 || type.IndexOf('\0') >= 0)
            throw new ArgumentException("Classe Terraria invalida", nameof(type));
        if (string.IsNullOrWhiteSpace(method) || method.Length > 128 || method[0] == '.' || method.IndexOf('\0') >= 0)
            throw new ArgumentException("Metodo invalido", nameof(method));
        if (callback is null) throw new ArgumentNullException(nameof(callback));
        lock (RegistrationLock)
        {
            int slot = -1;
            for (int i = 0; i < Slots.Length; ++i)
                if (Slots[i] is null || Slots[i]!.IsDisposed) { slot = i; break; }
            if (slot < 0) throw new InvalidOperationException("Limite de 64 hooks ativos");
            var handle = new BunnyMethodHook(callback);
            Volatile.Write(ref Slots[slot], handle);
            try
            {
                int native = NativeInstallInt32(type, method, slot, isStatic ? 1 : 0);
                if (native < 0) throw new NotSupportedException("Hook int(int) recusado pelo runtime (codigo " + native + ")");
                handle.NativeHandle = native;
                return handle;
            }
            catch
            {
                handle.Dispose();
                throw;
            }
        }
    }

    internal static BunnyMethodHook RegisterInt32Pair(string type, string method, BunnyInt32PairHook callback, bool isStatic)
    {
        if (string.IsNullOrEmpty(type) || !type.StartsWith("Terraria.", StringComparison.Ordinal) || type.Length > 256 || type.IndexOf('\0') >= 0)
            throw new ArgumentException("Classe Terraria invalida", nameof(type));
        if (string.IsNullOrWhiteSpace(method) || method.Length > 128 || method[0] == '.' || method.IndexOf('\0') >= 0)
            throw new ArgumentException("Metodo invalido", nameof(method));
        if (callback is null) throw new ArgumentNullException(nameof(callback));
        lock (RegistrationLock)
        {
            int slot = -1;
            for (int i = 0; i < Slots.Length; ++i)
                if (Slots[i] is null || Slots[i]!.IsDisposed) { slot = i; break; }
            if (slot < 0) throw new InvalidOperationException("Limite de 64 hooks ativos");
            var handle = new BunnyMethodHook(callback);
            Volatile.Write(ref Slots[slot], handle);
            try
            {
                int native = NativeInstallInt32Pair(type, method, slot, isStatic ? 1 : 0);
                if (native < 0) throw new NotSupportedException("Hook int(int,int) recusado pelo runtime (codigo " + native + ")");
                handle.NativeHandle = native;
                return handle;
            }
            catch
            {
                handle.Dispose();
                throw;
            }
        }
    }

    internal static BunnyMethodHook RegisterExtended(string type, string method, Delegate callback, bool isStatic, int mode)
    {
        if (string.IsNullOrEmpty(type) || !type.StartsWith("Terraria.", StringComparison.Ordinal) || type.Length > 256 || type.IndexOf('\0') >= 0)
            throw new ArgumentException("Classe Terraria invalida", nameof(type));
        if (string.IsNullOrWhiteSpace(method) || method.Length > 128 || method[0] == '.' || method.IndexOf('\0') >= 0)
            throw new ArgumentException("Metodo invalido", nameof(method));
        if (callback is null) throw new ArgumentNullException(nameof(callback));
        BunnyMethodHook handle = callback switch
        {
            BunnyInt32ZeroHook c when mode == 1 => new BunnyMethodHook(c),
            BunnyVoidInt32Hook c when mode == 2 => new BunnyMethodHook(c),
            BunnyVoidInt32PairHook c when mode == 3 => new BunnyMethodHook(c),
            BunnyInt32TripleHook c when mode == 4 => new BunnyMethodHook(c),
            BunnyVoidInt32TripleHook c when mode == 5 => new BunnyMethodHook(c),
            _ => throw new ArgumentException("Callback incompativel com assinatura", nameof(callback))
        };
        lock (RegistrationLock)
        {
            int slot = -1;
            for (int i = 0; i < Slots.Length; ++i)
                if (Slots[i] is null || Slots[i]!.IsDisposed) { slot = i; break; }
            if (slot < 0) throw new InvalidOperationException("Limite de 64 hooks ativos");
            Volatile.Write(ref Slots[slot], handle);
            try
            {
                int native = NativeInstallExtended(type, method, slot, isStatic ? 1 : 0, mode);
                if (native < 0) throw new NotSupportedException("Hook tipado recusado pelo runtime (codigo " + native + ")");
                handle.NativeHandle = native;
                return handle;
            }
            catch
            {
                handle.Dispose();
                throw;
            }
        }
    }

    public static void RemoveSlot(int slot)
    {
        if (slot >= 0 && slot < Slots.Length) Volatile.Read(ref Slots[slot])?.Dispose();
    }

    internal static void GetNativeContext(out int slot,out long ticket,out nint self) { slot=currentSlot;ticket=currentTicket;self=currentSelf; }

    internal static bool IsCurrent(int slot, long ticket) => ticket != 0 && currentTicket == ticket && currentSlot == slot;

    public static void Invoke(int slot, long ticket, nint self, out int result) => result = Dispatch(slot, ticket, self);

    public static int Dispatch(int slot, long ticket, nint self)
    {
        if (slot < 0 || slot >= Slots.Length || ticket <= 0 || currentTicket != 0) return 0;
        BunnyMethodHook? handle = Volatile.Read(ref Slots[slot]);
        BunnyVoidHook? callback = handle is null ? null : Volatile.Read(ref handle.Callback);
        if (callback is null) return 0;
        currentSlot = slot;
        currentTicket = ticket;
        currentSelf = self;
        try
        {
            callback(new BunnyOriginalVoid(slot, ticket), self);
            Calls++;
            return 1;
        }
        catch (BunnyAccessException error) when (error.Status == BunnyAccessStatus.Busy || error.Status == BunnyAccessStatus.TimedOut)
        {
            TransientRetries++;
            LastError = error.GetType().Name + ": " + error.Message;
            return 2;
        }
        catch (Exception error)
        {
            Faults++;
            LastError = error.GetType().Name + ": " + error.Message;
            return 0;
        }
        finally { currentTicket = 0; currentSlot = -1; currentSelf = 0; }
    }

    public static void InvokeInt32(int slot, long ticket, nint self, int value, out int success, out int returnedValue)
    {
        success = 0;
        returnedValue = 0;
        if (slot < 0 || slot >= Slots.Length || ticket <= 0 || currentTicket != 0) return;
        BunnyMethodHook? handle = Volatile.Read(ref Slots[slot]);
        BunnyInt32Hook? callback = handle is null ? null : Volatile.Read(ref handle.Int32Callback);
        if (callback is null) return;
        currentSlot = slot;
        currentTicket = ticket;
        currentSelf = self;
        try
        {
            returnedValue = callback(new BunnyOriginalInt32(slot, ticket), self, value);
            Calls++;
            success = 1;
        }
        catch (BunnyAccessException error) when (error.Status == BunnyAccessStatus.Busy || error.Status == BunnyAccessStatus.TimedOut)
        {
            TransientRetries++;
            LastError = error.GetType().Name + ": " + error.Message;
            success = 2;
        }
        catch (Exception error)
        {
            Faults++;
            LastError = error.GetType().Name + ": " + error.Message;
        }
        finally { currentTicket = 0; currentSlot = -1; currentSelf = 0; }
    }

    public static void InvokeInt32Pair(int slot, long ticket, nint self, int first, int second, out int success, out int returnedValue)
    {
        success = 0;
        returnedValue = 0;
        if (slot < 0 || slot >= Slots.Length || ticket <= 0 || currentTicket != 0) return;
        BunnyMethodHook? handle = Volatile.Read(ref Slots[slot]);
        BunnyInt32PairHook? callback = handle is null ? null : Volatile.Read(ref handle.Int32PairCallback);
        if (callback is null) return;
        currentSlot = slot;
        currentTicket = ticket;
        currentSelf = self;
        try
        {
            returnedValue = callback(new BunnyOriginalInt32(slot, ticket), self, first, second);
            Calls++;
            success = 1;
        }
        catch (BunnyAccessException error) when (error.Status == BunnyAccessStatus.Busy || error.Status == BunnyAccessStatus.TimedOut)
        {
            TransientRetries++;
            LastError = error.GetType().Name + ": " + error.Message;
            success = 2;
        }
        catch (Exception error)
        {
            Faults++;
            LastError = error.GetType().Name + ": " + error.Message;
        }
        finally { currentTicket = 0; currentSlot = -1; currentSelf = 0; }
    }

    public static void InvokeExtended(int slot, long ticket, nint self, int first, int second, int third,
        int mode, out int success, out int returnedValue)
    {
        success = 0;
        returnedValue = 0;
        if (slot < 0 || slot >= Slots.Length || ticket <= 0 || currentTicket != 0 || mode < 1 || mode > 5) return;
        BunnyMethodHook? handle = Volatile.Read(ref Slots[slot]);
        if (handle is null) return;
        if ((mode == 1 && handle.Int32ZeroCallback is null) ||
            (mode == 2 && handle.VoidInt32Callback is null) ||
            (mode == 3 && handle.VoidInt32PairCallback is null) ||
            (mode == 4 && handle.Int32TripleCallback is null) ||
            (mode == 5 && handle.VoidInt32TripleCallback is null)) return;
        currentSlot = slot;
        currentTicket = ticket;
        currentSelf = self;
        try
        {
            switch (mode)
            {
                case 1: returnedValue = handle.Int32ZeroCallback!(new BunnyOriginalInt32(slot, ticket), self); break;
                case 2: handle.VoidInt32Callback!(new BunnyOriginalVoid(slot, ticket), self, first); break;
                case 3: handle.VoidInt32PairCallback!(new BunnyOriginalVoid(slot, ticket), self, first, second); break;
                case 4: returnedValue = handle.Int32TripleCallback!(new BunnyOriginalInt32(slot, ticket), self, first, second, third); break;
                case 5: handle.VoidInt32TripleCallback!(new BunnyOriginalVoid(slot, ticket), self, first, second, third); break;
            }
            Calls++;
            success = 1;
        }
        catch (BunnyAccessException error) when (error.Status == BunnyAccessStatus.Busy || error.Status == BunnyAccessStatus.TimedOut)
        {
            TransientRetries++;
            LastError = error.GetType().Name + ": " + error.Message;
            success = 2;
        }
        catch (Exception error)
        {
            Faults++;
            LastError = error.GetType().Name + ": " + error.Message;
        }
        finally { currentTicket = 0; currentSlot = -1; currentSelf = 0; }
    }

    [DllImport("bunny", EntryPoint = "bunny_universal_hook_stat", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeStat(int key);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_install", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeInstall([MarshalAs(UnmanagedType.LPUTF8Str)] string type,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string method, int slot, int isStatic);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_remove", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void NativeRemove(int handle);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_install_i32", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int NativeInstallInt32([MarshalAs(UnmanagedType.LPUTF8Str)] string type,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string method, int slot, int isStatic);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_install_i32_pair", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int NativeInstallInt32Pair([MarshalAs(UnmanagedType.LPUTF8Str)] string type,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string method, int slot, int isStatic);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_install_extended", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int NativeInstallExtended([MarshalAs(UnmanagedType.LPUTF8Str)] string type,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string method, int slot, int isStatic, int mode);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_original_i32", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int NativeOriginalInt32(int slot, long ticket, out int returnedValue);
    [DllImport("bunny", EntryPoint = "bunny_universal_hook_original", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int NativeOriginal(int slot, long ticket);
}

public static class BunnyUniversalHookDispatcher
{
    private sealed class Context
    {
        public string Id = "";
        public string Path = "";
        public readonly BunnyHookScope Scope = new();
        public readonly List<BunnyHookModule> Modules = new();
    }
    private static readonly List<Context> Contexts = new();
    public static string LastError { get; private set; } = "";

    public static int Register(string id, string path)
    {
        if (string.IsNullOrWhiteSpace(id) || string.IsNullOrWhiteSpace(path)) return -1;
        foreach (Context existing in Contexts)
            if (existing.Id == id) return existing.Path == path ? existing.Modules.Count : -1;
        if (Contexts.Count >= 16) return -1;
        var context = new Context { Id = id, Path = path };
        try
        {
            Type[] types = Assembly.LoadFrom(path).GetTypes().Where(type =>
                !type.IsAbstract && typeof(BunnyHookModule).IsAssignableFrom(type))
                .OrderBy(type => type.FullName, StringComparer.Ordinal).ToArray();
            foreach (Type type in types)
            {
                var module = (BunnyHookModule)Activator.CreateInstance(type)!;
                context.Modules.Add(module);
                module.Load(context.Scope);
            }
            Contexts.Add(context);
            return context.Modules.Count;
        }
        catch (Exception error)
        {
            LastError = id + ": " + error.GetBaseException().Message;
            Close(context);
            return -1;
        }
    }
    public static string GetStatusSnapshot(string id)
    {
        Context? context = Contexts.Find(entry => entry.Id == id);
        if (context is null) return "nao registrado";
        var statuses = new List<string>();
        foreach (BunnyHookModule module in context.Modules)
        {
            string label = module.GetType().Name;
            string result;
            try { result = module.DiagnosticStatus(); }
            catch (Exception error) { result = "FALHA:" + error.GetType().Name; }
            if (!string.IsNullOrEmpty(result)) statuses.Add(label + "=" + result);
        }
        return statuses.Count == 0 ? "modulos=" + context.Modules.Count : string.Join(" | ", statuses);
    }

    private static void Close(Context context)
    {
        try { BunnyWorldUpdateRegistry.DetachScope(context.Scope); }
        catch (Exception error) { LastError = context.Id + ": world teardown: " + error.Message; }
        context.Scope.Dispose();
        foreach (BunnyHookModule module in context.Modules)
            try { module.Unload(); }
            catch (Exception error) { LastError = context.Id + ": " + error.Message; }
        context.Modules.Clear();
    }
    public static void UnloadAll()
    {
        foreach (Context context in Contexts) Close(context);
        Contexts.Clear();
    }
}
