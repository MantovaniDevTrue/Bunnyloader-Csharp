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

if (args.Length is not (2 or 3))
    throw new ArgumentException("source-root framework-directory [second-csharp-mod]");
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
    string minionRoot = Path.Combine(scratch, "native-minion");
    string minionContent = Path.Combine(minionRoot, "content");
    Directory.CreateDirectory(minionContent);
    File.WriteAllText(Path.Combine(minionContent, "Minion.cs"), """
using BunnyLoader.Source;
namespace NativeMinionSmoke;
public sealed class MinionProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Minion";
        Width = 16;
        Height = 16;
        AiStyle = 1;
        Friendly = true;
        Minion = true;
        MinionContactDamage = true;
        MinionSlots = 1f;
        Frames = 4;
        TileCollide = false;
        IgnoreWater = true;
        Penetrate = -1;
        TimeLeft = 1800;
    }
}
public sealed class MinionBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Native Minion Buff";
        MinionProjectile = nameof(MinionProjectile);
    }
}
public sealed class MinionStaff : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Native Summon Staff";
        Damage = 20;
        UseTime = 20;
        ShootSpeed = 10f;
        Shoot = nameof(MinionProjectile);
        Summon = true;
        MinionBuff = nameof(MinionBuff);
        MinionBuffTime = 2;
    }
}
""");
    string minionArtifacts = Path.Combine(scratch, "native-minion-artifacts");
    Directory.CreateDirectory(minionArtifacts);
    string minionOutput = Path.Combine(minionArtifacts, "NativeMinion.dll");
    string minionDiag = Path.Combine(minionArtifacts, "native-minion-diagnostics.txt");
    if (Compiler.Compile(minionRoot, minionOutput, framework, minionDiag) != 1 ||
        CompiledAssemblyProbe.Verify(minionOutput, minionDiag) != 3)
        throw new InvalidOperationException("Managed minion smoke failed: " + File.ReadAllText(minionDiag));
    string minionWire = File.ReadAllText(Path.Combine(minionArtifacts, "source-runtime-values.txt"));
    if (!minionWire.Contains("PROJECTILE\tMinionProjectile\t16\t16\t1\t-1\t1800\t1\t1\t1\t0\t1\t4\t6\t8\t700\t1\n", StringComparison.Ordinal) ||
        !minionWire.Contains("WEAPON\tMinionStaff\t20\t20\t10\tMinionProjectile\t1\tMinionBuff\t2\n", StringComparison.Ordinal) ||
        !minionWire.Contains("BUFF\tMinionBuff\t0\tMinionProjectile\t0\t-\t100\n", StringComparison.Ordinal))
        throw new InvalidOperationException("Native minion C# wire was not preserved: " + minionWire);
    Console.WriteLine("BUNNY_NATIVE_CSHARP_MINION_SMOKE=PASS");

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
    string consoleRoot = Path.Combine(scratch, "interactive");
    string consoleContent = Path.Combine(consoleRoot, "content");
    Directory.CreateDirectory(consoleContent);
    File.WriteAllText(Path.Combine(consoleContent, "ConsoleEntry.cs"),
        "using System;\nusing System.Text;\n" +
        "namespace BunnyLoader.Interactive {\n" +
        "public static class ConsoleEntry {\n" +
        "private static readonly StringBuilder Buffer = new StringBuilder();\n" +
        "public static void Print(object value) { Buffer.AppendLine(value == null ? \"null\" : value.ToString()); }\n" +
        "public static string Run() { Buffer.Clear(); Print(1 + 2); return Buffer.ToString(); }\n" +
        "}\n}\n");
    string consoleDll = Path.Combine(scratch, "InteractiveConsole.dll");
    string consoleDiag = Path.Combine(scratch, "console-diagnostics.txt");
    if (Compiler.Compile(consoleRoot, consoleDll, framework, consoleDiag) != 1 ||
        CompiledAssemblyProbe.Verify(consoleDll, consoleDiag) != 1)
        throw new InvalidOperationException("Interactive CSharp emit or verification failed: " +
            File.ReadAllText(consoleDiag));
    Assembly consoleAssembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(consoleDll);
    Type consoleType = consoleAssembly.GetType("BunnyLoader.Interactive.ConsoleEntry", true)!;
    string consoleResult = (string)consoleType.GetMethod("Run")!.Invoke(null, null)!;
    if (consoleResult.Trim() != "3")
        throw new InvalidOperationException("Interactive CSharp returned unexpected result: " + consoleResult);
    Console.WriteLine("BUNNY_INTERACTIVE_CSHARP_ROSLYN_EXEC=PASS");
    string independentRoot = Path.GetFullPath(Path.Combine(sourceRoot, "..", "ExampleModCSharp", "Playable"));
    string independentOut = Path.Combine(scratch, "IndependentExample.dll");
    string independentDiag = Path.Combine(scratch, "independent-diagnostics.txt");
    if (Compiler.Compile(independentRoot, independentOut, framework, independentDiag) != 1 ||
        CompiledAssemblyProbe.Verify(independentOut, independentDiag) != 59)
        throw new InvalidOperationException("Independent ExampleMod C# compile/verify failed: " +
            File.ReadAllText(independentDiag));
    string independentDefaults = File.ReadAllText(Path.Combine(scratch, "source-runtime-values.txt"));
    string exampleVersion = System.Text.Json.JsonDocument.Parse(
        File.ReadAllText(Path.Combine(independentRoot, "manifest.json")))
        .RootElement.GetProperty("version").GetString()!;
    string[] defaultRows = independentDefaults.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (defaultRows.Length != 76 || defaultRows[0] != "BUNNY_MANAGED_DEFAULTS_V2")
        throw new InvalidOperationException("Independent ExampleMod schema mismatch: Expected=76Rows,V2Header ActualRows=" +
            defaultRows.Length + " ActualHeader=" + defaultRows.FirstOrDefault());
    string[] expectedDefaults = {
        "PROJECTILE\tExampleBolt\t12\t12\t1\t1\t240\t1\n",
        "PROJECTILE\tExampleWaveBolt\t10\t10\t1\t2\t180\t1\n",
        "WEAPON\tExampleWand\t18\t20\t11\tExampleBolt\n",
        "WEAPON\tExampleWaveWand\t12\t16\t9\tExampleWaveBolt\n",
        "BUFF\tExampleDefenseBuff\t10\n",
        "POTION\tExampleBuffPotion\t600\tExampleDefenseBuff\n",
        "ACCESSORY\tExampleFocusAccessory\t0\tConcede +7 de defesa enquanto equipado\n",
        "ACCESSORY\tExampleSwiftAccessory\t0\tAumenta a velocidade de corrida em 8%\n",
        "ACCESSORY\tExampleMomentumAccessory\t0\tAumenta a aceleracao ao correr em 6%\n",
        "ACCESSORY\tExampleShadowAccessory\t0\tAtiva um efeito visual de sombra enquanto equipado\n",
        "PLAYER\tExampleFocusPlayer\tExampleFocusAccessory\t7\t0\t0\t0\t-\t-\t0\t0\t-\t0\t0\n",
        "PLAYER\tExampleSwiftPlayer\tExampleSwiftAccessory\t0\t8\t0\t0\t-\t-\t0\t0\t-\t0\t0\n",
        $"PLAYER\tExampleWelcomePlayer\t-\t0\t0\t0\t0\tC# Hook QA {exampleVersion}: consulte os estados H1-H5 no relatorio do Bunny Loader.\tExample C#: BunnyPlayer OnRespawn ativo!\t1\t1\tExample C#: BunnyPlayer UpdateDead ativo!\t0\t0\n",
        "PLAYER\tExampleMomentumPlayer\tExampleMomentumAccessory\t0\t0\t6\t0\t-\t-\t0\t0\t-\t0\t0\n",
        "PLAYER\tExampleShadowPlayer\tExampleShadowAccessory\t0\t0\t0\t1\t-\t-\t0\t0\t-\t0\t0\n",
        "ACCESSORY\tExampleVitalAccessory\t0\tRegenera +12 vida; reducao adicional contra NPC e projetil; esquiva no quarto golpe\n",
        "ACCESSORY\tExampleArcaneAccessory\t0\tAumenta a regeneracao de mana em 10 enquanto equipado\n",
        "PLAYER\tExampleVitalPlayer\tExampleVitalAccessory\t0\t0\t0\t0\t-\t-\t0\t0\t-\t12\t0\n",
        "PLAYER\tExampleArcanePlayer\tExampleArcaneAccessory\t0\t0\t0\t0\t-\t-\t0\t0\t-\t0\t10\n",
        "ACCESSORY\tExampleGuardianShieldAccessory\t0\t+5 defesa, +4 regeneracao de vida; reduz dano de NPCs e projeteis\t1\tContent/Items/Accessories/ExampleGuardianShieldAccessory_Shield.png\t",
        "ACCESSORY\tExampleTrailblazerBootsAccessory\t0\t+12% velocidade, +8% aceleracao\t3\tContent/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png\t",
        "ACCESSORY\tExampleMysticWardAccessory\t0\t+3 defesa, +8 regeneracao de mana; protecao com pouca vida e contra projeteis proximos\t2\tContent/Effects/MysticWardBarrier.png\t",
        "PLAYER\tExampleGuardianShieldPlayer\tExampleGuardianShieldAccessory\t5\t0\t0\t0\t-\t-\t0\t0\t-\t4\t0\n",
        "PLAYER\tExampleTrailblazerBootsPlayer\tExampleTrailblazerBootsAccessory\t0\t12\t8\t0\t-\t-\t0\t0\t-\t0\t0\n",
        "PLAYER\tExampleMysticWardPlayer\tExampleMysticWardAccessory\t3\t0\t0\t0\t-\t-\t0\t0\t-\t0\t8\n",
        "NPC\tExampleSlimeNPC\t32\t24\t7\t2\t25\t1\t1\t2\n",
        "PREFIX\tExampleMightyPrefix\t4\t5\t1.2",
        "PREFIX\tExampleGuardedPrefix\t5\t1\t1",
        "ITEM\tExampleWings\t",
        "ITEM\tExampleMeleeWeapon\t0\t0\t1\t50000\t2\t50\t6\t20\t20\t1\t",
        "ITEM\tExampleBullet\t0\t0\t9999\t5000\t2\t12\t",
        "ITEM\tExampleRocketLauncher\t0\t0\t1\t400000\t8\t55\t",
        "HOLDOUT\tExampleBulletLauncher\t-18\t0\n",
        "HOLDOUT\tExampleRocketLauncher\t-2\t-2\n",
        "NATIVE_ITEM\tExampleBullet\tammo:d:AmmoID:Bullet;ranged:b:1;shoot:p:ExampleBulletProjectile;shootSpeed:f:4.5\n",
        "NATIVE_PROJECTILE\tExampleBulletProjectile\textraUpdates:i:1;hostile:b:0;light:f:0.5;localNPCHitCooldown:i:-1;ranged:b:1;scale:f:1;usesLocalNPCImmunity:b:1\n",
    };
    foreach (string expectedDefault in expectedDefaults) {
        if (independentDefaults.Contains(expectedDefault, StringComparison.Ordinal)) continue;
        string identity = string.Join("\t", expectedDefault.Split('\t').Take(2));
        string actualDefault = defaultRows.FirstOrDefault(row => row.StartsWith(identity + "\t", StringComparison.Ordinal)) ?? "<missing>";
        throw new InvalidOperationException("Independent ExampleMod default mismatch: Content=" + identity +
            " Expected=" + expectedDefault.TrimEnd('\n') + " Actual=" + actualDefault);
    }
    string[] benchDefaults = defaultRows.Single(row => row.StartsWith("ITEM\tExampleWorkbench\t", StringComparison.Ordinal)).Split('\t');
    const string expectedBenchRecipes = "1,-1,v36:1,mExampleItem:10;1,-1,gWood:10,mExampleItem:10;1,-1,mExampleItem:10,gIronBar:2;1,-1,hExampleMaterials~416E79204578616D706C65206D6174657269616C~mExampleItem+mExampleSoul:10,gWood:10@7";
    if (benchDefaults[19] != expectedBenchRecipes)
        throw new InvalidOperationException("Independent ExampleWorkbench recipes mismatch: Expected=" + expectedBenchRecipes + " Actual=" + benchDefaults[19]);
    string independentCurves = File.ReadAllText(Path.Combine(scratch, "source-projectile-turns.txt"));
    string[] independentRows = independentCurves.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    if (independentRows.Length != 3 || independentRows[0] != "BUNNY_PROJECTILE_TURNS_V1")
        throw new InvalidOperationException("Independent ExampleMod curve header invalid");
    string[] independentSamples = independentRows[1].Split('\t');
    if (independentSamples.Length != 121 || independentSamples[0] != "ExampleBolt" ||
        !Enumerable.Range(0, 120).All(i =>
            int.TryParse(independentSamples[i + 1], out int value) &&
            value == (int)MathF.Round(MathF.Sin(i * 0.25f) * 0.025f * 1000f)))
        throw new InvalidOperationException("Independent ExampleMod C# curve is incorrect");
    string[] waveSamples = independentRows[2].Split('\t');
    if (waveSamples.Length != 121 || waveSamples[0] != "ExampleWaveBolt" ||
        !Enumerable.Range(0, 120).All(i =>
            int.TryParse(waveSamples[i + 1], out int value) &&
            value == (int)MathF.Round(MathF.Sin(i * 0.20f) * 0.018f * 1000f)))
        throw new InvalidOperationException("Independent ExampleWaveBolt C# curve is incorrect");
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
    var liveAssembly = Assembly.LoadFrom(independentOut);
    var vitalType = liveAssembly.GetType("ExampleModCSharp.ExampleVitalPlayer")
        ?? throw new InvalidOperationException("Example Vital C# player missing");
    var vital = Activator.CreateInstance(vitalType)
        ?? throw new InvalidOperationException("Example Vital C# player creation failed");
    var modifyHurt = vitalType.GetMethod("ModifyHurt")
        ?? throw new InvalidOperationException("Example Vital ModifyHurt missing");
    var onHurt = vitalType.GetMethod("OnHurt")
        ?? throw new InvalidOperationException("Example Vital OnHurt missing");
    var incoming = new BunnyLoader.Source.BunnyHurtModifiers {
        Damage = 80, HitDirection = 1, CurrentLife = 250,
        MaxLife = 500, PvP = false, Dodgeable = true
    };
    modifyHurt.Invoke(vital, new object[] { true, incoming });
    if (incoming.Damage != 60 || incoming.HitDirection != 1 || incoming.Cancelled)
        throw new InvalidOperationException("C# Vital ModifyHurt did not reduce equipped damage");
    var unbound = new BunnyLoader.Source.BunnyHurtModifiers { Damage = 80, PvP = false };
    modifyHurt.Invoke(vital, new object[] { false, unbound });
    var pvpHit = new BunnyLoader.Source.BunnyHurtModifiers { Damage = 80, PvP = true };
    modifyHurt.Invoke(vital, new object[] { true, pvpHit });
    if (unbound.Damage != 80 || pvpHit.Damage != 80)
        throw new InvalidOperationException("C# Vital ModifyHurt affected unequipped or PvP");
    var freeDodge = vitalType.GetMethod("FreeDodge")
        ?? throw new InvalidOperationException("Example Vital FreeDodge missing");
    for (int index = 1; index <= 4; index++)
    {
        var dodgeHit = new BunnyLoader.Source.BunnyHurtModifiers {
            Damage = 60, HitDirection = 1, CurrentLife = 250,
            MaxLife = 500, PvP = false, Dodgeable = true
        };
        bool dodged = (bool)(freeDodge.Invoke(vital, new object[] { true, dodgeHit }) ?? false);
        if (dodged != (index == 4))
            throw new InvalidOperationException("C# FreeDodge every fourth hit did not work");
    }
    foreach (var dodgeCase in new[] {
        new BunnyLoader.Source.BunnyHurtModifiers { Damage = 60, Dodgeable = false },
        new BunnyLoader.Source.BunnyHurtModifiers { Damage = 60, Dodgeable = true, PvP = true }
    })
    {
        if ((bool)(freeDodge.Invoke(vital, new object[] { true, dodgeCase }) ?? true))
            throw new InvalidOperationException("C# FreeDodge ignored PvP or Dodgeable gate");
    }
    if ((int)(vitalType.GetProperty("DodgeAttempts")?.GetValue(vital) ?? -1) != 4 ||
        (int)(vitalType.GetProperty("DodgesPerformed")?.GetValue(vital) ?? -1) != 1)
        throw new InvalidOperationException("C# FreeDodge managed state is incorrect");
    var playerTypes = liveAssembly.GetTypes()
        .Where(t => t.BaseType?.FullName == "BunnyLoader.Source.BunnyPlayer")
        .OrderBy(t => t.Name, StringComparer.Ordinal).ToArray();
    int vitalIndex = Array.FindIndex(playerTypes, t => t.Name == "ExampleVitalPlayer");
    if (vitalIndex < 0)
        throw new InvalidOperationException("Example Vital player missing in MonoVM order");
    int hurtSlot = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Register(
        "smoke-test-hurt", independentOut);
    if (hurtSlot < 0)
        throw new InvalidOperationException("C# managed hurt dispatcher registration failed");
    for (int hit = 1; hit <= 4; hit++)
    {
        long packed = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
            hurtSlot, 80, 1, 8, 250, 500, 1 << vitalIndex, -1, -1,
            -1, -1, -1, -1, -1, -1,
            -1f, -1f, -1f, -1f, -1f, -1f);
        if (packed < 0)
            throw new InvalidOperationException("C# managed hurt dispatcher returned failure");
        int damage = (int)(packed & 0xfffff);
        bool cancelled = ((packed >> 22) & 16) != 0;
        bool dodgeFlag = ((packed >> 22) & 32) != 0;
        if (damage != 60 || cancelled != (hit == 4) || dodgeFlag != (hit == 4))
            throw new InvalidOperationException("C# managed FreeDodge packed ABI differs");
        if (hit < 4 && BunnyLoader.Source.BunnyPlayerHurtDispatcher.After(
                hurtSlot, 50, 80, 1, 8, 200, 1 << vitalIndex, 7, -1,
                1, 23, 90, -1, -1, -1,
                100.5f, 200.25f, -2.5f, 3.75f, 1.25f, 64f) <= 0)
            throw new InvalidOperationException("C# managed OnHurt dispatch failed");
    }
    var testNpc = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
        hurtSlot, 80, 1, 0, 250, 500, 1 << vitalIndex, 7, -1,
        1, 23, 90, -1, -1, -1,
        100.5f, 200.25f, -2.5f, 3.75f, 1.25f, 64f);
    var testProjectile = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
        hurtSlot, 80, 1, 0, 250, 500, 1 << vitalIndex, -1, 31,
        -1, -1, -1, 9, 35, 255,
        300f, 400f, 5f, -6f, -0.5f, 128f);
    var testUnknown = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
        hurtSlot, 80, 1, 0, 250, 500, 1 << vitalIndex, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1f, -1f, -1f, -1f, -1f, -1f);
    var testUnequipped = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
        hurtSlot, 80, 1, 0, 250, 500, 0, 7, -1,
        1, 23, 90, -1, -1, -1,
        100.5f, 200.25f, -2.5f, 3.75f, 1.25f, 64f);
    var testPvp = BunnyLoader.Source.BunnyPlayerHurtDispatcher.Modify(
        hurtSlot, 80, 1, 1, 250, 500, 1 << vitalIndex, -1, 31,
        -1, -1, -1, 9, 35, 255,
        300f, 400f, 5f, -6f, -0.5f, 128f);
    if ((testNpc & 0xfffff) != 48 ||
        (testProjectile & 0xfffff) != 40 ||
        (testUnknown & 0xfffff) != 60 ||
        (testUnequipped & 0xfffff) != 80 ||
        (testPvp & 0xfffff) != 80)
        throw new InvalidOperationException("C# source-specific pre-hit reduction or equipment/PvP gating failed");

    var freshNpc = new BunnyLoader.Source.BunnyHurtModifiers {
        Damage = 80, NpcIndex = 7, NpcType = 1, NpcDamage = 23,
        NpcLife = 90,
        AttackerX = 100.5f, AttackerY = 200.25f,
        AttackerVelocityX = -2.5f, AttackerVelocityY = 3.75f,
        AttackerAngle = 1.25f, AttackerDistance = 64f
    };
    var freshProjectile = new BunnyLoader.Source.BunnyHurtModifiers {
        Damage = 80, ProjectileIndex = 31, ProjectileType = 9,
        ProjectileDamage = 35, ProjectileOwner = 255,
        AttackerX = 300f, AttackerY = 400f,
        AttackerVelocityX = 5f, AttackerVelocityY = -6f,
        AttackerAngle = -0.5f, AttackerDistance = 128f
    };
    var freshVital = Activator.CreateInstance(vitalType)
        ?? throw new InvalidOperationException("Vital attack context instance missing");
    vitalType.GetMethod("ModifyHitByNPC")?.Invoke(freshVital,
        new object[] { true, 7, freshNpc });
    vitalType.GetMethod("ModifyHitByProjectile")?.Invoke(freshVital,
        new object[] { true, 31, freshProjectile });
    if ((int)(vitalType.GetProperty("LastNpcType")?.GetValue(freshVital) ?? -1) != 1 ||
        (int)(vitalType.GetProperty("LastNpcDamage")?.GetValue(freshVital) ?? -1) != 23 ||
        (int)(vitalType.GetProperty("LastNpcLife")?.GetValue(freshVital) ?? -1) != 90 ||
        (int)(vitalType.GetProperty("LastProjectileType")?.GetValue(freshVital) ?? -1) != 9 ||
        (int)(vitalType.GetProperty("LastProjectileDamage")?.GetValue(freshVital) ?? -1) != 35 ||
        (int)(vitalType.GetProperty("LastProjectileOwner")?.GetValue(freshVital) ?? -1) != 255 ||
        (int)(vitalType.GetProperty("SlimePreHits")?.GetValue(freshVital) ?? 0) != 1 ||
        (int)(vitalType.GetProperty("EnemyProjectilePreHits")?.GetValue(freshVital) ?? 0) != 1)
        throw new InvalidOperationException("BunnyPlayer attacker context attributes not preserved");
    float spatial(string name) =>
        (float)(vitalType.GetProperty(name)?.GetValue(freshVital) ?? -10000f);
    if (spatial("LastAttackerX") != 300f ||
        spatial("LastAttackerY") != 400f ||
        spatial("LastAttackerVelocityX") != 5f ||
        spatial("LastAttackerVelocityY") != -6f ||
        spatial("LastAttackerAngle") != -0.5f ||
        spatial("LastAttackerDistance") != 128f)
        throw new InvalidOperationException("Spatial context not delivered to Vital Player");
    var postSpatial = new BunnyLoader.Source.BunnyHurtInfo(
        17, 30, 1, 300, false, false, 7, -1,
        1, 23, 90, -1, -1, -1,
        100.5f, 200.25f, -2.5f, 3.75f, 1.25f, 64f);
    onHurt.Invoke(freshVital, new object[] { true, postSpatial });
    if (spatial("LastPostHitDistance") != 64f ||
        postSpatial.AttackerX != 100.5f ||
        postSpatial.AttackerY != 200.25f ||
        postSpatial.AttackerVelocityX != -2.5f ||
        postSpatial.AttackerVelocityY != 3.75f ||
        postSpatial.AttackerAngle != 1.25f ||
        postSpatial.AttackerDistance != 64f)
        throw new InvalidOperationException("Six hurt spatial fields lost after damage");
    Console.WriteLine("BUNNY_CSHARP_HURT_SPATIAL_CONTEXT=PASS");

    var contextInfo = new BunnyLoader.Source.BunnyHurtInfo(
        17, 30, 1, 300, false, false, 7, -1,
        1, 23, 90, -1, -1, -1);
    if (contextInfo.NpcType != 1 || contextInfo.NpcDamage != 23 ||
        contextInfo.NpcLife != 90 || contextInfo.ProjectileType != -1 ||
        contextInfo.ProjectileDamage != -1 || contextInfo.ProjectileOwner != -1)
        throw new InvalidOperationException("BunnyHurtInfo snapshot metadata corrupted");
    Console.WriteLine("BUNNY_CSHARP_ATTACKER_CONTEXT_SIX_FIELDS=PASS");

    var directNpc = vitalType.GetMethod("ModifyHitByNPC")
        ?? throw new InvalidOperationException("Example Vital ModifyHitByNPC missing");
    var directProjectile = vitalType.GetMethod("ModifyHitByProjectile")
        ?? throw new InvalidOperationException("Example Vital ModifyHitByProjectile missing");
    var npcModifiers = new BunnyLoader.Source.BunnyHurtModifiers { Damage = 60, NpcIndex = 7 };
    directNpc.Invoke(vital, new object[] { true, 7, npcModifiers });
    var projectileModifiers = new BunnyLoader.Source.BunnyHurtModifiers {
        Damage = 60, NpcIndex = -1, ProjectileIndex = 31
    };
    directProjectile.Invoke(vital, new object[] { true, 31, projectileModifiers });
    if (npcModifiers.Damage != 48 || projectileModifiers.Damage != 40 ||
        (int)(vitalType.GetProperty("NpcPreHitCalls")?.GetValue(vital) ?? -1) != 1 ||
        (int)(vitalType.GetProperty("ProjectilePreHitCalls")?.GetValue(vital) ?? -1) != 1)
        throw new InvalidOperationException("C# source-aware pre-hit callback state failed");
    Console.WriteLine("BUNNY_CSHARP_PRE_HIT_NPC_PROJECTILE=PASS");

    var onNpc = vitalType.GetMethod("OnHitByNPC")
        ?? throw new InvalidOperationException("Example Vital OnHitByNPC missing");
    var onProjectile = vitalType.GetMethod("OnHitByProjectile")
        ?? throw new InvalidOperationException("Example Vital OnHitByProjectile missing");
    var fromNpc = new BunnyLoader.Source.BunnyHurtInfo(
        11, 16, 1, 240, false, false, 7, -1);
    var fromProjectile = new BunnyLoader.Source.BunnyHurtInfo(
        15, 25, -1, 225, false, false, -1, 31);
    if (fromNpc.NpcIndex != 7 || fromNpc.ProjectileIndex != -1 ||
        fromProjectile.NpcIndex != -1 || fromProjectile.ProjectileIndex != 31)
        throw new InvalidOperationException("BunnyHurtInfo source identity lost");
    onNpc.Invoke(vital, new object[] { true, 7, fromNpc });
    onNpc.Invoke(vital, new object[] { false, 9, fromNpc });
    onProjectile.Invoke(vital, new object[] { true, 31, fromProjectile });
    onProjectile.Invoke(vital, new object[] { false, 42, fromProjectile });
    if ((int)(vitalType.GetProperty("NpcHitsTaken")?.GetValue(vital) ?? -1) != 1 ||
        (int)(vitalType.GetProperty("ProjectileHitsTaken")?.GetValue(vital) ?? -1) != 1 ||
        (int)(vitalType.GetProperty("LastNpcIndex")?.GetValue(vital) ?? -1) != 7 ||
        (int)(vitalType.GetProperty("LastProjectileIndex")?.GetValue(vital) ?? -1) != 31)
        throw new InvalidOperationException("C# hit source callbacks lost state");
    if (BunnyLoader.Source.BunnyPlayerHurtDispatcher.After(
        hurtSlot, 10, 15, -1, 8, 190, 1 << vitalIndex, -1, 31,
        -1, -1, -1, 9, 35, 255,
        300f, 400f, 5f, -6f, -0.5f, 128f) <= 0)
        throw new InvalidOperationException("C# projectile hit source dispatch failed");
    if (BunnyLoader.Source.BunnyPlayerHurtDispatcher.After(
        hurtSlot, 10, 15, -1, 8, 180, 1 << vitalIndex, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1f, -1f, -1f, -1f, -1f, -1f) <= 0)
        throw new InvalidOperationException("C# generic hurt dispatch failed");
    Console.WriteLine("BUNNY_CSHARP_HIT_SOURCE_DISPATCH=PASS");
    onHurt.Invoke(vital, new object[] {
        true, new BunnyLoader.Source.BunnyHurtInfo(50, 80, 1, 200, false, false)
    });
    onHurt.Invoke(vital, new object[] {
        false, new BunnyLoader.Source.BunnyHurtInfo(20, 80, 1, 180, false, false)
    });
    if ((int)(vitalType.GetProperty("HitsTaken")?.GetValue(vital) ?? -1) != 1 ||
        (int)(vitalType.GetProperty("LastDamageTaken")?.GetValue(vital) ?? -1) != 50)
        throw new InvalidOperationException("C# Vital OnHurt did not retain callback state");
    var dodge = new BunnyLoader.Source.BunnyHurtModifiers { Damage = 100 };
    dodge.Cancel();
    if (!dodge.Cancelled)
        throw new InvalidOperationException("C# BunnyHurtModifiers.Cancel failed");
    Console.WriteLine("BUNNY_CSHARP_FREE_DODGE_DISPATCH=PASS expected_mod_runtime_pending");
    var liveType = liveAssembly.GetType("BunnyLoader.Source.BunnyLiveEntry")
        ?? throw new InvalidOperationException("Independent live C# callback class missing");
    var liveTick = liveType.GetMethod("Tick", BindingFlags.Public | BindingFlags.Static)
        ?? throw new InvalidOperationException("Independent live C# Tick(int,int) missing");
    int firstLive = (int)(liveTick.Invoke(null, new object[] { 5, 10 })
        ?? throw new InvalidOperationException("Live Tick did not return int"));
    int secondLive = (int)(liveTick.Invoke(null, new object[] { 5, 10 })
        ?? throw new InvalidOperationException("Live Tick did not return int"));
    if (firstLive != 13 || secondLive != 14)
        throw new InvalidOperationException(
            "C# live callbacks did not retain managed static state: " +
            firstLive + ", " + secondLive);
    var npcLiveType = liveAssembly.GetType("BunnyLoader.Source.BunnyLiveNpc")
        ?? throw new InvalidOperationException("Dynamic C# NPC callback class missing");
    var npcLiveAi = npcLiveType.GetMethod("AI", BindingFlags.Public | BindingFlags.Static)
        ?? throw new InvalidOperationException("Dynamic C# NPC AI(int,int) missing");
    int npcRight = (int)(npcLiveAi.Invoke(null, new object[] { 100, 25 })
        ?? throw new InvalidOperationException("Dynamic NPC callback returned null"));
    int npcRightNext = (int)(npcLiveAi.Invoke(null, new object[] { 100, 25 })
        ?? throw new InvalidOperationException("Dynamic NPC callback returned null"));
    int npcLeftLowLife = (int)(npcLiveAi.Invoke(null, new object[] { -100, 12 })
        ?? throw new InvalidOperationException("Dynamic NPC callback returned null"));
    int npcClose = (int)(npcLiveAi.Invoke(null, new object[] { 0, 25 })
        ?? throw new InvalidOperationException("Dynamic NPC callback returned null"));
    if (npcRight != 13 || npcRightNext != 14 ||
        npcLeftLowLife != -18 || npcClose != 0)
        throw new InvalidOperationException("Live NPC AI did not react to live state: " +
            npcRight + ", " + npcRightNext + ", " + npcLeftLowLife + ", " + npcClose);
    var secondaryType = liveAssembly.GetType("BunnyLoader.Source.ExampleWaveBoltLiveEvents")
        ?? throw new InvalidOperationException("Secondary C# projectile callback class missing");
    var secondaryHit = secondaryType.GetMethod("OnHitNPC",
        BindingFlags.Public | BindingFlags.Static)
        ?? throw new InvalidOperationException("Second projectile OnHitNPC missing");
    var secondaryKill = secondaryType.GetMethod("OnKill",
        BindingFlags.Public | BindingFlags.Static)
        ?? throw new InvalidOperationException("Second projectile OnKill missing");
    int secondaryFirst = (int)(secondaryHit.Invoke(null, new object[] { 5, 12 })
        ?? throw new InvalidOperationException("Second projectile OnHitNPC returned null"));
    int secondaryNext = (int)(secondaryHit.Invoke(null, new object[] { 5, 12 })
        ?? throw new InvalidOperationException("Second projectile OnHitNPC returned null"));
    int secondaryKillResult = (int)(secondaryKill.Invoke(null, new object[] { 5, 12 })
        ?? throw new InvalidOperationException("Second projectile OnKill returned null"));
    if (secondaryFirst != 1018 || secondaryNext != 1019 ||
        secondaryKillResult != 2018)
        throw new InvalidOperationException("Secondary projectile managed state or results invalid");
    if (args.Length == 3)
    {
        string secondOut = Path.Combine(scratch, "SecondCSharpMod.dll");
        string secondDiagnostics = Path.Combine(scratch, "second-diagnostics.txt");
        string secondRoot = Path.GetFullPath(args[2]);
        if (Compiler.Compile(secondRoot, secondOut, framework, secondDiagnostics) != 1)
            throw new InvalidOperationException("Second mod Roslyn compilation: " +
                File.ReadAllText(secondDiagnostics));
        var secondAssembly = Assembly.Load(File.ReadAllBytes(secondOut));
        var firstEvents = liveAssembly.GetType("BunnyLoader.Source.BunnyLiveEvents")
            ?? throw new InvalidOperationException("Primary mod event class missing");
        var secondEvents = secondAssembly.GetType("BunnyLoader.Source.BunnyLiveEvents")
            ?? throw new InvalidOperationException("Second mod event class missing");
        if (firstEvents.Assembly == secondEvents.Assembly)
            throw new InvalidOperationException("Same callback class name was not isolated by assembly");
        var hit = secondEvents.GetMethod("OnHitNPC", BindingFlags.Static | BindingFlags.Public)
            ?? throw new InvalidOperationException("Second mod OnHitNPC not found");
        var kill = secondEvents.GetMethod("OnKill", BindingFlags.Static | BindingFlags.Public)
            ?? throw new InvalidOperationException("Second mod OnKill not found");
        int hit1 = (int)(hit.Invoke(null, new object[] { 1, 17 }) ??
            throw new InvalidOperationException("Second mod OnHitNPC returned null"));
        int hit2 = (int)(hit.Invoke(null, new object[] { 1, 17 }) ??
            throw new InvalidOperationException("Second mod OnHitNPC returned null"));
        int kill1 = (int)(kill.Invoke(null, new object[] { 1, 17 }) ??
            throw new InvalidOperationException("Second mod OnKill returned null"));
        if (hit1 != 3019 || hit2 != 3020 || kill1 != 4019)
            throw new InvalidOperationException(
                $"Second mod class state was not isolated: {hit1}, {hit2}, {kill1}");
        Console.WriteLine("BUNNY_CSHARP_INDEPENDENT_MOD_ASSEMBLIES=PASS");
    }
    Console.WriteLine("BUNNY_CSHARP_PER_PROJECTILE_LIVE_CALLBACKS=PASS");
    Console.WriteLine("BUNNY_CSHARP_LIVE_NPC_AI_CALLBACK=PASS");
    Console.WriteLine("BUNNY_CSHARP_LIVE_STATEFUL_CALLBACK=PASS");
    Console.WriteLine("BUNNY_CSHARP_MANAGED_NPC_SLIME=PASS");
    Console.WriteLine("BUNNY_CSHARP_MANAGED_PLAYER_ACCESSORY=PASS");
    Console.WriteLine("BUNNY_CSHARP_MANAGED_BUFF_POTION=PASS");
    Console.WriteLine("BUNNY_CSHARP_AUTO_COMBAT_INDEPENDENT=PASS");
    Console.WriteLine("BUNNY_CSHARP_INDEPENDENT_EXAMPLE=PASS");
    var typedTile = new TileAnimatedTypeSmoke();
    var legacyTile = new TileAnimatedLegacySmoke();
    if ((typedTile.TileCallbackMask & (1 << 14)) == 0 ||
        (legacyTile.TileCallbackMask & (1 << 14)) == 0)
        throw new InvalidOperationException("BunnyTile overload override bit missing");
    int tx = 0, ty = 0;
    typedTile.AnimateIndividualTile(755, 4, 5, ref tx, ref ty);
    if (typedTile.LastType != 755 || tx != 9 || ty != 10)
        throw new InvalidOperationException("Native TileType lost in typed animation callback");
    tx = 0;
    ty = 0;
    legacyTile.AnimateIndividualTile(755, 4, 5, ref tx, ref ty);
    if (tx != 4 || ty != 5)
        throw new InvalidOperationException("Legacy tile animation overload delegation failed");
    string tileSmokeAssembly = typeof(TileAnimatedTypeSmoke).Assembly.Location;
    string typedDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-typed-tile",
        tileSmokeAssembly, nameof(TileAnimatedTypeSmoke), 13, 0,
        "4,5,0,0,16,16,0,0,0,0,0,755");
    string typedAnimatedDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-typed-tile",
        tileSmokeAssembly, nameof(TileAnimatedTypeSmoke), 13, 0,
        "4,5,0,0,16,16,0,0,0,0,0,755,3");
    if(typedAnimatedDraw != "DRAW|0|0|16|16|0|9|10|0")
        throw new InvalidOperationException("13-field typed tile draw lost TileType: "+typedAnimatedDraw);
    string legacyDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-legacy-tile",
        tileSmokeAssembly, nameof(TileAnimatedLegacySmoke), 13, 0,
        "4,5,0,0,16,16,0,0,0,0,0,755");
    string oldWireDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-legacy-tile",
        tileSmokeAssembly, nameof(TileAnimatedLegacySmoke), 13, 0,
        "4,5,0,0,16,16,0,0,0,0,0");
    string badTypeDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-typed-tile",
        tileSmokeAssembly, nameof(TileAnimatedTypeSmoke), 13, 0,
        "4,5,0,0,16,16,0,0,0,0,0,753");
    if (typedDraw != "DRAW|0|0|16|16|0|9|10|0" ||
        legacyDraw != "DRAW|0|0|16|16|0|4|5|0" ||
        oldWireDraw != legacyDraw ||
        badTypeDraw != "ERROR|Stage=DecodeTileDraw Reason=InvalidTileType")
        throw new InvalidOperationException("Tile animation managed draw transport mismatch: " +
            typedDraw + " / " + legacyDraw + " / " + oldWireDraw + " / " + badTypeDraw);
    string f0=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-frame-tile",tileSmokeAssembly,nameof(TileFrameSmoke),21,0,"2,4,1,0");
    string f1=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-frame-tile",tileSmokeAssembly,nameof(TileFrameSmoke),21,0,"3,4,0,1");
    string f2=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-frame-tile",tileSmokeAssembly,nameof(TileFrameSmoke),21,0,"2,4,2,0");
    if((new TileFrameSmoke().TileCallbackMask&(1<<21))==0||f0!="FRAME|1|0|1"||
       f1!="FRAME|0|0|1"||f2!="ERROR|Stage=DecodeTileFrame Reason=InvalidBooleans")
        throw new InvalidOperationException("CSharp TileFrame callback wire regression: "+f0+" / "+f1+" / "+f2);
    string smartAllow=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-smart-tile",
        tileSmokeAssembly,nameof(TileSmartInteractSmoke),20,0,"3,4,1,2,5,6");
    string smartReject=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-smart-tile",
        tileSmokeAssembly,nameof(TileSmartInteractSmoke),20,0,"9,4,1,2,5,6");
    string smartInvalid=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-smart-tile",
        tileSmokeAssembly,nameof(TileSmartInteractSmoke),20,0,"3,4,5,2,1,6");
    if((new TileSmartInteractSmoke().TileCallbackMask&(1<<20))==0||
       smartAllow!="1"||smartReject!="0"||!smartInvalid.StartsWith("ERROR|"))
        throw new InvalidOperationException("SmartInteract managed transport failed: "+
            smartAllow+" / "+smartReject+" / "+smartInvalid);
    string nativeFrame = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-native-animated",
        tileSmokeAssembly, nameof(TileNativeAnimatedSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754,5");
    string resetFrame = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-native-animated",
        tileSmokeAssembly, nameof(TileNativeAnimatedSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754,0");
    string missingFrame = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-native-animated",
        tileSmokeAssembly, nameof(TileNativeAnimatedSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    string invalidFrame = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-native-animated",
        tileSmokeAssembly, nameof(TileNativeAnimatedSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754,128");
    if (nativeFrame != "DRAW|0|0|16|16|0|0|0|0\nSPRITES|1|754,3,4,0,180,16,16,0,0,4294967295,0,2" ||
        resetFrame != "DRAW|0|0|16|16|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,0,0,4294967295,0,2" ||
        missingFrame != "DRAW|0|0|16|16|0|0|0|0" ||
        invalidFrame != "ERROR|Stage=DecodeTileDraw Reason=InvalidAnimationFrame")
        throw new InvalidOperationException("Native tile animation frame transport regression: " +
            nativeFrame + " / " + resetFrame + " / " + missingFrame + " / " + invalidFrame);
    string spriteDraw = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-sprite-tile",
        tileSmokeAssembly, nameof(TileSpriteSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if (spriteDraw != "DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,0,2,4294967295,1")
        throw new InvalidOperationException("CSharp PreDraw sprite wire mismatch: " + spriteDraw);
    if ((new TileSpriteSmoke().TileCallbackMask & (1 << 5)) == 0)
        throw new InvalidOperationException("CSharp PreDraw context overload undetected");
    string postOnly = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-postonly-tile",
        tileSmokeAssembly, nameof(TilePostOnlySmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if (postOnly != "DRAW|0|0|16|16|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,1,3,4294967295,0")
        throw new InvalidOperationException("PostDraw-only wire mismatch: "+postOnly);
    if ((new TilePostOnlySmoke().TileCallbackMask & (1 << 16)) == 0)
        throw new InvalidOperationException("PostDraw callback missing");
    string specialPost = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-postonly-tile",
        tileSmokeAssembly, nameof(TilePostOnlySmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,1,754");
    if (specialPost != "DRAW|0|0|16|16|0|0|0|0")
        throw new InvalidOperationException("PostDraw ran during special pass: "+specialPost);
    string legacyPost = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-postonly-tile",
        tileSmokeAssembly, nameof(TilePostOnlySmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0");
    if (legacyPost != "ERROR|Stage=DecodeTileDraw Reason=PostDrawRequiresTileType")
        throw new InvalidOperationException("PostDraw without native type: "+legacyPost);
    string both = BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-prepost-tile",
        tileSmokeAssembly, nameof(TilePrePostSmoke), 13, 0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if (both != "DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|2|754,3,4,0,0,16,16,0,0,4294967295,1|754,3,4,16,0,16,16,0,0,4294967295,2")
        throw new InvalidOperationException("PreDraw + PostDraw order: "+both);
    string overlayWire=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-overlay-tile",
        tileSmokeAssembly,nameof(TileOverlaySmoke),13,0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if(overlayWire!="DRAW|0|0|16|16|0|0|0|0\nSPRITES|3|754,3,4,0,0,16,16,0,0,4294967295,0|754,3,4,0,56,16,16,0,0,4294967295,0,1|754,3,4,0,36,16,16,0,2,16777215,0,2")
        throw new InvalidOperationException("Tile glow/flame source wire mismatch: "+overlayWire);
    string effects=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-effect-tile",
        tileSmokeAssembly,nameof(TileEffectsSmoke),13,0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if(effects!="DRAW|0|0|16|16|0|0|0|0\nSPRITES|2|754,3,4,0,0,16,16,0,0,4294967295,0,1|754,3,4,0,36,16,16,0,2,4294967295,1,2,1")
        throw new InvalidOperationException("DrawEffects special pass incorrect: "+effects);
    if((new TileEffectsSmoke().TileCallbackMask&((1<<17)|(1<<18)))!=((1<<17)|(1<<18)))
        throw new InvalidOperationException("DrawEffects/SpecialDraw override mask missing");
    string inactive=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-effect-tile",
        tileSmokeAssembly,nameof(TileEffectsSmoke),13,0,
        "3,4,18,0,16,16,0,0,0,0,0,754");
    if(inactive!="DRAW|18|0|16|16|0|0|0|0")
        throw new InvalidOperationException("SpecialDraw executed without request: "+inactive);
    string noSpecial=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-effect-tile",
        tileSmokeAssembly,nameof(TileEffectsSmoke),13,0,
        "3,4,0,0,16,16,0,0,0,0,1,754");
    if(noSpecial!="DRAW|0|0|16|16|0|0|0|0")
        throw new InvalidOperationException("SpecialDraw invoked in special pass: "+noSpecial);
    string dust=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-dust-tile",
        tileSmokeAssembly,nameof(TileDustSmoke),13,0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if(dust!="DRAW|0|0|16|16|0|0|0|0\nPARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295")
        throw new InvalidOperationException("EmitParticles expected native Dust wire: "+dust);
    if((new TileDustSmoke().TileCallbackMask&(1<<19))==0)
        throw new InvalidOperationException("EmitParticles override missing");
    string noDust=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-dust-tile",
        tileSmokeAssembly,nameof(TileDustSmoke),13,0,
        "3,4,0,18,16,16,0,0,0,0,0,754");
    if(noDust!="DRAW|0|18|16|16|0|0|0|0")
        throw new InvalidOperationException("EmitParticles ran with wrong tile frame: "+noDust);
    string styledDust=BunnyLoader.Source.BunnyTileRuntime.Dispatch("smoke-styled-dust",
        tileSmokeAssembly,nameof(TileStyledDustSmoke),13,0,
        "3,4,0,0,16,16,0,0,0,0,0,754");
    if(styledDust!="DRAW|0|0|16|16|0|0|0|0\nPARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,1,500,200,200,-1000,4,63")
        throw new InvalidOperationException("Styled Dust wire mismatch: "+styledDust);
    Console.WriteLine("BUNNY_CSHARP_TILE_DUST_STYLE=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_PARTICLES=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_DRAW_EFFECTS_SPECIAL=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_OVERLAYS=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_POSTDRAW=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_SPRITE_CONTEXT=PASS");
    Console.WriteLine("BUNNY_CSHARP_TILE_ANIMATION_TYPE=PASS");
    Console.WriteLine("BUNNY_ROSLYN_CACHE=PASS");
}
finally
{
    try { Directory.Delete(scratch, recursive: true); } catch { }
}



sealed class TileAnimatedTypeSmoke : BunnyLoader.Source.BunnyTile
{
    public int LastType { get; private set; } = -1;
    public override void SetStaticDefaults() { }
    public override void AnimateIndividualTile(int type, int x, int y,
        ref int addFrameX, ref int addFrameY)
    {
        LastType = type;
        addFrameX += x + type - 750;
        addFrameY += y + type - 750;
    }
}

sealed class TileAnimatedLegacySmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { }
    public override void AnimateIndividualTile(int x, int y, ref int addFrameX, ref int addFrameY)
    {
        addFrameX += x;
        addFrameY += y;
    }
}

sealed class TileSpriteSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData = false; }
    public override bool PreDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawTileTexture(0,0,16,16,0,2,0xffffffffu,1);
        return false;
    }
}

