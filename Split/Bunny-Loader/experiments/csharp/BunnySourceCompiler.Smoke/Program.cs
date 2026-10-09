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
    if (CompiledAssemblyProbe.Verify(output, diag) != 4 ||
        !File.ReadAllText(diag).Contains("ASSEMBLY_EXEC_OK"))
        throw new InvalidOperationException("Compiled C# classes were not executed: " +
            File.ReadAllText(diag));
    string managedDefaults = File.ReadAllText(Path.Combine(scratch, "source-runtime-values.txt"));
    if (!managedDefaults.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("PROJECTILE\tSourceBolt\t10\t10\t1\t1\t360\t1\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("PROJECTILE\tSourceSpark\t14\t14\t1\t2\t240\t1\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("WEAPON\tSourceStaff\t30\t24\t10\tSourceSpark\n", StringComparison.Ordinal) ||
        !managedDefaults.Contains("WEAPON\tSourceWand\t22\t18\t13\tSourceBolt\n", StringComparison.Ordinal) ||
        managedDefaults.Split('\n', StringSplitOptions.RemoveEmptyEntries).Length != 5)
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
    string sparkFile = File.ReadAllText(Path.Combine(scratch, "source-spark-turns.txt"));
    string[] sparkLines = sparkFile.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (sparkLines.Length != 3 ||
        sparkLines[0] != "BUNNY_SOURCE_SPARK_TURNS_V1" ||
        sparkLines[1] != "SourceSpark")
        throw new InvalidOperationException("SourceSpark C# motion policy header invalid");
    string[] sparkSamples = sparkLines[2].Split('\t');
    if (sparkSamples.Length != 120 ||
        !Enumerable.Range(0, 120).All(i =>
            int.TryParse(sparkSamples[i], out var mrad) &&
            mrad == (int)MathF.Round((MathF.Sin(i * 0.32f) * 0.045f) * 1000f)) ||
        !sparkSamples.Any(x => int.TryParse(x, out int mrad) && mrad > 30) ||
        !sparkSamples.Any(x => int.TryParse(x, out int mrad) && mrad < -30))
        throw new InvalidOperationException("Independent C# SourceSpark AI curve failed validation");
    string genericCurves = File.ReadAllText(Path.Combine(scratch, "source-projectile-turns.txt"));
    string[] genericLines = genericCurves.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (genericLines.Length != 3 || genericLines[0] != "BUNNY_PROJECTILE_TURNS_V1")
        throw new InvalidOperationException("Generic projectile policy header or curve count invalid");
    var genericRecords = genericLines.Skip(1).Select(line => line.Split('\t')).ToArray();
    if (genericRecords[0].Length != 121 || genericRecords[1].Length != 121 ||
        genericRecords[0][0] != "SourceBolt" || genericRecords[1][0] != "SourceSpark")
        throw new InvalidOperationException("Generic projectile motion class mapping invalid");
    for (int tick = 0; tick < 120; tick++)
    {
        int curveBolt = (int)MathF.Round(MathF.Sin(tick * 0.22f) * 0.03f * 1000f);
        int curveSpark = (int)MathF.Round((MathF.Sin(tick * 0.32f) * 0.045f) * 1000f);
        if (!int.TryParse(genericRecords[0][tick + 1], out int actualBolt) ||
            !int.TryParse(genericRecords[1][tick + 1], out int actualSpark) ||
            actualBolt != curveBolt || actualSpark != curveSpark)
            throw new InvalidOperationException("Generic projectile curve values incorrect");
    }
    Console.WriteLine("BUNNY_CSHARP_GENERIC_PROJECTILE_POLICIES=PASS");
    Console.WriteLine("BUNNY_CSHARP_SPARK_AI_BUFFER=PASS");

    string homingFile = File.ReadAllText(Path.Combine(scratch, "source-homing-policy.txt"));
    string[] homingLines = homingFile.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (homingLines.Length != 4 ||
        homingLines[0] != "BUNNY_SOURCE_HOMING_V1" ||
        homingLines[1] != "SourceBolt" ||
        homingLines[2] != "480\t8")
        throw new InvalidOperationException("Invalid managed C# homing header");
    string[] homingSamples = homingLines[3].Split('\t');
    if (homingSamples.Length != 129 ||
        !homingSamples.All(t => int.TryParse(t, out int value) && Math.Abs(value) <= 80) ||
        homingSamples[64] != "0" ||
        int.Parse(homingSamples[0]) >= -10 ||
        int.Parse(homingSamples[128]) <= 10 ||
        !Enumerable.Range(1, 128).All(i =>
            int.Parse(homingSamples[i]) >= int.Parse(homingSamples[i - 1])))
        throw new InvalidOperationException("C# homing LUT invalid or not monotonic");
    string effectPolicy = File.ReadAllText(Path.Combine(scratch, "source-onhit-policy.txt"));
    string[] effectLines = effectPolicy.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (effectLines.Length != 4 ||
        effectLines[0] != "BUNNY_SOURCE_ONHIT_V2" ||
        effectLines[1] != "SourceBolt" ||
        effectLines[2] != "24")
        throw new InvalidOperationException("C# OnHitNPC exported an invalid header");
    string[] effectSamples = effectLines[3].Split('\t');
    if (effectSamples.Length != 256 ||
        !Enumerable.Range(0, 256).All(i =>
            int.TryParse(effectSamples[i], out int value) &&
            value == Math.Clamp(60 + i * 3, 60, 150)))
        throw new InvalidOperationException("C# OnHitNPC precise damage table invalid");
    string killPolicy = File.ReadAllText(Path.Combine(scratch, "source-onkill-policy.txt"));
    string[] killLines = killPolicy.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (killLines.Length != 5 ||
        killLines[0] != "BUNNY_SOURCE_ONKILL_V1" ||
        killLines[1] != "SourceBolt" || killLines[2] != "24")
        throw new InvalidOperationException("C# OnKill policy header invalid");
    string[] radiusSamples = killLines[3].Split('\t');
    string[] durationSamples = killLines[4].Split('\t');
    if (radiusSamples.Length != 256 || durationSamples.Length != 256 ||
        !Enumerable.Range(0, 256).All(i =>
            int.TryParse(radiusSamples[i], out int radius) &&
            radius == Math.Clamp(24 + i * 2, 24, 100) &&
            int.TryParse(durationSamples[i], out int duration) &&
            duration == Math.Clamp(60 + i * 2, 60, 160)))
        throw new InvalidOperationException("C# OnKill policy sample validation failed");
    string autoCombat = File.ReadAllText(Path.Combine(scratch, "source-combat-policies.txt"));
    string[] autoCombatRows = autoCombat.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (autoCombatRows.Length != 3 ||
        autoCombatRows[0] != "BUNNY_COMBAT_POLICIES_V1")
        throw new InvalidOperationException("Generic C# combat policy header invalid");
    var hitRow = autoCombatRows[1].Split('\t');
    var killRow = autoCombatRows[2].Split('\t');
    if (hitRow.Length != 259 || killRow.Length != 515 ||
        hitRow[0] != "HIT" || hitRow[1] != "SourceBolt" || hitRow[2] != "24" ||
        killRow[0] != "KILL" || killRow[1] != "SourceBolt" || killRow[2] != "24" ||
        !Enumerable.Range(0, 256).All(i =>
            hitRow[i + 3] == Math.Clamp(60 + i * 3, 60, 150).ToString() &&
            killRow[i + 3] == Math.Clamp(24 + i * 2, 24, 100).ToString() &&
            killRow[i + 259] == Math.Clamp(60 + i * 2, 60, 160).ToString()))
        throw new InvalidOperationException("Generic C# combat policy values incorrect");
    Console.WriteLine("BUNNY_CSHARP_AUTO_COMBAT_FIXTURE=PASS");
    Console.WriteLine("BUNNY_CSHARP_ONKILL_POLICY=PASS");
    Console.WriteLine("BUNNY_CSHARP_ONHIT_POLICY=PASS");
    Console.WriteLine("BUNNY_CSHARP_HOMING_POLICY=PASS");
    Console.WriteLine("BUNNY_CSHARP_AI_BUFFER=PASS");
    Console.WriteLine("BUNNY_CSHARP_MULTI_CONTENT=PASS");
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
    string independentRoot = Path.GetFullPath(Path.Combine(sourceRoot, "..", "ExampleModCSharp", "Playable"));
    string independentOut = Path.Combine(scratch, "IndependentExample.dll");
    string independentDiag = Path.Combine(scratch, "independent-diagnostics.txt");
    if (Compiler.Compile(independentRoot, independentOut, framework, independentDiag) != 1 ||
        CompiledAssemblyProbe.Verify(independentOut, independentDiag) != 6)
        throw new InvalidOperationException("Independent ExampleMod C# compile/verify failed: " +
            File.ReadAllText(independentDiag));
    string independentDefaults = File.ReadAllText(Path.Combine(scratch, "source-runtime-values.txt"));
    if (!independentDefaults.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("PROJECTILE\tExampleBolt\t12\t12\t1\t1\t240\t1\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("WEAPON\tExampleWand\t18\t20\t11\tExampleBolt\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("BUFF\tExampleDefenseBuff\t10\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("POTION\tExampleBuffPotion\t600\tExampleDefenseBuff\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("ACCESSORY\tExampleFocusAccessory\t0\n", StringComparison.Ordinal) ||
        !independentDefaults.Contains("PLAYER\tExampleFocusPlayer\tExampleFocusAccessory\t7\n", StringComparison.Ordinal) ||
        independentDefaults.Split('\n', StringSplitOptions.RemoveEmptyEntries).Length != 7)
        throw new InvalidOperationException("Independent ExampleMod defaults are incorrect");
    string independentCurves = File.ReadAllText(Path.Combine(scratch, "source-projectile-turns.txt"));
    string[] independentRows = independentCurves.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (independentRows.Length != 2 || independentRows[0] != "BUNNY_PROJECTILE_TURNS_V1")
        throw new InvalidOperationException("Independent ExampleMod curve header invalid");
    string[] independentSamples = independentRows[1].Split('\t');
    if (independentSamples.Length != 121 || independentSamples[0] != "ExampleBolt" ||
        !Enumerable.Range(0, 120).All(i =>
            int.TryParse(independentSamples[i + 1], out int value) &&
            value == (int)MathF.Round(MathF.Sin(i * 0.25f) * 0.025f * 1000f)))
        throw new InvalidOperationException("Independent ExampleMod C# curve is incorrect");
    string independentCombat = File.ReadAllText(Path.Combine(scratch, "source-combat-policies.txt"));
    string[] independentCombatRows = independentCombat.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (independentCombatRows.Length != 3 ||
        independentCombatRows[0] != "BUNNY_COMBAT_POLICIES_V1")
        throw new InvalidOperationException("Independent C# combat header invalid");
    string[] exampleHit = independentCombatRows[1].Split('\t');
    string[] exampleKill = independentCombatRows[2].Split('\t');
    if (exampleHit.Length != 259 || exampleKill.Length != 515 ||
        exampleHit[0] != "HIT" || exampleHit[1] != "ExampleBolt" ||
        exampleHit[2] != "24" ||
        exampleKill[0] != "KILL" || exampleKill[1] != "ExampleBolt" ||
        exampleKill[2] != "24" ||
        !Enumerable.Range(0, 256).All(i =>
            exampleHit[3 + i] == Math.Clamp(45 + i * 2, 45, 130).ToString() &&
            exampleKill[3 + i] == Math.Clamp(28 + i, 28, 96).ToString() &&
            exampleKill[259 + i] == Math.Clamp(50 + i * 2, 50, 150).ToString()))
        throw new InvalidOperationException("Independent OnHitNPC/OnKill C# tables incorrect");
    Console.WriteLine("BUNNY_CSHARP_MANAGED_PLAYER_ACCESSORY=PASS");
    Console.WriteLine("BUNNY_CSHARP_MANAGED_BUFF_POTION=PASS");
    Console.WriteLine("BUNNY_CSHARP_AUTO_COMBAT_INDEPENDENT=PASS");
    Console.WriteLine("BUNNY_CSHARP_INDEPENDENT_EXAMPLE=PASS");
    Console.WriteLine("BUNNY_ROSLYN_CACHE=PASS");
}
finally
{
    try { Directory.Delete(scratch, recursive: true); } catch { }
}
