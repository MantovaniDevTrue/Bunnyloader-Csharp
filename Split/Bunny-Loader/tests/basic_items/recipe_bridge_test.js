const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const descriptors=[{id:'mod/ExampleItem#0',result:6196,stack:999,station:18,ingredients:[{type:2,count:10}]},
 {id:'mod/ExamplePickaxe#0',result:6197,stack:1,station:18,ingredients:[{type:6196,count:10}]}];
let failLookup=false,failAck=false,lookups=0,logs=[],ack=new Map(),registrations=[];
const api={pendingItemRecipes:()=>descriptors.filter(r=>!ack.has(r.id)),acknowledgeItemRecipe(id,index){if(failAck)return false;ack.set(id,index);return true;}};
class FakeRecipe {
 constructor(){this.recipe={createItem:{},requiredItem:[]};this.ingredients=0;this.station=-1;}
 SetResult(type,stack){this.recipe.createItem={type,stack};return this;}
 AddIngredient(type,count){this.recipe.requiredItem.push({type,stack:count});this.ingredients++;return this;}
 AddTile(type){this.station=type;return this;}
 Register(){registrations.push(this);context.Terraria.Recipe.numRecipes++;context.Loader.Added(registrations.length-1,this.station);}
}
const context={bl:{csharp:api,log:s=>logs.push(s)},Ready:{Add(){}},Hooks:{Once(){}},Safe:{Run(_,f){f();}},
 ModRecipe:FakeRecipe,TileLoader:{ByType:new Map(),SetCraftingStations(){}},
 Terraria:{Recipe:{maxRecipes:3700,numRecipes:0,
  'void CreateRequiredItemQuickLookups()'(){lookups++;if(failLookup)throw new Error('lookup unavailable');},
  'void UpdateMaterialFieldForAllRecipes()'(){},UpdateWhichItemsAreMaterials(){},UpdateWhichItemsAreCrafted(){}},
  GameContent:{ShimmerTransforms:{UpdateRecipeSets(){}}},ID:{ContentSamples:{FixItemsAfterRecipesAreAdded(){}}}}};
vm.runInNewContext(fs.readFileSync(process.argv[2],'utf8')+'\nglobalThis.Loader=RecipeLoader;',context);
failLookup=true;assert.throws(()=>context.__blSyncCSharpRecipes(),/lookup unavailable/);
assert.equal(registrations.length,2);assert.equal(ack.size,0,'no success ACK before lookups work');
failLookup=false;context.__blSyncCSharpRecipes();assert.equal(registrations.length,2,'retry must not duplicate recipes');
assert.equal(ack.size,2);assert.equal(lookups,2);assert.equal(registrations[0].recipe.createItem.stack,999);
assert.equal(registrations[1].recipe.requiredItem[0].type,6196);assert.equal(registrations[1].station,18);
context.__blSyncCSharpRecipes();assert.equal(registrations.length,2,'dedup after ACK');
descriptors.push({id:'mod/ExampleHamaxe#0',result:6198,stack:1,station:18,ingredients:[{type:6196,count:10}]});
failAck=true;assert.throws(()=>context.__blSyncCSharpRecipes(),/confirmacao nativa recusada/);assert.equal(registrations.length,3);
failAck=false;context.__blSyncCSharpRecipes();assert.equal(registrations.length,3);assert.equal(ack.get('mod/ExampleHamaxe#0'),2);
console.log('BASIC_RECIPE_BRIDGE_PASS=YES');
