using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using BunnyLoader.SourceCompiler;

string work = Path.Combine(Path.GetTempPath(), "bunny-angler-host-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(work);
try
{
    string diagnostic = Path.Combine(work, "verify.log");
    string assembly = Assembly.GetExecutingAssembly().Location;
    int verified = CompiledAssemblyProbe.Verify(assembly, diagnostic);
    if (verified < 1)
        throw new InvalidOperationException("Fixture compilation or content verification failed: " +
            File.ReadAllText(diagnostic));
    string values = File.ReadAllText(Path.Combine(work, "source-runtime-values.txt"));
    if (!values.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !values.Contains("\nITEM\tExampleQuestFish\t", StringComparison.Ordinal))
        throw new InvalidOperationException("Primary quest fish item missing from V2 output");
    string[] lines = values.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    string[] fish = lines.Single(line => line.StartsWith("QUEST_FISH\t", StringComparison.Ordinal)).Split('\t');
    if (fish.Length != 7 || fish[1] != "ExampleQuestFish" || fish[2] != "0" || fish[3] != "1" || fish[6] != "0")
        throw new InvalidOperationException("Quest availability or identity does not match the original fish");
    string description = Encoding.UTF8.GetString(Convert.FromHexString(fish[4]));
    string location = Encoding.UTF8.GetString(Convert.FromHexString(fish[5]));
    if (!description.Contains("fish that swims upside-down", StringComparison.Ordinal) ||
        location != "Caught anywhere while standing upside-down.")
        throw new InvalidOperationException("Original Bunny fish dialogue did not survive compiler wire");
    string[] catchRow = lines.Single(line => line.StartsWith("QUEST_CATCH\t", StringComparison.Ordinal)).Split('\t');
    if (catchRow.Length != 4 || catchRow[1] != "ExampleFishingPlayer" ||
        catchRow[2] != "ExampleQuestFish" || catchRow[3] != "3")
        throw new InvalidOperationException("Independent BunnyPlayer fishing policy missing from compiled wire");
    Console.WriteLine("ANGLER_QUEST_COMPILED_ASSEMBLY_OK");
}
finally
{
    Directory.Delete(work, recursive: true);
}
