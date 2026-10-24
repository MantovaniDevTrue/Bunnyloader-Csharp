using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

string temporary = Path.Combine(Path.GetTempPath(), "bunny-sdk-foundation-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temporary);
int checks = 0;
void Check(bool valid, string name)
{
    if (!valid) throw new InvalidOperationException(name);
    ++checks;
    Console.WriteLine("PASS " + name);
}
try
{
    string runtime = Path.Combine(temporary, "runtime");
    Directory.CreateDirectory(runtime);
    string framework = Path.GetDirectoryName(typeof(object).Assembly.Location)!;
    foreach (string name in new[] { "System.Private.CoreLib", "System.Runtime", "netstandard",
        "System.Collections", "System.Collections.Concurrent", "System.Linq", "System.Console",
        "System.Runtime.Extensions", "System.ObjectModel", "System.Threading", "System.Threading.Tasks",
        "System.Numerics.Vectors", "System.ComponentModel.Primitives" })
        File.Copy(Path.Combine(framework, name + ".dll"), Path.Combine(runtime, name + ".dll"));
    File.Copy(typeof(BunnyItem).Assembly.Location, Path.Combine(runtime, "BunnySourceSdk.dll"));
    string mod = Path.Combine(temporary, "mod");
    string content = Path.Combine(mod, "content");
    Directory.CreateDirectory(content);
    string source = Path.Combine(content, "Material.cs");
    File.WriteAllText(source, """
        using BunnyLoader.Source;
        namespace IndependentMod;
        public abstract class MaterialBase<T> : BunnyItem {
            public override void SetDefaults() { MaxStack = 9999; Value = 10; }
        }
        [BunnyContent(BunnyContentKind.Item, "Independent Material", "Content/Items/Material.png")]
        public sealed class Material : MaterialBase<int> {
            public override void SetDefaults() { base.SetDefaults(); Damage = 12 + 3; }
            public override void AddRecipes() { CreateRecipe().AddIngredient(2, 1).Register(); }
        }
        """);
    string output = Path.Combine(temporary, "Independent.dll");
    string diag = Path.Combine(temporary, "diagnostics.txt");
    int Compile() => Compiler.Compile(mod, output, runtime, diag);
    Check(Compile() == 1, "initial compile");
    int foundationResult = CompiledAssemblyProbe.Verify(output, diag);
    if (foundationResult != 1) Console.WriteLine(File.ReadAllText(diag));
    Check(foundationResult == 1, "closed generic base and abstract helper");
    string[] wire = File.ReadAllLines(Path.Combine(temporary, "source-runtime-values.txt"));
    string[] record = wire.Single(line => line.StartsWith("ITEM\tMaterial\t")).Split('\t');
    Check(record[4] == "9999" && record[7] == "15", "base and computed defaults reached real wire");
    Check(Compile() == 2 && File.ReadAllText(diag).Contains("parsed=0 integrity=verified"),
        "verified cache skips parsing");
    byte[] dll = File.ReadAllBytes(output);
    File.WriteAllBytes(output, dll.Take(dll.Length / 2).ToArray());
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == 1,
        "truncated DLL recompiles");
    dll = File.ReadAllBytes(output);
    dll[dll.Length / 2] ^= 0x40;
    File.WriteAllBytes(output, dll);
    Check(Compile() == 1, "same-length corrupt DLL recompiles");
    File.WriteAllText(output + ".sha256", "legacy or torn manifest");
    Check(Compile() == 1, "legacy cache migrates by recompiling");
    string optionalReference = Path.Combine(runtime, "System.ComponentModel.Primitives.dll");
    File.Move(optionalReference, optionalReference + ".saved");
    Check(Compile() == 1, "reference set removal invalidates cache");
    File.Move(optionalReference + ".saved", optionalReference);
    Check(Compile() == 1, "reference set restoration invalidates cache");
    byte[] savedReference = File.ReadAllBytes(optionalReference);
    var replacement = CSharpCompilation.Create("ChangedOptionalReference",
        new[] { CSharpSyntaxTree.ParseText("public sealed class ReplacementReference {}") },
        new[] { MetadataReference.CreateFromFile(Path.Combine(runtime, "System.Private.CoreLib.dll")) },
        new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
    using (var stream = File.Create(optionalReference))
        Check(replacement.Emit(stream).Success, "replacement framework fixture compiles");
    Check(Compile() == 1, "reference MVID change invalidates cache");
    File.WriteAllBytes(optionalReference, savedReference);
    string core = Path.Combine(runtime, "System.Runtime.dll");
    File.Move(core, core + ".saved");
    Check(Compile() == -7, "missing mandatory reference rejects cached output");
    File.Move(core + ".saved", core);
    string original = File.ReadAllText(source);
    File.WriteAllText(source, original.Replace("Damage = 12 + 3", "Damage = 12 + 4"));
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == 1,
        "source edit invalidates cache");
    File.WriteAllText(source, original.Replace("BunnyContentKind.Item", "BunnyContentKind.Tile"));
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == -1 &&
        File.ReadAllText(diag).Contains("kind does not match"), "wrong declared category rejected");
    File.WriteAllText(source, original.Replace("base.SetDefaults();", "base.SetDefaults(); Texture = \"Other.png\";"));
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == -1 &&
        File.ReadAllText(diag).Contains("changed declared Texture"), "resource identity mismatch rejected");
    File.WriteAllText(source, original.Replace(
        "[BunnyContent(BunnyContentKind.Item, \"Independent Material\", \"Content/Items/Material.png\")]", ""));
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == -1 &&
        File.ReadAllText(diag).Contains("inherited content requires"), "unregistered inherited content diagnosed");
    string starter = Path.GetFullPath("samples/StarterModCSharp/Playable");
    Check(Compiler.Compile(starter, output, runtime, diag) == 1 &&
        CompiledAssemblyProbe.Verify(output, diag) == 1, "starter mod compiles and exports");
    string declared = Path.GetFullPath("samples/DeclaredContentCSharp/Playable");
    Check(Compiler.Compile(declared, output, runtime, diag) == 1 &&
        CompiledAssemblyProbe.Verify(output, diag) == 1, "declared content sample compiles and exports");
    string accessory = Path.GetFullPath("samples/NativeAccessoryCSharp/Playable");
    Check(Compiler.Compile(accessory, output, runtime, diag) == 1 &&
        CompiledAssemblyProbe.Verify(output, diag) == 1, "native accessory sample compiles and exports");
    var nativeWire = File.ReadAllLines(Path.Combine(temporary, "source-runtime-values.txt"))
        .Single(line => line.StartsWith("ITEM\tNativeCharm\t")).Split('\t');
    Check(nativeWire.Length == 25 && nativeWire[20] == "-" && nativeWire[23] == "2" &&
        nativeWire[24] == "meleeDamage:mul:1.1;moveSpeed:add:0.05;strongBees:set:1", "accessory has native effects and no JS policy");
    Directory.CreateDirectory("build/sdk-foundation");
    File.Copy(Path.Combine(temporary, "source-runtime-values.txt"),
        "build/sdk-foundation/native-accessory-wire.txt", true);
    File.WriteAllText(source, """
        using BunnyLoader.Source;
        public sealed class InvalidAccessory : BunnyItem {
            public override void SetDefaults() {
                DisplayName="Invalid"; Texture="Content/Items/Invalid.png";
                Behavior.SetOnEquip(BunnyEquipFlag.StrongBees);
            }
        }
        """);
    Check(Compile() == 1 && CompiledAssemblyProbe.Verify(output, diag) == -1 &&
        File.ReadAllText(diag).Contains("Accessory=true"), "native equip effects require Accessory=true");
    string armorSample=Path.GetFullPath("samples/ArmorPoliciesCSharp/Playable");
    Check(Compiler.Compile(armorSample,output,runtime,diag)==1 &&
        CompiledAssemblyProbe.Verify(output,diag)==3,"independent armor sample compiles and exports");
    var armorRows=File.ReadAllLines(Path.Combine(temporary,"source-runtime-values.txt")).Skip(1).Select(x=>x.Split('\t')).ToArray();
    Check(armorRows.Length==3&&armorRows.All(x=>x.Length==26&&x[25].StartsWith("A1|")),"armor sample exports native policies");
    File.Copy(Path.Combine(temporary,"source-runtime-values.txt"),"build/sdk-foundation/armor-sample-wire.txt",true);
    string serviceSample = Path.GetFullPath("samples/ContentLoaderCSharp/Playable");
    Check(Compiler.Compile(serviceSample, output, runtime, diag) == 1, "service-only sample compiles with production Roslyn pipeline");
    Check(CompiledAssemblyProbe.Verify(output, diag) == 2 && CompiledAssemblyProbe.ManagedContentTypeCount() == 2,
        "service-only verifier reports two managed and zero native types");
    Check(ModContent.GetContent<ModType>().Count == 0, "compiler did not load persistent services");
    Check(BunnyContentHostDispatcher.Dispatch(0, "service-sample", output) == 2, "compiled service-only package loads on managed host");
    var summary = ModContent.GetContent<ModType>().Single(value => value.Name == "ServiceSummary");
    Check((int)summary.GetType().GetProperty("ComputedLife")!.GetValue(summary)! == 240,
        "compiled service dependencies and aliases ready before host ack");
    Check(BunnyContentHostDispatcher.Dispatch(1, "service-sample", "") == 0 && ModContent.GetContent<ModType>().Count == 0,
        "compiled service-only package unloads cleanly");
    Console.WriteLine("BUNNY_SDK_FOUNDATION=PASS checks=" + checks);
}
finally
{
    Directory.Delete(temporary, true);
}
