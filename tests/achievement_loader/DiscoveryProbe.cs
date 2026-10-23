using System;
using System.IO;
using BunnyLoader.SourceCompiler;

if (args.Length != 1 || !File.Exists(args[0]))
    throw new ArgumentException("Provide compiled AchievementsDemo.dll");

string log = Path.Combine(Path.GetTempPath(),
    "achievement-probe-" + Guid.NewGuid().ToString("N") + ".txt");
try
{
    int result = CompiledAssemblyProbe.Verify(args[0], log);
    string diagnostics = File.ReadAllText(log);
    if (result != 1 || !diagnostics.Contains("ASSEMBLY_EXEC_OK classes=1", StringComparison.Ordinal)
        || !diagnostics.Contains("achievements=1", StringComparison.Ordinal))
        throw new InvalidOperationException(
            "Achievement-only assembly was rejected: " + result + "\n" + diagnostics);
    Console.WriteLine("BUNNY_ACHIEVEMENT_ONLY_PROBE=PASS");
}
finally
{
    if (File.Exists(log)) File.Delete(log);
}
