using BunnyLoader.Source;
using ExampleModCSharp;

static void Check(bool success, string reason)
{
    if (!success) throw new InvalidOperationException(reason);
}

var basic = new ExampleModRarity();
var higher = new ExampleHigherTierModRarity();
Check(basic.RarityColor.R == 200 && basic.RarityColor.G == 215 &&
    basic.RarityColor.B == 230, "Base rarity must match ExampleModBunny");
Check(higher.RarityColor.R == 127 && higher.RarityColor.G == 204 &&
    higher.RarityColor.B == 170, "Animated rarity must sample Bunny native DiscoColor");

var material = new ExampleItem();
material.SetDefaults();
Check(material.ModRarityClassName.Length == 0,
    "ExampleItem must not declare an additional mod rarity");
var sword = new ExampleMeleeWeapon();
sword.SetDefaults();
Check(sword.ModRarityClassName.Length == 0 && sword.Rare == ItemRarityID.Green,
    "Melee sword must use original vanilla Green rarity");
var accessory = new ExampleStatAccessory();
accessory.SetDefaults();
Check(accessory.ModRarityClassName == nameof(ExampleHigherTierModRarity),
    "Higher-tier accessory rarity is not registered");

var wand = new ExampleWand();
wand.SetDefaults();
Check(wand.ModRarityClassName == nameof(ExampleModRarity),
    "Native weapon rarity is not registered");
var potion = new ExampleBuffPotion();
potion.SetDefaults();
Check(potion.ModRarityClassName.Length == 0 &&
    potion.NativeDefaultsDefinition.Contains("rare:d:ItemRarityID:Orange", StringComparison.Ordinal),
    "Buff potion must use original vanilla Orange rarity");

Console.WriteLine("PLAYABLE_RARITY_EXAMPLES=PASS");

// Runtime uses the persisted UID, not the human-readable manifest slug.
string uid = "68c3115f-62f4-49f2-a56b-388256781d5f";
string assembly = typeof(ExampleModRarity).Assembly.Location;
Check(BunnyRarityRuntime.Dispatch(uid, assembly, "", 0, 2,
    "ExampleModRarity:12;ExampleHigherTierModRarity:13") == "READY|2", "Bind native rarity IDs");
Check(BunnyRarityRuntime.Dispatch(uid, assembly, "ExampleModRarity", 0, 1, "1.2,1") == "RARITY|13",
    "Prefix upgrade resolves the actual mod UID");
Check(BunnyRarityRuntime.Dispatch(uid, assembly, "ExampleHigherTierModRarity", 0, 1, "0.8,-1") == "RARITY|12",
    "Prefix downgrade resolves the actual mod UID");
