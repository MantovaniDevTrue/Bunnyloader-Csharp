using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using Microsoft.CodeAnalysis;

namespace BunnyLoader.SourceCompiler;

// One immutable metadata snapshot per resident compiler, never one cache per mod.
internal sealed class CompilerReferences
{
    private static readonly object Gate = new();
    private static string cachedKey = "";
    private static MetadataReference[]? cachedReferences;
    private readonly List<(string Path, byte[] Bytes)> images = new();
    internal string Key { get; private set; } = "";
    internal long Bytes { get; private set; }

    internal CompilerReferences(string[] paths)
    {
        var fingerprint = new PortableSha256();
        foreach (string path in paths)
        {
            var info = new FileInfo(path);
            if (!info.Exists || info.Length < 1 || info.Length > 64 * 1024 * 1024)
                throw new InvalidDataException("COMPILER_REFERENCE_SIZE_INVALID File=" + path);
            byte[] bytes = File.ReadAllBytes(path);
            if (bytes.Length != info.Length)
                throw new InvalidDataException("COMPILER_REFERENCE_CHANGED_DURING_READ File=" + path);
            var digest = new PortableSha256();
            digest.AppendData(bytes);
            Frame(fingerprint, Path.GetFileName(path));
            Frame(fingerprint, Convert.ToHexString(digest.GetHashAndReset()));
            images.Add((path, bytes));
            Bytes += bytes.Length;
        }
        Key = Convert.ToHexString(fingerprint.GetHashAndReset());
    }

    private static void Frame(PortableSha256 hash, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        int length = bytes.Length;
        hash.AppendData(new[] { (byte)length, (byte)(length >> 8),
            (byte)(length >> 16), (byte)(length >> 24) });
        hash.AppendData(bytes);
    }

    internal MetadataReference[] Metadata(out bool reused)
    {
        lock (Gate)
        {
            reused = cachedReferences != null && cachedKey == Key;
            if (reused) { images.Clear(); return cachedReferences!; }
            var references = new MetadataReference[images.Count];
            for (int i = 0; i < images.Count; i++)
                references[i] = MetadataReference.CreateFromImage(images[i].Bytes,
                    filePath: images[i].Path);
            cachedReferences = references;
            cachedKey = Key;
            images.Clear();
            return references;
        }
    }
}
