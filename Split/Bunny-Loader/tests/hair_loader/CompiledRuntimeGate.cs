using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

static void Assert(bool result, string description)
{
    if (!result) throw new InvalidOperationException("Falhou: " + description);
    Console.WriteLine("OK " + description);
}

CompiledHairProbe.RejectUninstalledHairTypes(typeof(ModHair).Assembly);
Console.WriteLine("OK SDK com ModHair abstrato não bloqueia o compilador");

bool caught = false;
try
{
    CompiledHairProbe.RejectUninstalledHairTypes(Assembly.GetExecutingAssembly());
}
catch (InvalidOperationException ex)
{
    caught = ex.Message.Contains("NativeHairLifecycleIncomplete", StringComparison.Ordinal) &&
             ex.Message.Contains(nameof(HiddenHair), StringComparison.Ordinal);
}
Assert(caught, "herança indireta detectada antes de SetDefaults");

string diag = Path.Combine(Path.GetTempPath(), "bunny-hair-gate-" +
    Guid.NewGuid().ToString("N") + ".txt");
try
{
    int result = CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location, diag);
    string details = File.ReadAllText(diag);
    Assert(result == -1 &&
           details.Contains("NativeHairLifecycleIncomplete", StringComparison.Ordinal),
           "MonoVM real rejeita assembly com hair indireto mesmo contendo BunnyItem");
}
finally
{
    if (File.Exists(diag)) File.Delete(diag);
}

Console.WriteLine("HairLoader Roslyn/MonoVM inherited-class gate PASS; NO APK.");

public abstract class BaseHairWrapper : ModHair { }

public sealed class HiddenHair : BaseHairWrapper
{
    public override string Texture => "Content/Hairs/HiddenHair";
}

public sealed class SecondarySampleItem : BunnyItem
{
    public override void SetDefaults()
    {
        // Testa que outro conteúdo no mesmo assembly não libera um hair
        // sem completar os hooks nativos do carregador.
        DisplayName = "Sample";
        Texture = "Content/Items/Sample";
    }
}
