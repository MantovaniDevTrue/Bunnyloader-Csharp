const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let lookups=0,stations=0;
const context={bl:{csharp:new Proxy({},{get(){throw Error('JS recipes cannot register CSharp content');}}),log(){}},
 Hooks:{Once(){}},Ready:{Add(){throw Error('CSharp recipe jobs must not be installed');}},
 TileLoader:{ByType:new Map(),SetCraftingStations(){stations++;}},
 Terraria:{Recipe:{maxRecipes:3700,numRecipes:3701,
  'void CreateRequiredItemQuickLookups()'(){lookups++;},'void UpdateMaterialFieldForAllRecipes()'(){},UpdateWhichItemsAreMaterials(){},UpdateWhichItemsAreCrafted(){}},
  GameContent:{ShimmerTransforms:{UpdateRecipeSets(){}}},ID:{ContentSamples:{FixItemsAfterRecipesAreAdded(){}}}}};
vm.runInNewContext(fs.readFileSync(process.argv[2],'utf8')+'\nglobalThis.Loader=RecipeLoader;',context);
assert.equal(context.__blSyncCSharpRecipes,undefined);assert.equal(context.Loader.SyncCSharp,undefined);
context.Loader.Added(3700,18);context.Loader.Finish(true);assert.equal(lookups,1);assert.equal(stations,1);
console.log('JS_RECIPE_LOOKUPS_PRESERVED_WITHOUT_CSHARP_BRIDGE=PASS');
