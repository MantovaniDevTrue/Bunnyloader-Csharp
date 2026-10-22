using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

static void Check(bool value, string label)
{
    if (!value) throw new InvalidOperationException(label);
    Console.WriteLine("OK " + label);
}

string root = Path.Combine(Path.GetTempPath(), "bunny-hair-compiler-" +
    Guid.NewGuid().ToString("N"));
string mod = Path.Combine(root, "mod");
string assets = Path.Combine(mod, "content", "Content", "Hairs");
Directory.CreateDirectory(assets);
try
{
    string alphaPath = Path.Combine(assets, "AlphaHair.png");
    string betaPath = Path.Combine(assets, "BetaHair.png");
    string alternate = Path.Combine(assets, "BetaHair_Alt.png");
    File.WriteAllBytes(alphaPath, new byte[] { 1 });
    File.WriteAllBytes(betaPath, new byte[] { 1 });
    File.WriteAllBytes(alternate, new byte[] { 1 });
    string destination = Path.Combine(root, "source-hair-manifest.txt");
    HairManifestSession result = CompiledHairProbe.Export(
        Assembly.GetExecutingAssembly(), "examplemodcsharp",
        mod, destination, 300);

    Check(result.Count == 2, "descobre duas classes C# compiladas");
    Check(result.Loader.TryResolveSavedHair("examplemodcsharp/AlphaHair", out int alpha) &&
          alpha == 300, "alocação começa no ID reservado");
    Check(result.Loader.TryResolveSavedHair("examplemodcsharp/BetaHair", out int beta) &&
          beta == 301, "segunda classe mantém ordem alfabética");
    string manifest = File.ReadAllText(destination);
    Check(manifest == result.Manifest &&
          manifest.StartsWith("BUNNY_HAIR_MANIFEST_V1\n", StringComparison.Ordinal),
          "manifeste C# corresponde ao arquivo escrito");
    Check(manifest.Contains("HAIR\t300\texamplemodcsharp\tAlphaHair\tContent/Hairs/AlphaHair.png\tContent/Hairs/AlphaHair.png\t0\t1\n", StringComparison.Ordinal),
          "sem textura _Alt cai para a textura principal");
    Check(manifest.Contains("HAIR\t301\texamplemodcsharp\tBetaHair\tContent/Hairs/BetaHair.png\tContent/Hairs/BetaHair_Alt.png\t2\t0\n", StringComparison.Ordinal),
          "gênero e disponibilidade de criação preservados");

    File.Delete(betaPath);
    bool missingRejected = false;
    try
    {
        CompiledHairProbe.Export(Assembly.GetExecutingAssembly(), "examplemodcsharp",
            mod, destination, 300);
    }
    catch (InvalidOperationException)
    {
        missingRejected = true;
    }
    Check(missingRejected && !File.Exists(destination),
          "textura principal ausente interrompe o lote e apaga manifesto obsoleto");
    File.WriteAllBytes(betaPath, new byte[] { 1 });

    string outside = Path.Combine(root, "outside.png");
    File.WriteAllBytes(outside, new byte[] { 1 });
    File.Delete(alphaPath);
    File.CreateSymbolicLink(alphaPath, outside);
    bool symlinkRejected = false;
    try
    {
        CompiledHairProbe.Export(Assembly.GetExecutingAssembly(), "examplemodcsharp",
            mod, destination, 300);
    }
    catch (InvalidOperationException)
    {
        symlinkRejected = true;
    }
    Check(symlinkRejected && !File.Exists(destination),
          "simbolic link fora do mod nao é aceito como PNG interno");
    File.Delete(alphaPath);
    File.WriteAllBytes(alphaPath, new byte[] { 1 });

    bool idRejected = false;
    try
    {
        CompiledHairProbe.Export(Assembly.GetExecutingAssembly(), "examplemodcsharp",
            mod, destination, 227);
    }
    catch (ArgumentOutOfRangeException)
    {
        idRejected = true;
    }
    Check(idRejected && !File.Exists(destination),
          "ID na faixa vanilla é recusado e não cria manifesto");

    if (args.Length == 1)
    {
        // Hand-off em CI para o parser C++ real: os bytes vêm do SDK
        // C# compilado, não de strings ou entradas inventadas pelo teste.
        string handoff = Path.GetFullPath(args[0]);
        if (Path.GetDirectoryName(handoff) != root &&
            !handoff.StartsWith(Path.GetTempPath(), StringComparison.Ordinal))
            throw new InvalidOperationException("Destino de teste não autorizado.");
        string nativeWorkDir = Path.GetDirectoryName(handoff)!;
        Check(Path.GetFileName(handoff) == "source-hair-examplemodcsharp.manifest",
              "nome seguro do manifesto fixado por UID");
        CompiledHairProbe.ExportForNative(Assembly.GetExecutingAssembly(),
            "examplemodcsharp", mod, nativeWorkDir, 300);
        Check(File.Exists(handoff), "manifesto real escrito no nome nativo esperado");
        bool unsafeUidRejected = false;
        try
        {
            CompiledHairProbe.ExportForNative(Assembly.GetExecutingAssembly(),
                "../examplemodcsharp", mod, nativeWorkDir, 300);
        }
        catch (ArgumentException)
        {
            unsafeUidRejected = true;
        }
        Check(unsafeUidRejected, "exportador MonoVM recusa UID com traversal");
        Console.WriteLine("HAIR_MANAGED_HANDOFF=" + handoff);
    }
    Console.WriteLine("HairCompiledProbe: verificações passaram sem APK.");
}
finally
{
    if (Directory.Exists(root)) Directory.Delete(root, true);
}

public sealed class AlphaHair : ModHair
{
    // Este é o caminho relativo à pasta content/ do mod C#.
    public override string Texture => "Content/Hairs/AlphaHair";
}

public sealed class BetaHair : BunnyHair
{
    // O _Alt só é usado quando o personagem está usando um chapéu.
    public override string Texture => "Content/Hairs/BetaHair";
    public override Gender RandomizedCharacterCreationGender => Gender.Female;
    public override bool AvailableDuringCharacterCreation => false;
}
