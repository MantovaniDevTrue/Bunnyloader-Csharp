using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace BunnyLoader.SourceCompiler;

public static class Compiler
{
    private const int MaxFiles = 4096;
    private const int MaxFileBytes = 1024 * 1024;
    private const int MaxTotalBytes = 32 * 1024 * 1024;
    private const string CacheVersion = "roslyn-net8-source-v50-ready-prs-integration";
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
        long invocationStarted = Environment.TickCount64;
        try
        {
            Stage(diagnosticsFile, "compiler-entered");
            int result = CompileCore(modRoot, outputFile, runtimeDir, diagnosticsFile);
            if (result > 0 && File.Exists(diagnosticsFile))
                File.WriteAllText(diagnosticsFile + ".profile",
                    File.ReadAllText(diagnosticsFile).Trim().Replace('\n', ' ') +
                    " invocationMs=" + (Environment.TickCount64 - invocationStarted));
            return result;
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
        long compilationStarted = Environment.TickCount64;
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
            .Take(MaxFiles + 1).OrderBy(path => path, StringComparer.Ordinal).ToArray();
        if (files.Length == 0 || files.Length > MaxFiles)
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_FILE_COUNT_INVALID atual=" + files.Length +
                " permitido=1.." + MaxFiles + " pasta=content/; remova copias ou divida o pacote.");
            return -3;
        }

        Stage(diagnosticsFile, "sources-found-" + files.Length);
        var sources = new List<(string Path, byte[] Bytes)>(files.Length);
        Stage(diagnosticsFile, "initializing-portable-sha256");
        var fingerprint = new PortableSha256();
        Stage(diagnosticsFile, "portable-sha256-ready");
        AppendFingerprint(fingerprint, Encoding.UTF8.GetBytes(CacheVersion));
        AppendFingerprint(fingerprint, Encoding.UTF8.GetBytes(
            typeof(Compiler).Module.ModuleVersionId.ToString("D")));
        AppendFingerprint(fingerprint, Encoding.UTF8.GetBytes(
            typeof(CSharpCompilation).Module.ModuleVersionId.ToString("D")));
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
            if (new FileInfo(absolute).Length > MaxFileBytes)
            {
                WriteDiagnostic(diagnosticsFile, "SOURCE_TOO_LARGE " + path);
                return -5;
            }
            byte[] bytes = File.ReadAllBytes(absolute);
            totalBytes += bytes.Length;
            if (bytes.Length > MaxFileBytes || totalBytes > MaxTotalBytes)
            {
                WriteDiagnostic(diagnosticsFile, "SOURCE_TOO_LARGE " + path);
                return -5;
            }
            string relative = Path.GetRelativePath(content, absolute);
            AppendFingerprint(fingerprint, Encoding.UTF8.GetBytes(relative));
            AppendFingerprint(fingerprint, bytes);
            sources.Add((relative, bytes));
        }

        long sourcesMs = Environment.TickCount64 - compilationStarted;
        string sdkFile = Path.Combine(runtimeDir, "BunnySourceSdk.dll");
        if (!File.Exists(sdkFile))
        {
            WriteDiagnostic(diagnosticsFile, "SOURCE_SDK_MISSING");
            return -6;
        }
        Stage(diagnosticsFile, "listing-framework-assemblies");
        string referenceDir = Path.Combine(runtimeDir, "compiler-references");
        bool compactReferences = Directory.Exists(referenceDir);
        string[] framework = ReferenceNames
            .Where(name => !compactReferences || name != "System.Private.CoreLib.dll")
            .Select(name => Path.Combine(compactReferences ? referenceDir : runtimeDir, name))
            .Where(File.Exists).ToArray();
        int expectedReferences = ReferenceNames.Length - (compactReferences ? 1 : 0);
        if ((compactReferences && framework.Length != expectedReferences) ||
            (!compactReferences && (!framework.Contains(sdkFile) ||
                !framework.Contains(Path.Combine(runtimeDir, "System.Private.CoreLib.dll")) ||
                !framework.Contains(Path.Combine(runtimeDir, "System.Runtime.dll")))))
        {
            WriteDiagnostic(diagnosticsFile, "FRAMEWORK_REFERENCES_INCOMPLETE Expected=" +
                expectedReferences + " Actual=" + framework.Length + " Directory=" +
                (compactReferences ? referenceDir : runtimeDir));
            return -7;
        }
        var referenceSnapshot = new CompilerReferences(framework);
        AppendFingerprint(fingerprint, Encoding.UTF8.GetBytes(referenceSnapshot.Key));
        Stage(diagnosticsFile, "calculating-fingerprint");
        string hash = Convert.ToHexString(fingerprint.GetHashAndReset());
        string cache = outputFile + ".sha256";
        long fingerprintMs = Environment.TickCount64 - compilationStarted - sourcesMs;
        if (ValidCachedAssembly(outputFile, cache, hash))
        {
            WriteDiagnostic(diagnosticsFile, "CACHE_HIT " + hash +
                " parsed=0 integrity=verified sourcesMs=" + sourcesMs +
                " fingerprintMs=" + fingerprintMs +
                " totalMs=" + (Environment.TickCount64 - compilationStarted));
            return 2;
        }

        long parsingStarted = Environment.TickCount64;
        var trees = new List<SyntaxTree>(files.Length);
        for (int i = 0; i < sources.Count; ++i)
        {
            var source = sources[i];
            if ((trees.Count & 63) == 0)
                Stage(diagnosticsFile, "parsing-source-" + trees.Count);
            trees.Add(CSharpSyntaxTree.ParseText(
                Encoding.UTF8.GetString(source.Bytes),
                CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.CSharp12),
                source.Path));
            sources[i] = default;
        }
        sources.Clear();
        Stage(diagnosticsFile, "source-trees-created");

        long parsingMs = Environment.TickCount64 - parsingStarted;
        long metadataStarted = Environment.TickCount64;
        Stage(diagnosticsFile, "creating-metadata-references-" + framework.Length);
        var references = referenceSnapshot.Metadata(out bool metadataReused);
        long metadataMs = Environment.TickCount64 - metadataStarted;
        Stage(diagnosticsFile, "metadata-references-created-" + references.Length);
        bool extendedDiagnostics =
            Environment.GetEnvironmentVariable("BUNNY_ROSLYN_DIAGNOSTIC_PROBES") == "1";
        if (extendedDiagnostics)
        {
            int probeResult = RunMinimalProbes(runtimeDir, diagnosticsFile);
            if (probeResult != 0)
                return probeResult;
        }
        else
        {
            Stage(diagnosticsFile, "minimal-probes-skipped-low-memory");
        }
        Stage(diagnosticsFile, "main-compilation-create-start");
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
        if (extendedDiagnostics)
        {
            Stage(diagnosticsFile, "declaration-diagnostics-start");
            var declarationErrors = compilation.GetDeclarationDiagnostics()
                .Where(item => item.Severity == DiagnosticSeverity.Error)
                .ToArray();
            Stage(diagnosticsFile, "declaration-diagnostics-complete-" + declarationErrors.Length);
            if (declarationErrors.Length > 0)
            {
                WriteDiagnostic(diagnosticsFile, "COMPILE_DECLARATION_FAILED\n" +
                    SourceDiagnostics.Format(declarationErrors));
                return -8;
            }
            Stage(diagnosticsFile, "methodbody-diagnostics-start");
            var bodyErrors = compilation.GetMethodBodyDiagnostics()
                .Where(item => item.Severity == DiagnosticSeverity.Error)
                .ToArray();
            Stage(diagnosticsFile, "methodbody-diagnostics-complete-" + bodyErrors.Length);
            if (bodyErrors.Length > 0)
            {
                WriteDiagnostic(diagnosticsFile, "COMPILE_BODY_FAILED\n" +
                    SourceDiagnostics.Format(bodyErrors));
                return -8;
            }
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
        long emitMs = Environment.TickCount64 - emitStarted;
        Stage(diagnosticsFile, "emit-duration-ms-" + emitMs);
        Stage(diagnosticsFile, "before-flush");
        stream.Flush(true);
        Stage(diagnosticsFile, "after-flush");
        stream.Close();

        if (!result.Success)
        {
            File.Delete(temporary);
            WriteDiagnostic(diagnosticsFile,
                "COMPILE_FAILED\n" +
                SourceDiagnostics.Format(result.Diagnostics));
            return -8;
        }

        Stage(diagnosticsFile, "before-assembly-install");
        File.Move(temporary, outputFile, true);
        string cacheTemporary = cache + ".tmp";
        File.WriteAllText(cacheTemporary, "BUNNY_ASSEMBLY_CACHE_V1\n" + hash + "\n" +
            AssemblyHash(outputFile) + "\n");
        File.Move(cacheTemporary, cache, true);
        WriteDiagnostic(diagnosticsFile, "COMPILE_OK files=" + files.Length +
            " references=" + references.Length + " hash=" + hash +
            " bytes=" + new FileInfo(outputFile).Length + " parsed=" + trees.Count +
            " sourcesMs=" + sourcesMs + " fingerprintMs=" + fingerprintMs +
            " parsingMs=" + parsingMs + " referencesMs=" + metadataMs +
            " emitMs=" + emitMs + " referenceMode=" + (compactReferences ? "Compact" : "Runtime") +
            " referenceBytes=" + referenceSnapshot.Bytes + " metadataReused=" + (metadataReused ? 1 : 0) + " totalMs=" +
            (Environment.TickCount64 - compilationStarted));
        return 1;
    }

    private static void AppendFingerprint(PortableSha256 fingerprint, byte[] bytes)
    {
        int length = bytes.Length;
        fingerprint.AppendData(new[] { (byte)length, (byte)(length >> 8),
            (byte)(length >> 16), (byte)(length >> 24) });
        fingerprint.AppendData(bytes);
    }

    private static string AssemblyHash(string path)
    {
        var hash = new PortableSha256();
        using var file = File.OpenRead(path);
        byte[] buffer = new byte[64 * 1024];
        int count;
        while ((count = file.Read(buffer, 0, buffer.Length)) != 0)
            hash.AppendData(buffer, 0, count);
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private static bool ValidCachedAssembly(string output, string cache, string fingerprint)
    {
        if (!File.Exists(output) || !File.Exists(cache)) return false;
        if (new FileInfo(output).Length is <= 0 or > 8 * 1024 * 1024 ||
            new FileInfo(cache).Length > 256) return false;
        try
        {
            string[] lines = File.ReadAllLines(cache);
            return lines.Length == 3 && lines[0] == "BUNNY_ASSEMBLY_CACHE_V1" &&
                lines[1] == fingerprint && lines[2] == AssemblyHash(output);
        }
        catch (IOException) { return false; }
    }



    private static int ProbeCoreMetadata(string corelib, string diagnosticsFile)
    {
        Stage(diagnosticsFile, "core-metadata-open-start");
        using var file = File.OpenRead(corelib);
        using var reader = new PEReader(file, PEStreamOptions.LeaveOpen);
        Stage(diagnosticsFile, "core-metadata-pe-ready");
        var metadata = reader.GetMetadataReader();
        Stage(diagnosticsFile, "core-metadata-reader-ready");
        int count = metadata.TypeDefinitions.Count;
        Stage(diagnosticsFile, "core-metadata-types-" + count);
        if (count == 0)
        {
            WriteDiagnostic(diagnosticsFile, "CORE_METADATA_EMPTY");
            return -13;
        }
        var first = metadata.GetTypeDefinition(metadata.TypeDefinitions.First());
        string firstName = metadata.GetString(first.Name);
        Stage(diagnosticsFile, "core-metadata-first-type-" + firstName.Length);
        return 0;
    }

    private static int RunMinimalProbes(string runtimeDir, string diagnosticsFile)
    {
        string corelib = Path.Combine(runtimeDir, "System.Private.CoreLib.dll");
        string runtime = Path.Combine(runtimeDir, "System.Runtime.dll");
        string sdk = Path.Combine(runtimeDir, "BunnySourceSdk.dll");
        if (!File.Exists(corelib) || !File.Exists(runtime) || !File.Exists(sdk))
        {
            WriteDiagnostic(diagnosticsFile, "PROBE_REFERENCES_MISSING");
            return -9;
        }

        int metadataResult = ProbeCoreMetadata(corelib, diagnosticsFile);
        if (metadataResult != 0)
            return metadataResult;

        Stage(diagnosticsFile, "probe-core-references-start");
        MetadataReference[] coreReferences =
        {
            MetadataReference.CreateFromFile(corelib),
            MetadataReference.CreateFromFile(runtime)
        };
        Stage(diagnosticsFile, "probe-core-references-ready");
        int coreResult = RunProbe("core",
            "public static class BunnyCoreProbe { public static int Sum(int a, int b) => a + b; }",
            coreReferences, diagnosticsFile);
        if (coreResult != 0)
            return coreResult;

        Stage(diagnosticsFile, "probe-sdk-references-start");
        MetadataReference[] sdkReferences =
        {
            coreReferences[0],
            coreReferences[1],
            MetadataReference.CreateFromFile(sdk)
        };
        Stage(diagnosticsFile, "probe-sdk-references-ready");
        int sdkResult = RunProbe("sdk",
            "public sealed class BunnySdkProbe : BunnyLoader.Source.BunnyProjectile { " +
            "public override void SetDefaults() { Width = 10; Height = 10; } }",
            sdkReferences, diagnosticsFile);
        if (sdkResult != 0)
            return sdkResult;

        Stage(diagnosticsFile, "minimal-probes-complete");
        return 0;
    }

    private static int RunProbe(string name, string source,
        MetadataReference[] references, string diagnosticsFile)
    {
        Stage(diagnosticsFile, "probe-" + name + "-syntax-start");
        SyntaxTree tree = CSharpSyntaxTree.ParseText(
            source, CSharpParseOptions.Default.WithLanguageVersion(LanguageVersion.CSharp12));
        Stage(diagnosticsFile, "probe-" + name + "-create-start");
        var compilation = CSharpCompilation.Create(
            assemblyName: "Bunny_" + name + "_Probe",
            syntaxTrees: new[] { tree },
            references: references,
            options: new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary,
                optimizationLevel: OptimizationLevel.Release,
                deterministic: false,
                concurrentBuild: false,
                allowUnsafe: false));
        Stage(diagnosticsFile, "probe-" + name + "-specialtype-start");
        var objectSymbol = compilation.GetSpecialType(SpecialType.System_Object);
        Stage(diagnosticsFile, "probe-" + name + "-specialtype-" +
            (objectSymbol.TypeKind == TypeKind.Error ? "missing" : "ready"));
        Stage(diagnosticsFile, "probe-" + name + "-semantic-model-start");
        var model = compilation.GetSemanticModel(tree);
        Stage(diagnosticsFile, "probe-" + name + "-semantic-model-ready");
        Stage(diagnosticsFile, "probe-" + name + "-declarations-start");
        var declarationErrors = compilation.GetDeclarationDiagnostics()
            .Where(item => item.Severity == DiagnosticSeverity.Error)
            .Take(20).ToArray();
        Stage(diagnosticsFile, "probe-" + name + "-declarations-done-" + declarationErrors.Length);
        if (declarationErrors.Length > 0)
        {
            WriteDiagnostic(diagnosticsFile, "PROBE_DECLARATIONS_FAILED " + name + "\n" +
                string.Join("\n", declarationErrors.Select(item => item.ToString())));
            return -10;
        }

        Stage(diagnosticsFile, "probe-" + name + "-methods-start");
        var bodyErrors = compilation.GetMethodBodyDiagnostics()
            .Where(item => item.Severity == DiagnosticSeverity.Error)
            .Take(20).ToArray();
        Stage(diagnosticsFile, "probe-" + name + "-methods-done-" + bodyErrors.Length);
        if (bodyErrors.Length > 0)
        {
            WriteDiagnostic(diagnosticsFile, "PROBE_METHODS_FAILED " + name + "\n" +
                string.Join("\n", bodyErrors.Select(item => item.ToString())));
            return -11;
        }

        using var bytes = new MemoryStream();
        Stage(diagnosticsFile, "probe-" + name + "-emit-start");
        var result = compilation.Emit(bytes);
        Stage(diagnosticsFile, "probe-" + name + "-emit-done");
        if (!result.Success || bytes.Length == 0)
        {
            WriteDiagnostic(diagnosticsFile, "PROBE_EMIT_FAILED " + name + "\n" +
                string.Join("\n", result.Diagnostics
                    .Where(item => item.Severity == DiagnosticSeverity.Error)
                    .Take(20).Select(item => item.ToString())));
            return -12;
        }
        Stage(diagnosticsFile, "probe-" + name + "-complete");
        return 0;
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


