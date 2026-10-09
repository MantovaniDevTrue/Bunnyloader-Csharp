using BunnyLoader.SourceCompiler;
using System.Reflection;
using System.Runtime.Loader;

static void VerifyHash(string content, string expected)
{
    var hash = new PortableSha256();
    byte[] input = System.Text.Encoding.UTF8.GetBytes(content);
    int chunk = 1;
    int offset = 0;
    while (offset < input.Length)
    {
        int length = Math.Min(chunk, input.Length - offset);
        hash.AppendData(input, offset, length);
        offset += length;
        chunk = chunk >= 101 ? 1 : chunk + 1;
    }
    string result = Convert.ToHexString(hash.GetHashAndReset());
    if (!string.Equals(result, expected, StringComparison.OrdinalIgnoreCase))
        throw new InvalidOperationException("Portable SHA256 mismatch expected=" +
            expected + " actual=" + result);
    string sameAgain = Convert.ToHexString(hash.GetHashAndReset());
    if (sameAgain !=
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855")
        throw new InvalidOperationException("SHA256 reset failed");
}

VerifyHash("", "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
VerifyHash("abc", "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
VerifyHash(new string('a', 1000000),
    "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");
Console.WriteLine("BUNNY_PORTABLE_SHA256_VECTORS=PASS");

if (args.Length != 2) throw new ArgumentException("source-root framework-directory");
string sourceRoot = Path.GetFullPath(args[0]);
string framework = Path.GetFullPath(args[1]);
string scratch = Path.Combine(Path.GetTempPath(), "bunny-source-roslyn-smoke-" +
    Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(scratch);
try
{
    string output = Path.Combine(scratch, "CompiledSource.dll");
    string diag = Path.Combine(scratch, "diagnostics.txt");
    if (BunnyLoader.SourceCompiler.Preflight.Ping(diag) != 42 ||
        File.ReadAllText(diag) != "ROSLYN_PREFLIGHT=42")
        throw new InvalidOperationException("Mono preflight method contract failed");
    if (Compiler.Compile(sourceRoot, output, framework, diag) != 1)
        throw new InvalidOperationException(File.ReadAllText(diag));
    if (CompiledAssemblyProbe.Verify(output, diag) != 2 ||
        !File.ReadAllText(diag).Contains("ASSEMBLY_EXEC_OK"))
        throw new InvalidOperationException("Compiled C# classes were not executed: " +
            File.ReadAllText(diag));
    string managedDefaults = File.ReadAllText(Path.Combine(scratch, "source-runtime-values.txt"));
    if (!managedDefaults.StartsWith("BUNNY_MANAGED_DEFAULTS_V1\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("PROJECTILE\tSourceBolt\t10\t10\t1\t1\t360\t1\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("WEAPON\tSourceWand\t22\t18\t13\tSourceBolt\n", StringComparison.Ordinal))
        throw new InvalidOperationException("Unexpected managed Terraria defaults payload: " + managedDefaults);
    string motionBuffer = File.ReadAllText(Path.Combine(scratch, "source-ai-turns.txt"));
    string[] motionLines = motionBuffer.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (motionLines.Length != 3 ||
        motionLines[0] != "BUNNY_SOURCE_AI_TURNS_V1" ||
        motionLines[1] != "SourceBolt")
        throw new InvalidOperationException("Missing managed AI motion header");
    string[] samples = motionLines[2].Split('\t');
    if (samples.Length != 120 || samples[0] != "0" ||
        !samples.Any(x => int.TryParse(x, out var n) && n > 15) ||
        !samples.Any(x => int.TryParse(x, out var n) && n < -15) ||
        !samples.All(x => int.TryParse(x, out var n) && Math.Abs(n) <= 80))
        throw new InvalidOperationException("Invalid 120-tick motion buffer: " + motionLines[2]);
    Console.WriteLine("BUNNY_CSHARP_AI_BUFFER=PASS");
    Console.WriteLine("BUNNY_MANAGED_DEFAULTS_BRIDGE=PASS");
    Console.WriteLine("BUNNY_MANAGED_ASSEMBLY_EXECUTION=PASS");
    byte[] original = File.ReadAllBytes(output);
    if (original.Length < 1024) throw new InvalidOperationException("No assembly generated");
    if (Compiler.Compile(sourceRoot, output, framework, diag) != 2)
        throw new InvalidOperationException("Expected cache hit");

    var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(output);
    Type? bolt = assembly.GetTypes().FirstOrDefault(t => t.Name == "SourceBolt");
    if (bolt == null || bolt.BaseType?.FullName != "BunnyLoader.Source.BunnyProjectile")
        throw new InvalidOperationException("SourceBolt class not emitted");
    object projectile = Activator.CreateInstance(bolt)!;
    bolt.GetMethod("SetDefaults")!.Invoke(projectile, null);
    int width = (int)bolt.GetProperty("Width")!.GetValue(projectile)!;
    if (width != 10) throw new InvalidOperationException("SetDefaults did not execute");

    string copyRoot = Path.Combine(scratch, "mod");
    string copyContent = Path.Combine(copyRoot, "content");
    Directory.CreateDirectory(copyContent);
    foreach (string input in Directory.EnumerateFiles(Path.Combine(sourceRoot, "content"),
                 "*.cs", SearchOption.AllDirectories))
    {
        string relative = Path.GetRelativePath(Path.Combine(sourceRoot, "content"), input);
        string destination = Path.Combine(copyContent, relative);
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
        File.Copy(input, destination);
    }
    string main = Path.Combine(copyContent, "main.cs");
    string oldSource = File.ReadAllText(main);
    if (!oldSource.Contains("Width = 10;"))
        throw new InvalidOperationException("Expected SourceBolt defaults not found");
    File.WriteAllText(main, oldSource.Replace("Width = 10;", "Width = 11;"));
    if (Compiler.Compile(copyRoot, output, framework, diag) != 1)
        throw new InvalidOperationException("Source update did not trigger recompilation: " +
            File.ReadAllText(diag));
    if (original.SequenceEqual(File.ReadAllBytes(output)))
        throw new InvalidOperationException("Source update did not change compiled DLL");
    Console.WriteLine("BUNNY_ROSLYN_COMPILE=PASS");
    Console.WriteLine("BUNNY_ROSLYN_SOURCE_UPDATE=PASS");
    Console.WriteLine("BUNNY_ROSLYN_CACHE=PASS");
}
finally
{
    try { Directory.Delete(scratch, recursive: true); } catch { }
}
