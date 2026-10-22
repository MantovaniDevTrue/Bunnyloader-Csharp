using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp;

static void Eq<T>(T expected, T actual, string name)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new InvalidOperationException($"{name}: esperado={expected}, atual={actual}");
}

var campfire = new ExampleCampfire();
campfire.SetDefaults();
Eq("mExampleCampfire:0", campfire.PlacementDefinition, "ExampleCampfire.createTile");
Eq(9999, campfire.MaxStack, "ExampleCampfire.maxStack");
campfire.AddRecipes();
Eq("1,-1,gWood:10,mExampleTorch:5", campfire.RecipeDefinitions.Single(),
    "ExampleCampfire.AddRecipeGroup/AddIngredient");

var herb = new ExampleHerbSeeds();
herb.SetDefaults();
Eq("mExampleHerb:0", herb.PlacementDefinition, "ExampleHerbSeeds.createTile");
Eq(12, herb.Width, "ExampleHerbSeeds.width");
Eq(14, herb.Height, "ExampleHerbSeeds.height");
Eq(80, herb.Value, "ExampleHerbSeeds.value");
herb.AddRecipes();
Eq("1,-1,mExampleTileItem:1", herb.RecipeDefinitions.Single(),
    "ExampleHerbSeeds.AddRecipes");

var lamp = new ExampleLamp();
lamp.SetDefaults();
Eq("mExampleLamp:0", lamp.PlacementDefinition, "ExampleLamp.createTile");
Eq(10, lamp.Width, "ExampleLamp.width");
Eq(24, lamp.Height, "ExampleLamp.height");
Eq(500, lamp.Value, "ExampleLamp.value");
lamp.AddRecipes();
Eq("1,mExampleWorkbench,mExampleItem:1", lamp.RecipeDefinitions.Single(),
    "ExampleLamp.AddRecipes");

var sink = new ExampleSinkItem();
sink.SetDefaults();
Eq("mExampleSink:0", sink.PlacementDefinition, "ExampleSinkItem.createTile");
Eq(24, sink.Width, "ExampleSinkItem.width");
Eq(30, sink.Height, "ExampleSinkItem.height");
Eq(3000, sink.Value, "ExampleSinkItem.value");
sink.AddRecipes();
Eq("1,18,mExampleItem:1", sink.RecipeDefinitions.Single(), "ExampleSinkItem.AddRecipes");

Directory.CreateDirectory("build/placeable-stage");
var diagnostic = "build/placeable-stage/probe.txt";
var count = CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location, diagnostic);
if (count != 7)
    throw new InvalidOperationException("Native content serialization failed: " +
        (File.Exists(diagnostic) ? File.ReadAllText(diagnostic) : "missing probe"));
Console.WriteLine("EXAMPLE_PLACEABLE_ITEMS_SOURCE=PASS; four original items parsed without enabling incomplete tiles");
