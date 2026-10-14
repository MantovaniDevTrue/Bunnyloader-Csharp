#include "content/recipes/NativeRecipes.h"
#include "content/items/ItemRegistryInternal.h"
#include "content/tiles/TileRegistryInternal.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

struct Il2CppClass { std::string ns,name; unsigned size=0; bool value=false; Il2CppClass* element=nullptr; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { std::string name; Il2CppType type; size_t offset; unsigned flags; void* storage=nullptr; };
struct MethodInfo { std::string signature; bool instance; };
struct Item { Il2CppObject obj; int type=0, stack=1, maxStack=9999; unsigned sentinel=0x12345678; };
struct Recipe { Il2CppObject obj; Il2CppObject* result=nullptr; Il2CppArray* ingredients=nullptr; int station=-1; };
Il2CppClass intClass{"System","Int32",4,true},floatClass{"System","Single",4,true},
    itemClass{"Terraria","Item",sizeof(Item)},recipeClass{"Terraria","Recipe",sizeof(Recipe)},
    mainClass{"Terraria","Main"},itemArray{"Terraria","Item[]",0,false,&itemClass},
    recipeArray{"Terraria","Recipe[]",0,false,&recipeClass},variantClass{"Terraria","ItemVariant",4,true},
    shimmerClass{"Terraria.GameContent","ShimmerTransforms"},samplesClass{"Terraria.ID","ContentSamples"},
    exceptionClass{"System","InvalidOperationException",sizeof(Il2CppObject)};
bl::il2cpp::Api apiInstance;
std::vector<FieldInfo> fields;
std::map<std::string,MethodInfo> methods;
std::map<unsigned,Il2CppObject*> roots;
unsigned nextRoot=1;int numRecipes=0,maxRecipes=16,lookups=0,commits=0,barriers=0;
Il2CppObject* current=nullptr;Il2CppArray* table=nullptr;
bool failDefaults=false,failAdd=false,failLookup=false,wrongArray=false,failedAllocation=false;
Il2CppObject exception{&exceptionClass,nullptr};
std::vector<void*> allocated;
Il2CppArray* makeArray(Il2CppClass* cls,unsigned length){
 auto* p=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+length*sizeof(void*)));
 allocated.push_back(p);p->obj.klass=cls;p->length=length;return p;
}
Item* makeItem(){auto* p=new Item;p->obj.klass=&itemClass;allocated.push_back(p);return p;}
Recipe* makeRecipe(){auto* p=new Recipe;p->obj.klass=&recipeClass;allocated.push_back(p);return p;}
namespace bl::il2cpp {
Api& api(){return apiInstance;}
Il2CppClass* findClass(const TypeRef& r){for(auto* cls:{&itemClass,&recipeClass,&mainClass,&shimmerClass,&samplesClass})if(cls->ns==r.ns&&cls->name==r.name)return cls;return nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){const auto key=cls->name+"."+std::string(name);for(auto& f:fields)if(f.name==key)return &f;return nullptr;}
Signature parseSignature(std::string_view s){Signature out;out.name=std::string(s);return out;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& s,bool*,const MethodInfo**){auto it=methods.find(s.name);return it==methods.end()?nullptr:&it->second;}
std::string describeMethod(const MethodInfo* m){return m->signature;}
}
void configure(){
 fields={
  {"Recipe.currentRecipe",{&recipeClass},0,0x10,&current},{"Recipe.numRecipes",{&intClass},0,0x10,&numRecipes},
  {"Recipe.maxRecipes",{&intClass},0,0x10,&maxRecipes},{"Main.recipe",{&recipeArray},0,0x10,&table},
  {"Recipe.createItem",{&itemClass},offsetof(Recipe,result),0},{"Recipe.requiredItem",{&itemArray},offsetof(Recipe,ingredients),0},
  {"Item.type",{&intClass},offsetof(Item,type),0},{"Item.stack",{&intClass},offsetof(Item,stack),0},
  {"Item.maxStack",{&intClass},offsetof(Item,maxStack),0}};
 for(const char* s:{"void .ctor()","void SetDefaults(int Type, ItemVariant variant)","void SetCraftingStation(int tileType)",
   "void AddRecipe()","void CreateRequiredItemQuickLookups()","void UpdateMaterialFieldForAllRecipes()",
   "void UpdateWhichItemsAreMaterials()","void UpdateWhichItemsAreCrafted()","void UpdateRecipeSets()","void FixItemsAfterRecipesAreAdded()"})
   methods.emplace(s,MethodInfo{s,std::string(s)=="void .ctor()"||std::string(s)=="void SetDefaults(int Type, ItemVariant variant)"||std::string(s)=="void SetCraftingStation(int tileType)"});
 apiInstance.class_get_name=[](Il2CppClass* c){return c->name.c_str();};
 apiInstance.class_get_namespace=[](Il2CppClass* c){return c->ns.c_str();};
 apiInstance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 apiInstance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 apiInstance.class_instance_size=[](Il2CppClass* c){return c->size;};
 apiInstance.class_value_size=[](Il2CppClass* c,uint32_t* alignment){*alignment=4;return static_cast<int>(c->size);};
 apiInstance.field_get_type=[](FieldInfo* f){return static_cast<const Il2CppType*>(&f->type);};
 apiInstance.field_get_flags=[](FieldInfo* f){return f->flags;};
 apiInstance.field_get_offset=[](FieldInfo* f){return f->offset;};
 apiInstance.field_static_get_value=[](FieldInfo* f,void* out){assert(f->storage);std::memcpy(out,f->storage,f->type.cls==&intClass?4:sizeof(void*));};
 apiInstance.field_static_set_value=[](FieldInfo* f,void* in){assert(f->storage);std::memcpy(f->storage,in,f->type.cls==&intClass?4:sizeof(void*));};
 apiInstance.runtime_class_init=[](Il2CppClass*){};
 apiInstance.class_get_rank=[](const Il2CppClass* c){return c->element?1:0;};
 apiInstance.class_get_element_class=[](Il2CppClass* c){return c->element;};
 apiInstance.method_is_instance=[](const MethodInfo* m){return m->instance;};
 apiInstance.method_get_param=[](const MethodInfo*,unsigned){static Il2CppType t{&variantClass};return static_cast<const Il2CppType*>(&t);};
 apiInstance.object_get_class=[](Il2CppObject* o){return o->klass;};
 apiInstance.object_new=[](Il2CppClass* cls)->Il2CppObject*{assert(cls==&recipeClass);if(failedAllocation)return nullptr;return &makeRecipe()->obj;};
 apiInstance.gchandle_new=[](Il2CppObject* obj,bool){const auto h=nextRoot++;roots[h]=obj;return h;};
 apiInstance.gchandle_free=[](unsigned h){assert(roots.erase(h)==1);};
 apiInstance.gc_wbarrier_set_field=[](Il2CppObject*,void** field,void* value){*field=value;barriers++;};
 apiInstance.runtime_invoke=[](const MethodInfo* m,void* self,void** args,Il2CppObject** ex)->Il2CppObject*{
  *ex=nullptr;
  if(m->signature=="void .ctor()"){
   auto* r=static_cast<Recipe*>(self);r->result=&makeItem()->obj;r->ingredients=makeArray(wrongArray?&recipeArray:&itemArray,15);
   auto** slots=static_cast<Il2CppObject**>(arrayData(r->ingredients));for(int i=0;i<15;i++)slots[i]=&makeItem()->obj;
  }else if(m->signature=="void SetDefaults(int Type, ItemVariant variant)"){
   assert(*static_cast<int*>(args[1])==0);auto* item=static_cast<Item*>(self);item->type=*static_cast<int*>(args[0]);
   if(item->type==3)item->maxStack=1;
   if(failDefaults)*ex=&exception;
  }else if(m->signature=="void SetCraftingStation(int tileType)")static_cast<Recipe*>(self)->station=*static_cast<int*>(args[0]);
  else if(m->signature=="void AddRecipe()"){
   assert(!self);auto** slots=static_cast<Il2CppObject**>(arrayData(table));slots[numRecipes++]=current;commits++;
   current=&makeRecipe()->obj;if(failAdd)*ex=&exception;
  }else{lookups++;if(failLookup)*ex=&exception;}
  return nullptr;
 };
 table=makeArray(&recipeArray,16);current=&makeRecipe()->obj;
}
using namespace bl::runtime;
NativeRecipeRequest request(const std::string& name){NativeRecipeRequest r;r.key="cs/"+name+"#0";r.mod="cs";r.name=name;r.source="content/"+name+".cs";r.result=kVanillaItemCount;r.station=18;r.ingredients={{2,10,{}}};return r;}
int main(int argc,char** argv){
 configure();
 ModItemDef item;item.mod="cs";item.name="Material";item.texture="asset.png";
 assert(registerModItem(item)==kVanillaItemCount);assert(registerModItem(item)==-1);
 item_registry::g_installed.store(1);
 assert(modItemKey(kVanillaItemCount)=="cs/Material");assert(modItemTypeByKey("cs/Material")==kVanillaItemCount);
 ModTileDef station{"cs","Bench","bench.png"};const auto bench=registerModTile(station);
 assert(bench==kVanillaTileCount);assert(registerModTile(station)==-1);assert(modTileKey(bench)=="cs/Bench");
 auto r=request("Armor");NativeRecipeRequest resolved;std::string error;r.station=-1;r.stationKey="Bench";r.ingredients={{-1,4,"Material"}};
 assert(!resolveNativeRecipe(r,&resolved,&error));assert(error.find("DependencyNotInstalled")!=std::string::npos);
 tile_registry::g_installed.store(1);assert(resolveNativeRecipe(r,&resolved,&error));assert(resolved.station==bench);assert(resolved.ingredients[0].type==kVanillaItemCount);
 r.stationKey="other/Bench";assert(!resolveNativeRecipe(r,&resolved,&error));assert(error.find("other/Bench")!=std::string::npos);
 assert(registerModTile({"other","Bench","other.png"})==bench+1);tile_registry::g_installed.store(2);
 assert(resolveNativeRecipe(r,&resolved,&error));assert(resolved.station==bench+1);
 r.ingredients={{-1,1,"later/Material"}};assert(!resolveNativeRecipe(r,&resolved,&error));assert(error.find("Stage=ResolveIngredient")!=std::string::npos);
 ModItemDef later;later.mod="later";later.name="Material";const auto laterId=registerModItem(later);item_registry::g_installed.store(2);
 assert(resolveNativeRecipe(r,&resolved,&error));assert(resolved.ingredients[0].type==laterId);
 auto* saved=current;
 const std::string mode=argc>1?argv[1]:"--normal";
 if(mode=="--type")fields[7].type.cls=&floatClass;
 if(mode=="--bounds")fields[7].offset=sizeof(Item)-1;
 if(mode=="--method")methods.erase("void SetCraftingStation(int tileType)");
 if(mode!="--normal"){
  const auto result=installNativeRecipe(r);assert(result.index<0);assert(result.error.find("NativeContractMismatch")!=std::string::npos);
  assert(result.error.find(mode=="--type"?"Actual=System.Single":mode=="--bounds"?"InvalidFieldBounds":"MissingOrIncompatibleSignature")!=std::string::npos);
  assert(numRecipes==0&&current==saved&&roots.empty());std::cout<<"NATIVE_RECIPE_CONTRACT_REJECTION_PASS "<<mode<<'\n';return 0;
 }
 auto result=installNativeRecipe(r);assert(result.index<0&&result.error.find("RecipeSetupNotReady")!=std::string::npos);assert(numRecipes==0&&commits==0);
 numRecipes=1; // A completed vanilla SetupRecipes provides the first recipe.
 result=installNativeRecipe(r);assert(result.index==1&&result.error.empty());assert(current==saved&&numRecipes==2&&roots.empty());
 auto** slots=static_cast<Recipe**>(arrayData(table));assert(slots[1]->station==bench+1);
 assert(reinterpret_cast<Item*>(slots[1]->result)->type==r.result);
 auto** ingredients=static_cast<Item**>(arrayData(slots[1]->ingredients));assert(ingredients[0]->type==laterId);
 assert(ingredients[0]->sentinel==0x12345678);assert(lookups==6);
 result=installNativeRecipe(r);assert(result.index==1&&commits==1);
 r=request("RetryLookups");failLookup=true;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("RebuildLookups")!=std::string::npos);assert(numRecipes==3&&roots.empty());
 failLookup=false;result=installNativeRecipe(r);assert(result.index==2&&numRecipes==3&&commits==2);
 r=request("BadAdd");failAdd=true;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("System.InvalidOperationException")!=std::string::npos);assert(numRecipes==3&&slots[3]==nullptr&&current==saved&&roots.empty());
 failAdd=false;result=installNativeRecipe(r);assert(result.index==3&&numRecipes==4);
 r=request("BadDefaults");failDefaults=true;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("Stage=SetResult")!=std::string::npos);assert(numRecipes==4&&roots.empty());failDefaults=false;
 r=request("BadArray");wrongArray=true;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("InvalidArrayLayout")!=std::string::npos);assert(numRecipes==4);wrongArray=false;
 r=request("BadAllocation");failedAllocation=true;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("AllocationFailed")!=std::string::npos);assert(numRecipes==4);failedAllocation=false;
 r=request("OversizeStack");r.ingredients={{3,2,{}}};result=installNativeRecipe(r);assert(result.index<0&&result.error.find("MaxStack=1")!=std::string::npos);assert(numRecipes==4);
 r=request("Missing");r.stationKey="missing/Station";result=installNativeRecipe(r);assert(result.index<0&&result.error.find("MissingDependency")!=std::string::npos);
 r=request("Independent");result=installNativeRecipe(r);assert(result.index==4&&numRecipes==5);
 r=request("InvalidVanillaStation");r.station=754;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("InvalidVanillaTile")!=std::string::npos);
 r=request("Limit");maxRecipes=numRecipes;result=installNativeRecipe(r);assert(result.index<0&&result.error.find("NativeRecipeLimitReached")!=std::string::npos);assert(current==saved&&roots.empty());
 assert(barriers>0);std::cout<<"NATIVE_RECIPES_WITHOUT_QUICKJS_PASS registry,station,intermod,load-order,rollback,lookups,types,bounds,allocation,isolation,capacity\n";
}
