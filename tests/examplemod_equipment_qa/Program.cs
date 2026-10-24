using System.Text.Json;
using BunnyLoader.Source;
using ExampleModCSharp;
using ExampleModCSharp.Content.Items.Accessories;

static void Check(bool ok, string reason)
{
    if (!ok) throw new InvalidOperationException(reason);
}

var wings = new ExampleWings();
wings.SetDefaults();
Check(wings.Width == 22 && wings.Height == 20 && wings.Value == 10000 &&
      wings.Rare == ItemRarityID.Green && wings.Accessory, "ExampleWings defaults");
using (var json = JsonDocument.Parse(wings.BehaviorDefinition))
{
    var root = json.RootElement;
    var equip = root.GetProperty("equips")[0];
    Check(equip[0].GetString() == "Wings" &&
          equip[1].GetString() == "Content/Items/Accessories/ExampleWings_Wings", "Wing texture and native slot");
    var stats = root.GetProperty("wings").GetProperty("stats");
    Check(stats[0].GetInt32() == 180 && stats[1].GetSingle() == 9f &&
          stats[2].GetSingle() == 2.5f, "Wing flight stats");
    var vertical = root.GetProperty("wings").GetProperty("vertical");
    float[] expected = { 0.85f, 0.15f, 1f, 3f, 0.135f };
    Check(vertical.GetArrayLength() == expected.Length, "Wing vertical stat count");
    for (int i = 0; i < expected.Length; i++)
        Check(vertical[i].GetSingle() == expected[i], "Wing vertical stat " + i);
}
wings.AddRecipes();
Check(wings.RecipeDefinitions.Count == 1 &&
      wings.RecipeDefinitions[0].Contains("mExampleItem:1", StringComparison.Ordinal) &&
      wings.RecipeDefinitions[0].Contains("mExampleWorkbench", StringComparison.Ordinal), "Wing recipe");

var beard = new ExampleBeard();
beard.SetDefaults();
Check(beard.Width == 18 && beard.Height == 14 && beard.MaxStack == 1 &&
      beard.Value == BunnyItem.SellPrice(gold: 1) && beard.Accessory && beard.Vanity, "Beard defaults and sellPrice");
using (var json = JsonDocument.Parse(beard.BehaviorDefinition))
{
    var root = json.RootElement;
    Check(root.GetProperty("hair").GetBoolean(), "Beard follows player hair color");
    var equip = root.GetProperty("equips")[0];
    Check(equip[0].GetString() == "Beard" &&
          equip[1].GetString() == "Content/Items/Accessories/ExampleBeard_Beard" &&
          equip[2].GetInt32() == (int)BunnyEquipDrawFlags.UseHairColor, "Beard native hair-color slot");
}

var stat = new ExampleStatAccessory();
stat.SetDefaults();
Check(stat.Width == 28 && stat.Height == 28 && stat.Accessory &&
      stat.Value == BunnyItem.SellPrice(gold: 5) &&
      stat.Rare == ItemRarityID.Green, "Stat accessory defaults and sellPrice");
Check(stat.ModRarityClassName.Length == 0, "Original Green rarity must not be replaced by mod rarity");
Check(stat.NativeAccessoryEffectsDefinition ==
      "meleeDamage:mul:1.1;rangedDamage:mul:1.1;magicDamage:mul:1.1",
      "Only melee, ranged and magic gain 10% multiplicative damage");

var sword = new ExampleMeleeWeapon();
sword.SetDefaults();
Check(sword.Melee && sword.Damage == 50 && sword.KnockBack == 6f &&
      sword.Crit == 6 && sword.UseTime == 20 && sword.UseAnimation == 20 &&
      sword.UseStyle == 1 && sword.AutoReuse &&
      sword.Value == BunnyItem.SellPrice(gold: 1) &&
      sword.Rare == ItemRarityID.Green && sword.UseSound == "Item1", "Example sword original defaults");

var bullet = new ExampleBullet();
bullet.SetDefaults();
Check(bullet.Damage == 12 && bullet.MaxStack == 9999 &&
      bullet.Consumable && bullet.KnockBack == 1f &&
      bullet.Value == BunnyItem.SellPrice(silver: 10) &&
      bullet.Rare == ItemRarityID.Green, "Example bullet original defaults");
var defaults = bullet.NativeDefaultsDefinition;
Check(defaults.Contains("ammo:d:AmmoID:Bullet", StringComparison.Ordinal) &&
      defaults.Contains("shoot:p:ExampleBulletProjectile", StringComparison.Ordinal) &&
      defaults.Contains("shootSpeed:f:4.5", StringComparison.Ordinal) &&
      defaults.Contains("ranged:b:1", StringComparison.Ordinal), "Native bullet ammunition and projectile");

Console.WriteLine("EXAMPLEMOD_EQUIPMENT_HOST_QA=PASS wings beard stat sword bullet");
