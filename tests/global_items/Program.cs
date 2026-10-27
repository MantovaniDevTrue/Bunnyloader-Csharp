using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp;

static void Check(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}

// Foto sintética no formato do host (BUNNY_ITEM_SNAPSHOT_V1): coluna 0 vazia, 1 = type.
string[] columns = { "damage", "crit", "knockBack", "useTime", "useAnimation", "melee", "accessory", "rare", "defense", "shoot" };
string Row(int type, int damage, float knockBack, int useTime, bool melee, bool accessory, int shoot = 0) =>
    string.Join("\t", "", type, damage, 4, knockBack.ToString(System.Globalization.CultureInfo.InvariantCulture),
        useTime, useTime, melee ? 1 : 0, accessory ? 1 : 0, 0, 0, shoot);
string snapshot = "BUNNY_ITEM_SNAPSHOT_V1\ttype\t" + string.Join("\t", columns) + "\n" +
    Row(23, 0, 0, 15, false, false) + "\n" +          // Gel
    Row(24, 7, 5, 25, true, false) + "\n" +           // Wooden Sword
    Row(211, 0, 0, 0, false, true) + "\n" +           // Feral Claws
    Row(3507, 5, 4, 12, true, false, 949) + "\n" +    // Copper Shortsword (atira a estocada)
    Row(4, 10, 6, 20, false, false) + "\n";           // nada se aplica

Assembly mod = typeof(ExampleShortswordGlobalItem).Assembly;
var globals = BunnyGlobalItemRuntime.ContentGlobals(mod);
Check(globals.Length == 4, "Expected four content GlobalItems, got " + globals.Length);
Check(BunnyGlobalItemRuntime.ContentMask(typeof(ExampleShortswordGlobalItem)) == (1 | 2 | 512), "Shortsword mask");
Check(BunnyGlobalItemRuntime.ShootMask(typeof(ExampleShortswordGlobalItem)) == 4, "Shortsword shoot mask");
Check(BunnyGlobalItemRuntime.ShootMask(typeof(ExampleGelGlobalItem)) == 0, "Gel shoot mask");
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
const string shortswordOwner = "@GlobalItem:ExampleModCSharp.ExampleShortswordGlobalItem";
Check(lines.Contains("SHOOT\t3507\t4\t" + shortswordOwner), "Shortsword shoot owner: " + wire);
Check(!lines.Any(line => line.StartsWith("SHOOT\t24\t")), "Items without shooting callbacks have no SHOOT");
Check(lines.Length == 8, "Unexpected lines: " + wire);

// Disparo no momento do uso: BunnyShootRuntime resolve o dono @GlobalItem.
string assemblyPath = mod.Location;
string shotInput = "0,3507,50,7,949,100,200,2,0,50,4,0,0,0";
string Shoot(int kind, string snapshot) => BunnyShootRuntime.Dispatch("globals", assemblyPath, shortswordOwner, 0, kind, snapshot);
Check(Shoot(2, "") == "READY", "Global shoot warmup");
Check(Shoot(0, shotInput) == "1", "Default CanShoot");
string shot = Shoot(1, shotInput);
Check(shot == "SHOT1|0|v949,100,200,2,0,50,4,0,0,0|v30,100,200,10,0,50,4,0,0,0", "Grenade replaces the stab: " + shot);
Check(BunnyShootRuntime.Dispatch("globals", assemblyPath, "@GlobalItem:ExampleModCSharp.ExampleGelGlobalItem", 0, 2, "").StartsWith("ERROR|"),
    "GlobalItem without shooting callbacks is rejected");

// SetDefaults fora do SetDefaults e campo somente leitura falham.
bool threw = false;
try { BunnyGlobalItemRuntime.Evaluate(mod, "BUNNY_ITEM_SNAPSHOT_V1\ttype\tdamage\n\t0\t5\n"); }
catch (FormatException) { threw = true; }
Check(threw, "Invalid type accepted");
Check(BunnyGlobalItemRuntime.EvaluateFile(mod, "/nonexistent/snapshot.txt").StartsWith("ERROR\t"), "Missing file must be ERROR");
Console.WriteLine("BUNNY_GLOBAL_ITEM_CONTRACT=PASS globals=" + globals.Length + " lines=" + (lines.Length - 1));