sealed class TilePostOnlySmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData = false; }
    public override void PostDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawTileTexture(0,0,16,16,1,3);
    }
}
sealed class TilePrePostSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData = false; }
    public override bool PreDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawTileTexture(0,0,16,16,0,0,0xffffffffu,1);
        return false;
    }
    public override void PostDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawTileTexture(16,0,16,16,0,0,0xffffffffu,2);
    }
}

sealed class TileOverlaySmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData=false; }
    public override void PostDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawTileTexture(0,0,16,16);
        draw.DrawGlowTexture(0,56,16,16);
        draw.DrawFlameTexture(0,36,16,16,0,2,0x00ffffffu);
    }
}

sealed class TileEffectsSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData=false; }
    public override void DrawEffects(int x,int y,int tileFrameX,int tileFrameY,
        BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        if(tileFrameX!=0)return;
        draw.DrawGlowTexture(0,0,16,16);
        draw.RequestSpecialDraw();
    }
    public override void SpecialDraw(int x,int y,BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        draw.DrawFlameTexture(0,36,16,16,0,2,0xffffffffu,1);
    }
}

sealed class TileDustSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData=false; }
    public override void EmitParticles(int x,int y,int tileFrameX,int tileFrameY,
        BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        if(tileFrameY!=0)return;
        draw.EmitDust(31,2,-4,4,8,0,-1500,100,1000);
    }
}

