using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// MonoVM worker entrypoint. Operations: 0=load, 1=unload one mod, 2=shutdown all.
/// The native host calls only from its persistent Mono owner, before gameplay activation.
/// No world-enter/exit hooks are inferred from these operations.
/// </summary>
public static class BunnyContentHostDispatcher
{
    private sealed record Registration(string Path, string Hash, BunnyContentMod Mod);
    private static readonly Dictionary<string, Registration> Registered = new(StringComparer.Ordinal);
    private static readonly Dictionary<string, string> SeenPaths = new(StringComparer.Ordinal);
    private static int ownerThread;
    private static bool closing, dispatching;
    public static string LastError { get; private set; } = "";

    public static int Dispatch(int operation, string modId, string assemblyPath)
    {
        lock (ContentCatalog.Gate)
        {
            // Mono thread identity is stable for the pinned owner; reject accidental game-thread calls.
            int thread = Environment.CurrentManagedThreadId;
            if (ownerThread != 0 && ownerThread != thread) return -1;
            ownerThread = thread;
            if (dispatching) { LastError = "Recursive managed host dispatch"; return -1; }
            dispatching = true;
            LastError = "";
            try
            {
                if (operation == 2)
                {
                    closing = true;
                    try { BunnyContentLoader.UnloadAll(); }
                    finally { Registered.Clear(); LoaderManager.Clear(); }
                    return 0;
                }
                ContentCatalog.ValidateSegment(modId, nameof(modId));
                if (operation == 1)
                {
                    if (!Registered.TryGetValue(modId, out var removed)) return 0;
                    Exception? failure = null;
                    try { BunnyContentLoader.Unload(modId); } catch (Exception error) { failure = error; }
                    Registered.Remove(modId);
                    try { ReleaseUnusedAssembly(removed.Mod.Assembly); }
                    catch (Exception error) { failure = failure == null ? error : new AggregateException(failure, error); }
                    if (failure != null) throw failure;
                    return 0;
                }
                if (operation != 0) throw new ArgumentOutOfRangeException(nameof(operation));
                if (closing) throw new InvalidOperationException("Managed content host is shutting down.");
                string path = Path.GetFullPath(assemblyPath);
                var file = new FileInfo(path);
                if (!file.Exists || file.Length == 0 || file.Length > 32 * 1024 * 1024)
                    throw new InvalidOperationException("Missing or oversized managed assembly.");
                var fingerprint = new ContentAssemblyHash();
                using (var stream = File.OpenRead(path))
                {
                    byte[] buffer = new byte[64 * 1024];
                    int count;
                    while ((count = stream.Read(buffer, 0, buffer.Length)) != 0)
                        fingerprint.AppendData(buffer, 0, count);
                }
                string hash = Convert.ToHexString(fingerprint.GetHashAndReset());
                if (Registered.TryGetValue(modId, out var previous))
                {
                    if (previous.Path != path || previous.Hash != hash || !previous.Mod.IsLoaded)
                        throw new InvalidOperationException("Assembly changed while loaded; restart the host before replacing it.");
                    return previous.Mod.GetContent<ModType>().Count;
                }
                if (SeenPaths.TryGetValue(path, out var seenHash) && seenHash != hash)
                    throw new InvalidOperationException("Previously loaded assembly changed; restart required.");
                // LoadFrom shares the assembly with existing NPC/player dispatchers. We do not instantiate
                // their BunnyNPC/BunnyItem types here; only the new ModType category participates.
                var assembly = System.Reflection.Assembly.LoadFrom(path);
                // Loader constructors are discovered automatically before ModType.Register needs them.
                SeenPaths[path] = hash;
                try
                {
                    LoaderManager.AutoLoad(assembly);
                    var mod = BunnyContentLoader.Load(modId, assembly);
                    BiomeConversionLoader.Publish(mod);
                    Registered.Add(modId, new Registration(path, hash, mod));
                    return mod.GetContent<ModType>().Count;
                }
                catch (Exception loadError)
                {
                    try { if (BunnyContentLoader.IsLoaded(modId)) BunnyContentLoader.Unload(modId); }
                    catch (Exception cleanupError) { loadError = new AggregateException(loadError, cleanupError); }
                    try { ReleaseUnusedAssembly(assembly); }
                    catch (Exception cleanupError) { throw new AggregateException(loadError, cleanupError); }
                    throw loadError;
                }
            }
            catch (Exception error)
            {
                LastError = error.GetType().Name + ": " + error.Message;
                return -1;
            }
            finally { dispatching = false; }
        }
    }
    private static void ReleaseUnusedAssembly(Assembly assembly)
    {
        if (!Registered.Values.Any(registration => registration.Mod.Assembly == assembly))
            LoaderManager.ReleaseAssembly(assembly);
    }
}
