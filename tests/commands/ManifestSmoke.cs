using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

var assembly = Assembly.GetExecutingAssembly();
string manifest = CommandManifestWriter.Build(assembly);
if (CommandManifestWriter.CountCommands(assembly) != 1)
    throw new Exception("esperava uma classe BunnyCommand");
if (!manifest.StartsWith("BUNNY_COMMANDS_V1\nCLASSES\t1\n", StringComparison.Ordinal))
    throw new Exception("header/count incorretos");
if (!manifest.Contains("CMD\tManifestSmoke.Heal\theal\t1\n", StringComparison.Ordinal) ||
    !manifest.Contains("ALIAS\tManifestSmoke.Heal\tcurar\t1\n", StringComparison.Ordinal))
    throw new Exception("aliases/rota de comando ausentes");
var registry = new BunnyCommandRegistry();
if (registry.RegisterAssembly("smoke", assembly) != 1)
    throw new Exception("runtime C# nao reconheceu manifest command");
var output = new List<BunnyCommandReply>();
if (!registry.TryHandle("/curar 7", new BunnyCommandCaller(0, 0, BunnyCommandCallerKind.Chat, output.Add)) ||
    output.Count != 1 || output[0].Text != "ok:7")
    throw new Exception("Alias nao executou");

string temp = Path.Combine(Path.GetTempPath(), "command-smoke-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
    string diagnostics = Path.Combine(temp, "verify.txt");
    int count = CompiledAssemblyProbe.Verify(assembly.Location, diagnostics);
    if (count != 1)
        throw new Exception("Verifier retornou " + count + ": " + File.ReadAllText(diagnostics));
    string installed = File.ReadAllText(Path.Combine(temp, "source-commands.txt"));
    if (installed != manifest)
        throw new Exception("manifesto do verificador nao coincide");
    if (!File.ReadAllText(Path.Combine(temp, "source-runtime-values.txt"))
          .StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal))
        throw new Exception("mod so de comandos perdeu cabecalho de defaults");
    if (!File.ReadAllText(Path.Combine(temp, "source-combat-policies.txt"))
          .StartsWith("BUNNY_COMBAT_POLICIES_V1\n", StringComparison.Ordinal))
        throw new Exception("mod so de comandos perdeu cabecalho de combate");
}
finally
{
    Directory.Delete(temp, true);
}
Console.WriteLine("BUNNY_CSHARP_COMMAND_MANIFEST=PASS");

namespace ManifestSmoke
{
    public sealed class Heal : BunnyCommand
    {
        public override string Command => "heal";
        public override IReadOnlyList<string> Aliases => new[] { "curar" };
        public override void Action(BunnyCommandContext ctx) =>
            ctx.Caller.Reply("ok:" + ctx.Arguments.Single());
    }
}
