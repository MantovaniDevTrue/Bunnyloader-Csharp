using BunnyLoader.Source;
using ExampleModCSharp;
using ExampleModCSharp.Items.Placeable;

static void Check(bool ok, string reason)
{
    if (!ok) throw new InvalidOperationException(reason);
}

var material = new ExampleItem();
material.SetDefaults();
Check(material.MaxStack == 9999 && material.Value == BunnyItem.BuyPrice(silver: 1), "ExampleItem stack and buyPrice");
material.AddRecipes();
Check(material.RecipeDefinitions.Count == 1 &&
      material.RecipeDefinitions[0].StartsWith("999,v18,", StringComparison.Ordinal) &&
      material.RecipeDefinitions[0].Contains("v2:10", StringComparison.Ordinal), "Original dirt-to-999-items recipe");

var tile = new ExampleTileItem();
tile.SetDefaults();
Check(tile.PlacementDefinition == "mExampleTile:0" && tile.Width == 12 &&
      tile.Height == 12 && tile.Consumable && tile.MaxStack == 9999,
      "Native ExampleTile placement and original dimensions");
tile.AddRecipes();
Check(tile.RecipeDefinitions.Count == 1 &&
      tile.RecipeDefinitions[0].StartsWith("10,v18,", StringComparison.Ordinal) &&
      tile.RecipeDefinitions[0].Contains("mExampleItem:1", StringComparison.Ordinal),
      "Original 1 ExampleItem to 10 ExampleTile recipe");

var wall = new ExampleWallAdvanced();
wall.SetDefaults();
Check(wall.PlacementDefinition.StartsWith("wmExampleWallAdvanced:0", StringComparison.Ordinal) &&
      wall.MaxStack == 9999, "Native placeable ExampleWallAdvanced registration");
wall.AddRecipes();
Check(wall.RecipeDefinitions.Count == 1 &&
      wall.RecipeDefinitions[0].StartsWith("4,mExampleWorkbench,", StringComparison.Ordinal) &&
      wall.RecipeDefinitions[0].Contains("mExampleItem:1", StringComparison.Ordinal),
      "Original 1 ExampleItem to 4 advanced walls recipe");

var potion = new ExampleBuffPotion();
potion.SetDefaults();
Check(potion.Buff == nameof(ExampleDefenseBuff) && potion.Duration == 5400,
      "ExampleDefenseBuff duration 5400 frames");
var defaults = potion.NativeDefaults.Definition;
Check(defaults.Contains("width:i:18", StringComparison.Ordinal) &&
      defaults.Contains("height:i:26", StringComparison.Ordinal) &&
      defaults.Contains("useStyle:d:ItemUseStyleID:DrinkLiquid", StringComparison.Ordinal) &&
      defaults.Contains("useTime:i:15", StringComparison.Ordinal) &&
      defaults.Contains("useAnimation:i:15", StringComparison.Ordinal) &&
      defaults.Contains("useTurn:b:1", StringComparison.Ordinal) &&
      defaults.Contains("maxStack:i:9999", StringComparison.Ordinal) &&
      defaults.Contains("consumable:b:1", StringComparison.Ordinal) &&
      defaults.Contains("rare:d:ItemRarityID:Orange", StringComparison.Ordinal) &&
      defaults.Contains("value:i:50000", StringComparison.Ordinal) &&
      defaults.Contains("UseSound:s:Item3", StringComparison.Ordinal), "Original drink potion item defaults");
Check(defaults.Contains("ItemSet_DrinkParticleColors:c:FFF0F0F0,FFC8C8C8,FF8C8C8C", StringComparison.Ordinal),
      "Original three drink particle colors");

Console.WriteLine("EXAMPLEMOD_ITEM_RECIPES_HOST_QA=PASS material tile wall potion");