sealed class TileStyledDustSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData=false; }
    public override void EmitParticles(int x,int y,int tileFrameX,int tileFrameY,
        BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        if(tileFrameY!=0)return;
        draw.EmitDustStyled(31,2,-4,4,8,0,0,100,1000,0xffffffffu,
                            true,500,200,200,-1000,4,63);
    }
}

sealed class TileNativeAnimatedSmoke : BunnyLoader.Source.BunnyTile
{
    public override void SetStaticDefaults() { CacheDrawData=false; }
    public override void DrawEffects(int x,int y,int tileFrameX,int tileFrameY,
        BunnyLoader.Source.BunnyTileDrawContext draw)
    {
        if (draw.AnimationFrame >= 0)
            draw.DrawFlameTexture(0,draw.AnimationFrame*36,16,16);
    }
}

sealed class TileSmartInteractSmoke : BunnyLoader.Source.BunnyTile {
    public override void SetStaticDefaults() {}
    public override bool HasSmartInteract(int x,int y,BunnyLoader.Source.BunnyTileSmartInteractSettings settings)
        => x>=settings.LX && x<=settings.HX && y>=settings.LY && y<=settings.HY;
}

sealed class TileFrameSmoke : BunnyLoader.Source.BunnyTile {
    public override void SetStaticDefaults() {}
    public override bool TileFrame(int x,int y,ref bool resetFrame,ref bool noBreak) {
        resetFrame=false;noBreak=true;
        return x!=3;
    }
}
