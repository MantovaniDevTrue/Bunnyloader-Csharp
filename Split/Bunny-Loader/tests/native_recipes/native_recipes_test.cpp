#include "content/recipes/NativeRecipes.h"
#include "content/items/ItemRegistryInternal.h"
#include "content/tiles/TileRegistryInternal.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

struct Il2CppClass { std::string ns,name; unsigned size=0; bool value=false; Il2CppClass* element=nullptr; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { std::string name; Il2CppType type; size_t offset; unsigned flags; void* storage=nullptr; };
struct MethodInfo { std::string signature; bool instance; };
struct Item {
 Il2CppObject obj; int type=0, stack=1, maxStack=9999; unsigned sentinel=0x12345678;
 int width=0,height=0,value=0,rare=0,damage=0,crit=0,useTime=0,useAnimation=0,useStyle=0,pick=0,axe=0,hammer=0;
 bool flags[7]{};float knockBack=0;void* sound=nullptr; int16_t createTile=-1;uint16_t placementSentinel=0x5a5a;int16_t placeStyle=0;uint16_t styleSentinel=0xa5a5;
};
struct Recipe { Il2CppObject obj; Il2CppObject* result=nullptr; Il2CppArray* ingredients=nullptr; int station=-1; };
Il2CppClass intClass{"System","Int32",4,true},floatClass{"System","Single",4,true},
    shortClass{"System","Int16",2,true},boolClass{"System","Boolean",1,true},byteClass{"System","Byte",1,true},legacyClass{"Terraria.Audio","LegacySoundStyle"},
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
namespace bl::hook { bool install(const MethodInfo*,void*,void**) { return false; } }
namespace bl::il2cpp {
Il2CppClass* findNested(Il2CppClass*,std::string_view){return nullptr;}
Il2CppClass* findClassQuiet(const std::string&,const std::string&){return nullptr;}

Api& api(){return apiInstance;}
Il2CppClass* findClass(const TypeRef& r){for(auto* cls:{&itemClass,&recipeClass,&mainClass,&shimmerClass,&samplesClass})if(cls->ns==r.ns&&cls->name==r.name)return cls;return nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){const auto key=cls->name+"."+std::string(name);for(auto& f:fields)if(f.name==key)return &f;return nullptr;}
int32_t fieldOffset(Il2CppClass* cls,std::string_view name){auto* f=findField(cls,name);return f?static_cast<int32_t>(f->offset):-1;}
Signature parseSignature(std::string_view s){Signature out;out.name=std::string(s);return out;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& s,bool*,const MethodInfo**){auto it=methods.find(s.name);return it==methods.end()?nullptr:&it->second;}
std::string describeMethod(const MethodInfo* m){return m->signature;}
}
namespace bl::csharp {
bool managedPrefixDescriptorReady(int) { return false; }
bool managedPrefixDescriptorPending(int) { return false; }
bool applyManagedPrefixStatFallback(Il2CppObject*, int, std::string*) { return false; }
}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::runtime {
void setModItemTooltip(int type,std::vector<std::pair<std::string,std::string>> text){
 std::lock_guard lock(item_registry::g_mx);item_registry::g_regs.at(type-kVanillaItemCount).def.tooltips=std::move(text);
}
void finishModItem(Il2CppObject*,int){}
}
namespace bl::csharp {void refreshManagedPrefixEligibilityOnWorldReady() {}}
void configure(){
 fields={
  {"Recipe.currentRecipe",{&recipeClass},0,0x10,&current},{"Recipe.numRecipes",{&intClass},0,0x10,&numRecipes},
  {"Recipe.maxRecipes",{&intClass},0,0x10,&maxRecipes},{"Main.recipe",{&recipeArray},0,0x10,&table},
  {"Recipe.createItem",{&itemClass},offsetof(Recipe,result),0},{"Recipe.requiredItem",{&itemArray},offsetof(Recipe,ingredients),0},
  {"Item.type",{&intClass},offsetof(Item,type),0},{"Item.stack",{&intClass},offsetof(Item,stack),0},
  {"Item.maxStack",{&intClass},offsetof(Item,maxStack),0}};
 const std::pair<const char*,size_t> extra[]={
  {"width",offsetof(Item,width)},{"height",offsetof(Item,height)},{"value",offsetof(Item,value)},
  {"rare",offsetof(Item,rare)},{"damage",offsetof(Item,damage)},{"crit",offsetof(Item,crit)},
  {"useTime",offsetof(Item,useTime)},{"useAnimation",offsetof(Item,useAnimation)},{"useStyle",offsetof(Item,useStyle)},
  {"pick",offsetof(Item,pick)},{"axe",offsetof(Item,axe)},{"hammer",offsetof(Item,hammer)}};
 for(const auto& [name,off]:extra)fields.push_back({"Item."+std::string(name),{std::string(name)=="useStyle"?&byteClass:&intClass},off,0});
 const char* flags[]={"melee","noMelee","autoReuse","useTurn","consumable","accessory","vanity"};
 for(unsigned i=0;i<7;i++)fields.push_back({"Item."+std::string(flags[i]),{&boolClass},offsetof(Item,flags)+i,0});
 fields.push_back({"Item.knockBack",{&floatClass},offsetof(Item,knockBack),0});
 fields.push_back({"Item.UseSound",{&legacyClass},offsetof(Item,sound),0});
 fields.push_back({"Item.createTile",{&shortClass},offsetof(Item,createTile),0});
 fields.push_back({"Item.placeStyle",{&shortClass},offsetof(Item,placeStyle),0});
 for(const char* s:{"void .ctor()","void SetDefaults(int Type, ItemVariant variant)","void SetCraftingStation(int tileType)",
   "void AddRecipe()","void CreateRequiredItemQuickLookups()","void UpdateMaterialFieldForAllRecipes()",
   "void UpdateWhichItemsAreMaterials()","void UpdateWhichItemsAreCrafted()","void UpdateRecipeSets()","void FixItemsAfterRecipesAreAdded()"})
   methods.emplace(s,MethodInfo{s,std::string(s)=="void .ctor()"||std::string(s)=="void SetDefaults(int Type, ItemVariant variant)"||std::string(s)=="void SetCraftingStation(int tileType)"});
 apiInstance.class_get_name=[](Il2CppClass* c){return c->name.c_str();};
 apiInstance.class_get_namespace=[](Il2CppClass* c){return c->ns.c_str();};
 apiInstance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 apiInstance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 apiInstance.class_is_enum=[](const Il2CppClass*){return false;};
 apiInstance.class_get_fields=[](Il2CppClass* c,void** it)->FieldInfo*{
  auto i=reinterpret_cast<uintptr_t>(*it);
  for(;i<fields.size();++i)if(fields[i].name.rfind(c->name+".",0)==0){*it=reinterpret_cast<void*>(i+1);return &fields[i];}
  return nullptr;
 };
 apiInstance.class_instance_size=[](Il2CppClass* c){return c->size;};
 apiInstance.class_value_size=[](Il2CppClass* c,uint32_t* alignment){*alignment=4;return static_cast<int>(c->size);};
 apiInstance.field_get_type=[](FieldInfo* f){return static_cast<const Il2CppType*>(&f->type);};
 apiInstance.field_get_flags=[](FieldInfo* f){return f->flags;};
 apiInstance.field_get_offset=[](FieldInfo* f){return f->offset;};
 apiInstance.field_static_get_value=[](FieldInfo* f,void* out){assert(f->storage);std::memcpy(out,f->storage,f->type.cls==&intClass?4:sizeof(void*));};
 apiInstance.field_static_set_value=[](FieldInfo* f,void* in){
  assert(f->storage);
  if(f->type.cls==&intClass)std::memcpy(f->storage,in,sizeof(int));
  else {
   // Match IL2CPP's reference ABI, unlike field_static_get_value's output address.
   assert(!in||std::find(allocated.begin(),allocated.end(),in)!=allocated.end());
   *static_cast<void**>(f->storage)=in;
  }
 };
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
   if(bl::csharp::isBasicItem(item->type))assert(bl::csharp::applyBasicItemToObject(&item->obj,item->type));
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
 if(mode=="--probe-wire") {
  assert(argc==3);std::ifstream file(argv[2]);std::string line;
  assert(std::getline(file,line)&&line=="BUNNY_MANAGED_DEFAULTS_V2");
  std::vector<bl::csharp::BasicItemEntry> entries;
  while(std::getline(file,line)) {
   if(line.empty())continue;
   bl::csharp::BasicItemEntry entry;assert(bl::csharp::parseBasicItemWire(bl::csharp::basicSplit(line,'\t'),&entry,&error));
   ModItemDef def;def.mod="probe";def.name=entry.name;def.texture="probe.png";entry.type=registerModItem(def);
   assert(bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error,"/probe",entry.name+".cs"));entries.push_back(entry);
  }
  assert(entries.size()==3);item_registry::g_installed.store(5);numRecipes=1;
  assert(bl::csharp::applyNativeBasicItemDefaults("probe",entries,&error));bl::csharp::pumpBasicRecipesOnGameThread();
  assert(bl::csharp::pendingBasicRecipes().empty()&&numRecipes==4&&commits==3);
  auto** slots=static_cast<Recipe**>(arrayData(table));
  for(int i=1;i<4;++i){auto* item=reinterpret_cast<Item*>(slots[i]->result);
   auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& entry){return entry.type==item->type;});assert(it!=entries.end());
   assert(item->createTile==it->placeTile&&item->placeStyle==it->placeStyle&&item->stack==10&&item->maxStack==9999);
   assert(item->useStyle==1&&item->useTime==10&&item->useAnimation==15&&item->flags[2]&&item->flags[3]&&item->flags[4]);
   assert(item->placementSentinel==0x5a5a&&item->styleSentinel==0xa5a5);
   Item restored;assert(bl::csharp::applyBasicItemToObject(&restored.obj,it->type));
   assert(restored.createTile==it->placeTile&&restored.placeStyle==it->placeStyle);
   auto** ingredients=static_cast<Item**>(arrayData(slots[i]->ingredients));assert(ingredients[0]->type==2&&ingredients[0]->stack==1);
  }
  assert(current==saved&&roots.empty());std::cout<<"ANDROID_PROBE_COMPILED_CSHARP_WIRE_NATIVE_REGISTRATION_DEFAULTS_RECIPES_PASS\n";return 0;
 }
 if(mode=="--placement-wire") {
  assert(argc==3);std::ifstream file(argv[2]);std::string line;
  assert(std::getline(file,line)&&line=="BUNNY_MANAGED_DEFAULTS_V2");assert(std::getline(file,line));
  bl::csharp::BasicItemEntry entry;assert(bl::csharp::parseBasicItemWire(bl::csharp::basicSplit(line,'\t'),&entry,&error));
  assert(entry.name=="ContractPlaceable"&&entry.placeable&&entry.placeTileKey=="other/Workbench"&&entry.placeStyle==3);
  ModItemDef def;def.mod="contract";def.name=entry.name;def.texture="contract.png";
  entry.type=registerModItem(def);item_registry::g_installed.store(3);numRecipes=1;
  assert(bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error,"/contract","ContractPlaceable.cs"));
  assert(bl::csharp::applyNativeBasicItemDefaults("contract",{entry},&error));
  bl::csharp::pumpBasicRecipesOnGameThread();Item before;assert(bl::csharp::applyBasicItemToObject(&before.obj,entry.type));assert(before.createTile==-1);
  auto tile=registerModTile({"other","Workbench","bench.png"});tile_registry::g_installed.store(3);
  def.name="OtherMaterial";auto ingredient=registerModItem(def);item_registry::g_installed.store(4);
  for(int i=0;i<120;i++)bl::csharp::pumpBasicRecipesOnGameThread();
  assert(bl::csharp::pendingBasicRecipes().empty()&&numRecipes==2);
  auto** slots=static_cast<Recipe**>(arrayData(table));auto* preview=reinterpret_cast<Item*>(slots[1]->result);
  assert(preview->createTile==tile&&preview->placeStyle==3&&preview->type==entry.type);
  assert(preview->width==28&&preview->height==14&&preview->useStyle==1&&preview->useTime==10&&preview->useAnimation==15);
  assert(preview->flags[2]&&preview->flags[3]&&preview->flags[4]);
  assert(preview->placementSentinel==0x5a5a&&preview->styleSentinel==0xa5a5);
  auto** parts=static_cast<Item**>(arrayData(slots[1]->ingredients));assert(parts[1]->type==ingredient&&parts[1]->stack==10);
  assert(current==saved&&roots.empty());
  std::cout<<"COMPILED_CSHARP_PLACEMENT_WIRE_TO_PRODUCTION_NATIVE_QUEUE_WITHOUT_QUICKJS_PASS\n";return 0;
 }
 if(mode=="--pump") {
  numRecipes=1;
  bl::csharp::BasicItemEntry own;own.type=kVanillaItemCount;own.name="Material";own.values[bl::csharp::BasicMaxStack]=9999;
  own.values[bl::csharp::BasicWidth]=28;own.values[bl::csharp::BasicHeight]=14;own.values[bl::csharp::BasicUseStyle]=5;
  own.recipes.push_back({1,18,{{"v2",10,2}},""});
  own.placeable=true;own.placeTileKey="Bench";own.placeStyle=3;
  assert(bl::csharp::registerBasicItemPlaceholder(own.type,own.name,&error,"/cs","Material.cs"));
  assert(bl::csharp::applyNativeBasicItemDefaults("cs",{own},&error));
  auto broken=own;broken.type=laterId;broken.placeTileKey="late/Bench";broken.recipes[0].station=-1;broken.recipes[0].stationKey="late/Bench";
  assert(bl::csharp::registerBasicItemPlaceholder(broken.type,broken.name,&error,"/later","Material.cs"));
  assert(bl::csharp::applyNativeBasicItemDefaults("later",{broken},&error));
  bl::csharp::pumpBasicRecipesOnGameThread();
  auto pending=bl::csharp::pendingBasicRecipes();assert(pending.size()==1&&pending[0].modId=="later");
  assert(pending[0].error.find("MissingDependency")!=std::string::npos);assert(numRecipes==2&&commits==1);
  auto** slots=static_cast<Recipe**>(arrayData(table));auto* preview=reinterpret_cast<Item*>(slots[1]->result);
  assert(preview->createTile==bench&&preview->placeStyle==3);
  assert(preview->placementSentinel==0x5a5a&&preview->styleSentinel==0xa5a5);
  Item disabled;assert(bl::csharp::applyBasicItemToObject(&disabled.obj,laterId));assert(disabled.createTile==-1);
  assert(preview->width==28&&preview->height==14&&preview->maxStack==9999&&preview->useStyle==5);
  assert(registerModTile({"late","Bench","late.png"})==kVanillaTileCount+2);tile_registry::g_installed.store(3);
  for(int i=0;i<120;i++)bl::csharp::pumpBasicRecipesOnGameThread();
  assert(bl::csharp::pendingBasicRecipes().empty()&&numRecipes==3&&commits==2&&roots.empty());
  assert(slots[2]->station==kVanillaTileCount+2);
  auto* restored=reinterpret_cast<Item*>(slots[2]->result);
  assert(restored->createTile==kVanillaTileCount+2&&restored->placeStyle==3);
  assert(restored->placementSentinel==0x5a5a&&restored->styleSentinel==0xa5a5);
  std::cout<<"CSHARP_PRODUCTION_QUEUE_PUMP_REGISTRY_DEFAULTS_RECIPE_AND_LATE_DEPENDENCY_WITHOUT_QUICKJS_PASS\n";return 0;
 }
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
