using System;
using System.Linq;
using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

string assemblyPath = Assembly.GetExecutingAssembly().Location;
string descriptor = CommandManifestWriter.Build(Assembly.GetExecutingAssembly());
if (!descriptor.Contains("CMD\tExampleModCSharp.Commands.ExampleHealCommand\theal\t1\n", StringComparison.Ordinal) ||
    !descriptor.Contains("DESC\tExampleModCSharp.Commands.ExampleHealCommand\t", StringComparison.Ordinal) ||
    !descriptor.Contains("CMD\tExampleModCSharp.Commands.ExampleTimeCommand\taddtime\t4\n", StringComparison.Ordinal) ||
    !descriptor.Contains("DESC\tExampleModCSharp.Commands.ExampleTimeCommand\t", StringComparison.Ordinal))
    throw new Exception("Roslyn deixou de registrar a ajuda dos dois exemplos");
var registry = new BunnyCommandRegistry();
if (registry.RegisterAssembly("exampleactions", Assembly.GetExecutingAssembly()) != 2)
    throw new Exception("Faltam HealCommand ou ExampleTimeCommand");

static string Expect(string input, int mode, BunnyCommandCallerKind kind,
                     bool handled = true)
{
    string result = BunnyCommandWorker.Execute(input, 0, mode, (int)kind);
    if (result.StartsWith("BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\n") != handled)
        throw new Exception("Tratamento divergente: " + input + " => " + result);
    return result;
}
if (BunnyCommandWorker.RegisterAssembly("exampleactions", assemblyPath) != 2)
    throw new Exception("Nao registrou os dois comandos no worker");
string heal = Expect("/heal 27.8", 0, BunnyCommandCallerKind.Chat);
if (!heal.Contains("ACTION\tHEAL\t27\n", StringComparison.Ordinal) ||
    heal.Contains("REPLY\t", StringComparison.Ordinal))
    throw new Exception("Heal prematuro ou incorreto: " + heal);
if (!Expect("/curar 15", 1, BunnyCommandCallerKind.Chat)
      .Contains("ACTION\tHEAL\t15\n", StringComparison.Ordinal))
    throw new Exception("Alias de cura falhou");
if (Expect("/heal -3", 0, BunnyCommandCallerKind.Chat).Contains("ACTION\t"))
    throw new Exception("Quantidade invalida deve falhar");
if (!Expect("/heal 999999999999", 0, BunnyCommandCallerKind.Chat)
      .Contains("ACTION\tHEAL\t1000000\n", StringComparison.Ordinal))
    throw new Exception("Heal sem limite de integridade");
if (!Expect("/addTime 90000", 0, BunnyCommandCallerKind.Chat)
      .Contains("ACTION\tADDTIME\t3600\n", StringComparison.Ordinal))
    throw new Exception("Ciclo do mundo incorreto em solo");
if (!Expect("/addTime -6", 2, BunnyCommandCallerKind.Server)
      .Contains("ACTION\tADDTIME\t-6\n", StringComparison.Ordinal))
    throw new Exception("Servidor dedicado nao recebeu hora");
if (!Expect("/addTime 72000", 3, BunnyCommandCallerKind.Server)
      .Contains("ACTION\tADDTIME\t72000\n", StringComparison.Ordinal))
    throw new Exception("Host nao recebeu hora");
Expect("/addTime 2", 1, BunnyCommandCallerKind.Chat, false);
if (Expect("/addTime 3.5", 0, BunnyCommandCallerKind.Chat).Contains("ACTION\t"))
    throw new Exception("Horario fracionario nao pode passar");
BunnyCommandWorker.Unregister("exampleactions");
Expect("/heal 5", 0, BunnyCommandCallerKind.Chat, false);
BunnyCommandWorker.Clear();
Console.WriteLine("BUNNY_CSHARP_COMMAND_ACTIONS=PASS");
