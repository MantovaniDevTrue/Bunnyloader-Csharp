using BunnyLoader.SourceCompiler;
using System.Reflection;
using System.Runtime.Loader;

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
    if (Compiler.Compile(sourceRoot, output, framework, diag) != 1)
        throw new InvalidOperationException(File.ReadAllText(diag));
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
