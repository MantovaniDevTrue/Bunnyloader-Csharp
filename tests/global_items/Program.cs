using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp;

static void Check(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}

// Foto sintética no formato do host (BUNNY_ITEM_SNAPSHOT_V1): coluna 0 vazia, 1 = type.
string[] columns = { "damage", "crit", "knockBack", "useTime", "useAnimation", "melee", "accessory", "rare", "defense" };
string Row(int type, int damage, float knockBack, int useTime, bool melee, bool accessory) =>
    string.Join("\t", "", type, damage, 4, knockBack.ToString(System.Globalization.CultureInfo.InvariantCulture),
        useTime, useTime, melee ? 1 : 0, accessory ? 1 : 0, 0, 0);
string snapshot = "BUNNY_ITEM_SNAPSHOT_V1\ttype\t" + string.Join("\t", columns) + "\n" +
    Row(23, 0, 0, 15, false, false) + "\n" +          // Gel
    Row(24, 7, 5, 25, true, false) + "\n" +           // Wooden Sword
    Row(211, 0, 0, 0, false, true) + "\n" +           // Feral Claws
    Row(3507, 5, 4, 12, true, false) + "\n" +         // Copper Shortsword
    Row(4, 10, 6, 20, false, false) + "\n";           // nada se aplica

Assembly mod = typeof(ExampleShortswordGlobalItem).Assembly;
var globals = BunnyGlobalItemRuntime.ContentGlobals(mod);
Check(globals.Length == 4, "Expected four content GlobalItems, got " + globals.Length);
Check(BunnyGlobalItemRuntime.ContentMask(typeof(ExampleShortswordGlobalItem)) == (1 | 2), "Shortsword mask");
Check(BunnyGlobalItemRuntime.ContentMask(typeof(ExampleGelGlobalItem)) == (1 | 4), "Gel mask");
Check(BunnyGlobalItemRuntime.ContentMask(typeof(ExampleWeaponTuningGlobalItem)) == (1 | 8 | 16), "Tuning mask");
Check(BunnyGlobalItemRuntime.ContentMask(typeof(ExampleAccessoryGlobalItem)) == (1 | 4 | 64), "Accessory mask");

string wire = BunnyGlobalItemRuntime.Evaluate(mod, snapshot);
var lines = wire.Split('\n', StringSplitOptions.RemoveEmptyEntries);
Check(lines[0] == "GLOBAL_ITEMS_V1", "Header");
Check(lines.Contains("DEFAULTS\t3507\tdamage:i:50"), "Shortsword SetDefaults: " + wire);
Check(lines.Contains("TOOLTIP\t23\t" + Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Linha adicionada por um GlobalItem C#"))), "Gel tooltip: " + wire);
Check(lines.Contains("DAMAGE\t24\t0\t1.1\t1\t0"), "Wooden sword damage: " + wire);
Check(lines.Contains("CRIT\t24\t4"), "Wooden sword crit: " + wire);
Check(lines.Contains("CLASS_BONUS\t211\tV2\t0\t1.05\t1\t0\t0\t1\t1\t0\t0\t0\t0"), "Feral claws bonus: " + wire);
Check(!lines.Any(line => line.Contains("\t4\t") && line.StartsWith("DEFAULTS\t4\t")), "Unrelated item untouched");
Check(lines.Contains("TOOLTIP\t211\t" + Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("+5% de dano corpo a corpo (GlobalItem C#)"))), "Feral claws tooltip: " + wire);
Check(lines.Length == 7, "Unexpected lines: " + wire);

// SetDefaults fora do SetDefaults e campo somente leitura falham.
bool threw = false;
try { BunnyGlobalItemRuntime.Evaluate(mod, "BUNNY_ITEM_SNAPSHOT_V1\ttype\tdamage\n\t0\t5\n"); }
catch (FormatException) { threw = true; }
Check(threw, "Invalid type accepted");
Check(BunnyGlobalItemRuntime.EvaluateFile(mod, "/nonexistent/snapshot.txt").StartsWith("ERROR\t"), "Missing file must be ERROR");
Console.WriteLine("BUNNY_GLOBAL_ITEM_CONTRACT=PASS globals=" + globals.Length + " lines=" + (lines.Length - 1));
