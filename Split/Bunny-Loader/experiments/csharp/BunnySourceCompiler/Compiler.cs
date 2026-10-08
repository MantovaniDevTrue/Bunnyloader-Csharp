using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace BunnyLoader.SourceCompiler;

public static class Compiler
{
    private const int MaxFiles = 32;
    private const int MaxFileBytes = 256 * 1024;
    private const int MaxTotalBytes = 2 * 1024 * 1024;
    private const string CacheVersion = "roslyn-net8-source-v5-compact-references";
    private static readonly string[] ReferenceNames =
    {
        "System.Private.CoreLib.dll",
        "System.Runtime.dll",
        "netstandard.dll",
        "System.Collections.dll",
        "System.Collections.Concurrent.dll",
        "System.Linq.dll",
        "System.Console.dll",
        "System.Runtime.Extensions.dll",
        "System.ObjectModel.dll",
        "System.Threading.dll",
        "System.Threading.Tasks.dll",
        "System.Numerics.Vectors.dll",
        "System.ComponentModel.Primitives.dll",
        "BunnySourceSdk.dll"
    };

    public static int Compile(
        string modRoot, string outputFile, string runtimeDir, string diagnosticsFile)
    {
        try
        {
            Stage(diagnosticsFile, "compiler-entered");
            return CompileCore(modRoot, outputFile, runtimeDir, diagnosticsFile);
        }
        catch (Exception exception)
        {
            WriteDiagnostic(diagnosticsFile,
                "COMPILER_EXCEPTION " + exception.ToString());
            return -1;
        }
    }

    private static int CompileCore(
        string modRoot, string outputFile, string runtimeDir, string diagnosticsFile)
    {
        Stage(diagnosticsFile, "normalizing-paths");
        string content = Path.GetFullPath(Path.Combine(modRoot, "content"));
        string root = Path.GetFullPath(modRoot);
        if (!content.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.Ordinal) ||
            !Directory.Exists(content))
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_DIRECTORY_INVALID");
            return -2;
        }

        Stage(diagnosticsFile, "enumerating-sources");
        var files = Directory.EnumerateFiles(content, "*.cs", SearchOption.AllDirectories)
            .OrderBy(path => path, StringComparer.Ordinal).ToArray();
        if (files.Length == 0 || files.Length > MaxFiles)
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_FILE_COUNT_INVALID " + files.Length);
            return -3;
        }

        Stage(diagnosticsFile, "sources-found-" + files.Length);
        var trees = new List<SyntaxTree>(files.Length);
        Stage(diagnosticsFile, "initializing-portable-sha256");
        var fingerprint = new PortableSha256();
        Stage(diagnosticsFile, "portable-sha256-ready");
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
            Stage(diagnosticsFile, "parsing-source-" + trees.Count);
            trees.Add(CSharpSyntaxTree.ParseText(
                Encoding.UTF8.GetString(bytes),
                CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.CSharp12),
                relative));
        }

        Stage(diagnosticsFile, "source-trees-created");
        string sdkFile = Path.Combine(runtimeDir, "BunnySourceSdk.dll");
        if (!File.Exists(sdkFile))
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_SDK_MISSING");
            return -6;
        }
        fingerprint.AppendData(File.ReadAllBytes(sdkFile));
        Stage(diagnosticsFile, "calculating-fingerprint");
        string hash = Convert.ToHexString(fingerprint.GetHashAndReset());
        string cache = outputFile + ".sha256";
        if (File.Exists(outputFile) && new FileInfo(outputFile).Length > 0 &&
            File.Exists(cache) && File.ReadAllText(cache) == hash)
        {
            WriteDiagnostic(diagnosticsFile, "CACHE_HIT " + hash);
            return 2;
        }

        Stage(diagnosticsFile, "listing-framework-assemblies");
        string[] framework = ReferenceNames.Select(name => Path.Combine(runtimeDir, name))
            .Where(File.Exists).ToArray();
        if (!framework.Contains(sdkFile) ||
            !framework.Contains(Path.Combine(runtimeDir, "System.Private.CoreLib.dll")) ||
            !framework.Contains(Path.Combine(runtimeDir, "System.Runtime.dll")))
        {
            WriteDiagnostic(diagnosticsFile, "FRAMEWORK_REFERENCES_INCOMPLETE " +
                framework.Length);
            return -7;
        }

        Stage(diagnosticsFile, "creating-metadata-references-" + framework.Length);
        var references = framework.Select(path => MetadataReference.CreateFromFile(path)).ToArray();
        Stage(diagnosticsFile, "metadata-references-created-" + references.Length);
        var compilation = CSharpCompilation.Create(
            assemblyName: "BunnySource_" + hash.Substring(0, 16),
            syntaxTrees: trees,
            references: references,
            options: new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary,
                optimizationLevel: OptimizationLevel.Release,
                deterministic: false,
                concurrentBuild: false,
                allowUnsafe: false));

        Stage(diagnosticsFile, "compilation-created");
        Stage(diagnosticsFile, "declaration-diagnostics-start");
        string[] declarationErrors = compilation.GetDeclarationDiagnostics()
            .Where(item => item.Severity == DiagnosticSeverity.Error)
            .Take(60).Select(item => item.ToString()).ToArray();
        Stage(diagnosticsFile, "declaration-diagnostics-complete-" + declarationErrors.Length);
        if (declarationErrors.Length > 0)
        {
            WriteDiagnostic(diagnosticsFile, "COMPILE_DECLARATION_FAILED\n" +
                string.Join("\n", declarationErrors));
            return -8;
        }
        Stage(diagnosticsFile, "methodbody-diagnostics-start");
        string[] bodyErrors = compilation.GetMethodBodyDiagnostics()
            .Where(item => item.Severity == DiagnosticSeverity.Error)
            .Take(60).Select(item => item.ToString()).ToArray();
        Stage(diagnosticsFile, "methodbody-diagnostics-complete-" + bodyErrors.Length);
        if (bodyErrors.Length > 0)
        {
            WriteDiagnostic(diagnosticsFile, "COMPILE_BODY_FAILED\n" +
                string.Join("\n", bodyErrors));
            return -8;
        }
        Directory.CreateDirectory(Path.GetDirectoryName(outputFile)!);
        string temporary = outputFile + ".tmp";
        using var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write,
            FileShare.None);
        Stage(diagnosticsFile, "before-emit");
        Stage(diagnosticsFile, "emit-compact-references-start");
        long emitStarted = Environment.TickCount64;
        var result = compilation.Emit(stream);
        Stage(diagnosticsFile, "after-emit");
        Stage(diagnosticsFile, "emit-duration-ms-" +
            (Environment.TickCount64 - emitStarted));
        Stage(diagnosticsFile, "before-flush");
        stream.Flush(true);
        Stage(diagnosticsFile, "after-flush");
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

        Stage(diagnosticsFile, "before-assembly-install");
        File.Move(temporary, outputFile, true);
        File.WriteAllText(cache, hash);
        WriteDiagnostic(diagnosticsFile, "COMPILE_OK files=" + files.Length +
            " references=" + references.Length + " hash=" + hash +
            " bytes=" + new FileInfo(outputFile).Length);
        return 1;
    }

    private static void Stage(string path, string stage)
    {
        WriteDiagnostic(path, "ROSLYN_STAGE=" + stage);
    }

    private static void WriteDiagnostic(string path, string message)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, message.Length > 12000 ?
            message.Substring(0, 12000) : message);
    }
}
