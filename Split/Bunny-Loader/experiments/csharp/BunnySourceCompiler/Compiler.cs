using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace BunnyLoader.SourceCompiler;

public static class Compiler
{
    private const int MaxFiles = 32;
    private const int MaxFileBytes = 256 * 1024;
    private const int MaxTotalBytes = 2 * 1024 * 1024;
    private const string CacheVersion = "roslyn-net8-source-v1";

    public static int Compile(
        string modRoot, string outputFile, string runtimeDir, string diagnosticsFile)
    {
        try
        {
            return CompileCore(modRoot, outputFile, runtimeDir, diagnosticsFile);
        }
        catch (Exception exception)
        {
            WriteDiagnostic(diagnosticsFile, "COMPILER_EXCEPTION " +
                exception.GetType().Name + ": " + exception.Message);
            return -1;
        }
    }

    private static int CompileCore(
        string modRoot, string outputFile, string runtimeDir, string diagnosticsFile)
    {
        string content = Path.GetFullPath(Path.Combine(modRoot, "content"));
        string root = Path.GetFullPath(modRoot);
        if (!content.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.Ordinal) ||
            !Directory.Exists(content))
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_DIRECTORY_INVALID");
            return -2;
        }

        var files = Directory.EnumerateFiles(content, "*.cs", SearchOption.AllDirectories)
            .OrderBy(path => path, StringComparer.Ordinal).ToArray();
        if (files.Length == 0 || files.Length > MaxFiles)
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_FILE_COUNT_INVALID " + files.Length);
            return -3;
        }

        var trees = new List<SyntaxTree>(files.Length);
        using var fingerprint = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        fingerprint.AppendData(Encoding.UTF8.GetBytes(CacheVersion));
        int totalBytes = 0;
        foreach (string path in files)
        {
            string absolute = Path.GetFullPath(path);
            if (!absolute.StartsWith(content + Path.DirectorySeparatorChar,
                    StringComparison.Ordinal) ||
                (File.GetAttributes(absolute) & FileAttributes.ReparsePoint) != 0)
            {
                WriteDiagnostic(diagnosticsFile, "SOURCE_PATH_REJECTED " + path);
                return -4;
            }
            byte[] bytes = File.ReadAllBytes(absolute);
            totalBytes += bytes.Length;
            if (bytes.Length > MaxFileBytes || totalBytes > MaxTotalBytes)
            {
                WriteDiagnostic(diagnosticsFile, "SOURCE_TOO_LARGE " + path);
                return -5;
            }
            string relative = Path.GetRelativePath(content, absolute);
            fingerprint.AppendData(Encoding.UTF8.GetBytes(relative));
            fingerprint.AppendData(bytes);
            trees.Add(CSharpSyntaxTree.ParseText(
                Encoding.UTF8.GetString(bytes),
                CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.CSharp12),
                relative));
        }

        string sdkFile = Path.Combine(runtimeDir, "BunnySourceSdk.dll");
        if (!File.Exists(sdkFile))
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_SDK_MISSING");
            return -6;
        }
        fingerprint.AppendData(File.ReadAllBytes(sdkFile));
        string hash = Convert.ToHexString(fingerprint.GetHashAndReset());
        string cache = outputFile + ".sha256";
        if (File.Exists(outputFile) && new FileInfo(outputFile).Length > 0 &&
            File.Exists(cache) && File.ReadAllText(cache) == hash)
        {
            WriteDiagnostic(diagnosticsFile, "CACHE_HIT " + hash);
            return 2;
        }

        string[] framework = Directory.EnumerateFiles(runtimeDir, "*.dll",
                SearchOption.TopDirectoryOnly)
            .Where(path => Path.GetFileName(path).StartsWith("System.", StringComparison.Ordinal) ||
                           Path.GetFileName(path) is "mscorlib.dll" or "netstandard.dll" or
                           "BunnySourceSdk.dll")
            .OrderBy(path => path, StringComparer.Ordinal).ToArray();
        if (framework.Length < 50 || !framework.Contains(sdkFile))
        {
            WriteDiagnostic(diagnosticsFile, "FRAMEWORK_REFERENCES_INCOMPLETE " +
                framework.Length);
            return -7;
        }

        var references = framework.Select(path => MetadataReference.CreateFromFile(path));
        var compilation = CSharpCompilation.Create(
            assemblyName: "BunnySource_" + hash.Substring(0, 16),
            syntaxTrees: trees,
            references: references,
            options: new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary,
                optimizationLevel: OptimizationLevel.Release,
                deterministic: true,
                concurrentBuild: true,
                allowUnsafe: false));

        Directory.CreateDirectory(Path.GetDirectoryName(outputFile)!);
        string temporary = outputFile + ".tmp";
        using var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write,
            FileShare.None);
        var result = compilation.Emit(stream);
        stream.Flush(true);
        stream.Close();

        if (!result.Success)
        {
            File.Delete(temporary);
            WriteDiagnostic(diagnosticsFile,
                "COMPILE_FAILED\n" +
                string.Join("\n", result.Diagnostics
                    .Where(item => item.Severity == DiagnosticSeverity.Error)
                    .Take(60)
                    .Select(item => item.ToString())));
            return -8;
        }

        File.Move(temporary, outputFile, true);
        File.WriteAllText(cache, hash);
        WriteDiagnostic(diagnosticsFile, "COMPILE_OK files=" + files.Length +
            " hash=" + hash + " bytes=" + new FileInfo(outputFile).Length);
        return 1;
    }

    private static void WriteDiagnostic(string path, string message)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, message.Length > 12000 ?
            message.Substring(0, 12000) : message);
    }
}
