using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Security.Cryptography;
using System.Threading;

[assembly: System.Runtime.CompilerServices.InternalsVisibleTo("Probe")]

namespace BunnyLoader.Source;

internal sealed class BunnyModAssemblyLease : IDisposable
{
    private BunnyModAssemblyRegistry.Entry? entry;

    internal BunnyModAssemblyLease(BunnyModAssemblyRegistry.Entry value) => entry = value;

    internal Assembly Assembly => Volatile.Read(ref entry)?.Assembly
        ?? throw new ObjectDisposedException(nameof(BunnyModAssemblyLease));

    internal string Path => Volatile.Read(ref entry)?.Path
        ?? throw new ObjectDisposedException(nameof(BunnyModAssemblyLease));

    public void Dispose()
    {
        BunnyModAssemblyRegistry.Entry? current = Interlocked.Exchange(ref entry, null);
        if (current != null) BunnyModAssemblyRegistry.Release(current);
    }
}

internal static class BunnyModAssemblyRegistry
{
    private sealed class ModContext : AssemblyLoadContext
    {
        internal ModContext(string id) : base("BunnyMod:" + id, isCollectible: true) { }

        protected override Assembly? Load(AssemblyName requested)
        {
            Assembly sdk = typeof(BunnyModAssemblyRegistry).Assembly;
            if (string.Equals(requested.Name, sdk.GetName().Name, StringComparison.Ordinal))
                return sdk;
            return Default.Assemblies.FirstOrDefault(candidate =>
                string.Equals(candidate.GetName().Name, requested.Name, StringComparison.Ordinal));
        }
    }

    internal sealed class Entry
    {
        internal readonly string Id;
        internal readonly string Path;
        internal readonly string Fingerprint;
        internal readonly ModContext Context;
        internal readonly Assembly Assembly;
        internal int References;

        internal Entry(string id, string path, string fingerprint, ModContext context, Assembly assembly)
        {
            Id = id;
            Path = path;
            Fingerprint = fingerprint;
            Context = context;
            Assembly = assembly;
            References = 1;
        }
    }

    private static readonly object Gate = new();
    private static readonly Dictionary<string, Entry> Entries = new(StringComparer.Ordinal);

    internal static int ActiveModCount
    {
        get { lock (Gate) return Entries.Count; }
    }

    internal static BunnyModAssemblyLease Acquire(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 128 ||
            string.IsNullOrWhiteSpace(assemblyPath) || assemblyPath.Length > 2048 ||
            !System.IO.Path.IsPathFullyQualified(assemblyPath) ||
            !assemblyPath.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Invalid CSharp mod assembly identity");

        string path = System.IO.Path.GetFullPath(assemblyPath);
        var info = new FileInfo(path);
        if (!info.Exists || info.Length == 0 || info.Length > 32 * 1024 * 1024)
            throw new InvalidDataException("CSharp mod assembly unavailable or too large");

        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length == 0 || bytes.Length > 32 * 1024 * 1024)
            throw new InvalidDataException("CSharp mod assembly size changed during read");
        string fingerprint = Convert.ToHexString(SHA256.HashData(bytes));

        lock (Gate)
        {
            if (Entries.TryGetValue(modId, out Entry? existing))
            {
                if (!string.Equals(existing.Path, path, StringComparison.Ordinal) ||
                    !string.Equals(existing.Fingerprint, fingerprint, StringComparison.Ordinal))
                    throw new InvalidOperationException("CSharp mod assembly changed while registered: " + modId);
                checked { existing.References++; }
                return new BunnyModAssemblyLease(existing);
            }

            var context = new ModContext(modId);
            try
            {
                using var input = new MemoryStream(bytes, writable: false);
                Assembly assembly = context.LoadFromStream(input);
                var entry = new Entry(modId, path, fingerprint, context, assembly);
                Entries.Add(modId, entry);
                return new BunnyModAssemblyLease(entry);
            }
            catch
            {
                context.Unload();
                throw;
            }
        }
    }

    internal static void Release(Entry entry)
    {
        bool unload = false;
        lock (Gate)
        {
            if (!Entries.TryGetValue(entry.Id, out Entry? current) ||
                !ReferenceEquals(current, entry))
                return;
            if (--entry.References == 0)
            {
                Entries.Remove(entry.Id);
                unload = true;
            }
        }
        if (unload) entry.Context.Unload();
    }
}
