using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp;

static void Equal<T>(T expected, T actual, string field)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new InvalidOperationException($"{field}: esperado={expected}, atual={actual}");
}

var tile = new ExampleTile();
tile.SetStaticDefaults();
Equal("ExampleTile", tile.DisplayName, "nome do tile");
Equal("Content/Tiles/ExampleTile.png", tile.Texture, "textura do tile");
Equal("Stone", tile.DustName, "DustID.Stone");
Equal(0, tile.MapR, "mapa vermelho");
Equal(200, tile.MapG, "mapa verde");
Equal(255, tile.MapB, "mapa azul");
Equal(BunnyTileShape.Single, tile.Shape, "tile de um bloco");
Equal(
    BunnyTileFlags.Solid | BunnyTileFlags.BlockLight |
    BunnyTileFlags.MergeDirt | BunnyTileFlags.ChecksForMerge,
    tile.Flags, "flags IL2CPP do tile");

var item = new ExampleTileItem();
item.SetDefaults();
Equal("Content/Items/Placeable/ExampleTileItem.png", item.Texture, "textura do item");
Equal(12, item.Width, "largura");
Equal(12, item.Height, "altura");
Equal("mExampleTile:0", item.PlacementDefinition, "identidade do tile");
Equal(9999, item.MaxStack, "pilha de blocos");
Equal(true, item.Consumable, "consumo por colocacao");
item.AddRecipes();
Equal("10,18,mExampleItem:1", item.RecipeDefinitions.Single(), "receita original");

var material = new ExampleItem();
material.SetDefaults();
Equal(9999, material.MaxStack, "pilha do material");

Directory.CreateDirectory("build/tile-parity");
var report = "build/tile-parity/compiled-source.txt";
var verified = CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location, report);
if (verified != 3)
    throw new InvalidOperationException("Falha no registro fonte C#: " +
        (File.Exists(report) ? File.ReadAllText(report) : "sem diagnostico"));

var values = File.ReadAllText("build/tile-parity/source-runtime-values.txt");
if (!values.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
    !values.Contains("TILE\tExampleTile\t", StringComparison.Ordinal) ||
    !values.Contains("ITEM\tExampleTileItem\t", StringComparison.Ordinal))
    throw new InvalidOperationException("Metadados nativos V2 incompletos");

Console.WriteLine("EXAMPLE_TILE_SOURCE_PARITY=PASS; original JS 1x1, placement, receita e registro V2");
