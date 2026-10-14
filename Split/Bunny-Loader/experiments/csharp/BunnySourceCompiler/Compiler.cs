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
    private const int MaxFiles = 512;
    private const int MaxFileBytes = 256 * 1024;
    private const int MaxTotalBytes = 16 * 1024 * 1024;
    private const string CacheVersion = "roslyn-net8-source-v15-native-recipe-keys";
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
            WriteDiagnostic(diagnosticsFile, "SOURCE_FILE_COUNT_INVALID atual=" + files.Length +
                " permitido=1.." + MaxFiles + " pasta=content/; remova copias ou divida o pacote.");
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
                SourceDiagnostics.Format(result.Diagnostics));
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


