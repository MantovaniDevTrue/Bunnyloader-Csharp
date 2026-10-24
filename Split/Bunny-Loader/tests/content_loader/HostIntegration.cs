using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ContentLoaderExample;

static class HostIntegration
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string label)
        {
            if (!ok) throw new Exception("HOST_INTEGRATION_FAIL " + label + ": " + BunnyContentHostDispatcher.LastError);
            checks++;
        }
        string assembly = typeof(ExampleBootstrap).Assembly.Location;
        string temp = Path.Combine(Path.GetTempPath(), "bunny-content-host-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try
        {
            string diagnostics = Path.Combine(temp, "verify.txt");
            // Exercise the production compiled-assembly verifier on an assembly with NO NPC/item.
            // Its Load callbacks are deliberately NOT run in the isolated compiler process.
            Check(CompiledAssemblyProbe.Verify(assembly, diagnostics) == 2, "service-only assembly accepted by real verifier");
            Check(CompiledAssemblyProbe.ManagedContentTypeCount() == 2, "service count exported separately from native counts");
            Check(ModContent.GetContent<ModType>().Count == 0, "compiler worker did not invoke Load/Register");
            Check(File.ReadAllText(Path.Combine(temp, "source-runtime-values.txt")).Trim() == "BUNNY_MANAGED_DEFAULTS_V2", "service-only wire has no invented native records");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-one", assembly) == 2, "automatic loader discovery and managed bootstrap");
            var settings = ModContent.GetInstance<ExampleSpawnSettings>();
            Check(settings?.Life == 240, "SetupContent completed before acknowledgement");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-one", assembly) == 2 &&
                ReferenceEquals(settings, ModContent.GetInstance<ExampleSpawnSettings>()), "duplicate load idempotent");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-two", assembly) == 2, "two owners share an assembly without duplicating each mod");
            Check(ContentInstance<ExampleSpawnSettings>.Instances?.Count == 2, "two real scope instances");
            Check(Task.Run(() => BunnyContentHostDispatcher.Dispatch(1, "host-one", "")).GetAwaiter().GetResult() == -1, "wrong thread cannot unload content");
            Check(BunnyContentLoader.IsLoaded("host-one"), "thread rejection preserves content");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-one", Path.Combine(temp, "missing.dll")) == -1, "invalid assembly cannot replace active scope");
            Check(BunnyContentHostDispatcher.Dispatch(1, "host-one", "") == 0 &&
                BunnyContentLoader.IsLoaded("host-two"), "unload one mod preserves the other");
            Check(ModContent.GetInstance<ExampleSpawnSettings>()?.Life == 240, "unload transitions multiple-to-single");
            Check(BunnyContentHostDispatcher.Dispatch(1, "host-two", "") == 0, "last assembly owner unload");
            Check(ModContent.GetContent<ModType>().Count == 0, "all scope references released");
            var freshLoader = LoaderManager.Get<ExampleDefinitionLoader>();
            Check(freshLoader.TotalCount == 4, "assembly loader released after last mod");
            LoaderManager.Clear();
            string mixedAssembly = typeof(MixedContentFixture.FixtureNpc).Assembly.Location;
            Check(CompiledAssemblyProbe.Verify(mixedAssembly, diagnostics) == 2 && CompiledAssemblyProbe.ManagedContentTypeCount() == 1,
                "mixed assembly count separates one legacy NPC from one ModType");
            Check(BunnyContentHostDispatcher.Dispatch(0, "mixed", mixedAssembly) == 1 &&
                ModContent.GetInstance<MixedContentFixture.Settings>()?.Life == 77, "mixed mod service loaded");
            Check(MixedContentFixture.FixtureNpc.Constructions == 0, "bootstrap does not instantiate legacy NPC a second time");
            Check(BunnyContentHostDispatcher.Dispatch(1, "mixed", "") == 0, "mixed mod service unloaded");
            Check(!typeof(BunnyContentHostDispatcher).Assembly.GetReferencedAssemblies().Any(name => name.Name!.StartsWith("System.Security.Cryptography", StringComparison.Ordinal)),
                "bootstrap introduces no Android Crypto JNI dependency");
            // LoadFrom can cache code even after scope unload. Changing bytes must require a
            // restart instead of acknowledging a new file while executing old CLR code.
            string copiedAssembly = Path.Combine(temp, "copied-example.dll");
            File.Copy(assembly, copiedAssembly);
            Check(BunnyContentHostDispatcher.Dispatch(0, "hash-check", copiedAssembly) == 2, "load copied assembly path");
            Check(BunnyContentHostDispatcher.Dispatch(0, "hash-check", assembly) == -1, "active scope rejects changed path even with identical bytes");
            using (var changed = new FileStream(copiedAssembly, FileMode.Append)) changed.WriteByte(0);
            Check(BunnyContentHostDispatcher.Dispatch(0, "hash-check", copiedAssembly) == -1, "active scope rejects changed assembly bytes");
            Check(BunnyContentHostDispatcher.Dispatch(1, "hash-check", "") == 0, "unload scope whose file changed");
            Check(BunnyContentHostDispatcher.Dispatch(0, "hash-check", copiedAssembly) == -1, "previously loaded path rejects replacement after unload");
            File.Copy(assembly, copiedAssembly, true);
            Check(BunnyContentHostDispatcher.Dispatch(0, "hash-check", copiedAssembly) == 2 &&
                BunnyContentHostDispatcher.Dispatch(1, "hash-check", "") == 0, "unchanged original bytes can be re-enabled");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-one", assembly) == 2, "re-enable unchanged assembly");
            Check(BunnyContentHostDispatcher.Dispatch(2, "", "") == 0, "global shutdown acknowledgement");
            Check(ModContent.GetContent<ModType>().Count == 0 && ContentInstance<ExampleSpawnSettings>.Instance == null, "shutdown clears singletons and names");
            Check(BunnyContentHostDispatcher.Dispatch(2, "", "") == 0, "shutdown idempotent");
            Check(BunnyContentHostDispatcher.Dispatch(0, "host-one", assembly) == -1, "shutdown terminal: no queued reactivation");
        }
        finally
        {
            BunnyContentHostDispatcher.Dispatch(2, "", "");
            Directory.Delete(temp, true);
        }
        return checks;
    }
}
