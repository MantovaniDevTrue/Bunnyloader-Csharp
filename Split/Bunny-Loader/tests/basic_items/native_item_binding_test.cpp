#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

struct Il2CppClass { const char* name; bool value; int size; std::vector<FieldInfo*> fields; Il2CppClass* element=nullptr; int rank=0; Il2CppClass* parent=nullptr; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType* type; size_t offset; uint32_t flags; };
struct FakeItem { Il2CppObject header; int type; int values[13]; bool flags[7]; float knockBack; alignas(void*) unsigned char sound[64]; int16_t defense; unsigned char defenseSentinel[2]; int headSlot; int16_t bodySlot; int16_t legSlot; int8_t wingSlot; int8_t beardSlot; int8_t backSlot; unsigned char sentinel[3]; uint16_t createWall=0;unsigned char wallSentinel[2]={0x5a,0x5a};uint32_t color=0; void* clonedReference=nullptr; };
struct FakePlayer { Il2CppObject header; float meleeDamage=2,rangedDamage=3,magicDamage=4,minionDamage=5,moveSpeed=1; bool strongBees=false; unsigned char sentinel[3]={0x5a,0x5a,0x5a}; int statManaMax2=200,maxMinions=1,statDefense=0,lifeRegen=0; Il2CppArray* buffImmune=nullptr; Il2CppArray* armor=nullptr; uint32_t hairColor=0;int wingsLogic=0;float velocity[2]{},gravDir=1,wingTime=180;int wingTimeMax=180;bool empressBrooch=false; };
struct MethodInfo {void* pointer=nullptr;};
MethodInfo functionalMethod;
MethodInfo wallHelper;
bool wallMode=false;int wallHelperCalls=0;
using EquipCallback=void (*)(Il2CppObject*,int32_t,Il2CppObject*,const MethodInfo*);
EquipCallback functionalHook=nullptr;int originalEquipCalls=0,hookInstalls=0;
bool rejectEquipHook=false;
Il2CppClass* (*additionalClass)(const bl::il2cpp::TypeRef&)=nullptr;
const MethodInfo* (*additionalMethod)(Il2CppClass*,const bl::il2cpp::Signature&)=nullptr;
bool (*additionalInstall)(const MethodInfo*,void*,void**)=nullptr;
void originalEquip(Il2CppObject*,int32_t,Il2CppObject*,const MethodInfo*) {++originalEquipCalls;}
struct SoundValue { int volume; void* path; };
Il2CppClass intClass{"Int32",true,4,{}},boolClass{"Boolean",true,1,{}},floatClass{"Single",true,4,{}},
    legacyClass{"LegacySoundStyle",false,0,{}},stringClass{"String",false,0,{}},
    soundClass{"SoundStyle",true,static_cast<int>(sizeof(SoundValue)),{}},
    nullableClass{"Nullable`1",true,24,{}},itemClass{"Item",false,sizeof(FakeItem),{}},idClass{"SoundID",false,0,{}};
Il2CppType intType{&intClass},boolType{&boolClass},floatType{&floatClass},legacyType{&legacyClass},
    stringType{&stringClass},soundType{&soundClass},nullableType{&nullableClass};
Il2CppClass playerClass{"Player",false,sizeof(FakePlayer),{}};
Il2CppClass styleClass{"Byte",true,1,{}};
Il2CppType styleType{&styleClass};
bool enumStyle=false,rejectStyle=false,narrowValue=false;
Il2CppClass shortClass{"Int16",true,2,{}};Il2CppType shortType{&shortClass};
std::vector<FieldInfo> storage;std::map<int,std::string> identities;
std::map<int,std::string> tooltips;bl::il2cpp::Api apiInstance;
bool nullable=false,initialized=false;int barriers=0;
bool unboxed=false,badBounds=false;
Il2CppObject soundObject{},pathObject{};
SoundValue soundValue{7,&pathObject};

namespace bl::il2cpp {
Api& api(){return apiInstance;}
Il2CppClass* findClass(const TypeRef& type){if(additionalClass)if(auto* cls=additionalClass(type))return cls;return type.name=="Item"?&itemClass:type.name=="SoundID"?&idClass:type.name=="Player"?&playerClass:nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){for(auto* c=cls;c;c=c->parent)for(auto* f:c->fields)if(name==f->name)return f;return nullptr;}
Signature parseSignature(std::string_view text){Signature s;s.name=std::string(text);return s;}
const MethodInfo* findMethodBySignature(Il2CppClass* cls,const Signature& s,bool*,const MethodInfo**) {
 if(additionalMethod)if(auto* method=additionalMethod(cls,s))return method;
 if(cls==&playerClass&&s.name=="void ApplyEquipFunctional(int itemSlot, Item currentItem)")return &functionalMethod;
 if(additionalMethod)return nullptr; // A missing overload is a normal lookup result.
 assert(false&&"Unexpected method lookup in the basic item fixture");return nullptr;
}
Il2CppClass* findNested(Il2CppClass* outer,std::string_view name){return additionalClass?additionalClass({"",outer->name,name}):nullptr;}
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name){return findClass({ns,name,{}});}
int32_t fieldOffset(Il2CppClass* cls,std::string_view name){auto* f=findField(cls,name);return f?static_cast<int32_t>(f->offset):-1;}
}
namespace bl::hook {
bool install(const MethodInfo* method,void* replacement,void** original) {
 if(additionalInstall)return additionalInstall(method,replacement,original);
 assert(method==&functionalMethod);if(rejectEquipHook)return false;
 *original=reinterpret_cast<void*>(originalEquip);functionalHook=reinterpret_cast<EquipCallback>(replacement);++hookInstalls;return true;
}
}
namespace bl::csharp {
bool managedPrefixDescriptorReady(int) { return false; }
bool managedPrefixDescriptorPending(int) { return false; }
bool applyManagedPrefixStatFallback(Il2CppObject*, int, std::string*) { return false; }
}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::runtime {
bool modWallAssetReady(int){return true;}
std::string modItemKey(int type){return identities[type];}
int modTileTypeByKey(const std::string&){return -1;}
int tileTypeCount(){return 754;}
int modWallTypeByKey(const std::string& key){return wallMode&&key=="mod/IndependentWall"?367:-1;}
int wallTypeCount(){return wallMode?368:367;}
int modItemTypeByKey(const std::string& key){for(auto& p:identities)if(p.second==key)return p.first;return -1;}
void setModItemTooltip(int type,std::vector<std::pair<std::string,std::string>> values){tooltips[type]=values.at(0).second;}
}

void configure(){
 storage.reserve(64);
 if(wallMode){static Il2CppClass ushortClass{"UInt16",true,2,{}};static Il2CppType ushortType{&ushortClass};storage.push_back({"createWall",&ushortType,offsetof(FakeItem,createWall),0});itemClass.fields.push_back(&storage.back());}
 storage.push_back({"type",&intType,offsetof(FakeItem,type),0});itemClass.fields.push_back(&storage.back());
 const char* playerFields[]={"meleeDamage","rangedDamage","magicDamage","minionDamage","moveSpeed","strongBees"};
 const size_t playerOffsets[]={offsetof(FakePlayer,meleeDamage),offsetof(FakePlayer,rangedDamage),offsetof(FakePlayer,magicDamage),offsetof(FakePlayer,minionDamage),offsetof(FakePlayer,moveSpeed),offsetof(FakePlayer,strongBees)};
 for(size_t i=0;i<6;++i){storage.push_back({playerFields[i],i==5?&boolType:&floatType,playerOffsets[i],0});playerClass.fields.push_back(&storage.back());}
 storage.push_back({"defense",&shortType,offsetof(FakeItem,defense),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"headSlot",&intType,offsetof(FakeItem,headSlot),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"bodySlot",&shortType,offsetof(FakeItem,bodySlot),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"legSlot",&shortType,offsetof(FakeItem,legSlot),0});itemClass.fields.push_back(&storage.back());
 static Il2CppClass signedClass{"SByte",true,1,{}};static Il2CppType signedType{&signedClass};
 storage.push_back({"wingSlot",&signedType,offsetof(FakeItem,wingSlot),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"beardSlot",&signedType,offsetof(FakeItem,beardSlot),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"backSlot",&signedType,offsetof(FakeItem,backSlot),0});itemClass.fields.push_back(&storage.back());
 const char* names[]={"width","height","maxStack","value","rare","damage","crit","useTime","useAnimation","useStyle","pick","axe","hammer"};
 for(size_t i=0;i<13;i++){storage.push_back({names[i],i==9?&styleType:i==3&&narrowValue?&shortType:&intType,offsetof(FakeItem,values)+i*sizeof(int),0});itemClass.fields.push_back(&storage.back());}
 const char* flags[]={"melee","noMelee","autoReuse","useTurn","consumable","accessory","vanity"};
 for(size_t i=0;i<7;i++){storage.push_back({flags[i],&boolType,offsetof(FakeItem,flags)+i,0});itemClass.fields.push_back(&storage.back());}
 storage.push_back({"knockBack",&floatType,offsetof(FakeItem,knockBack),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"UseSound",nullable?&nullableType:&legacyType,offsetof(FakeItem,sound),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"Item1",nullable?&soundType:&legacyType,0,0x10});idClass.fields.push_back(&storage.back());
 if(nullable){
  const auto header=unboxed?0:sizeof(Il2CppObject);
  storage.push_back({"hasValue",&boolType,header,0});nullableClass.fields.push_back(&storage.back());
  storage.push_back({"value",&soundType,header+8,0});nullableClass.fields.push_back(&storage.back());
  storage.push_back({"path",&stringType,header+offsetof(SoundValue,path),0});soundClass.fields.push_back(&storage.back());
 }
 apiInstance.field_get_type=[](FieldInfo* f){return static_cast<const Il2CppType*>(f->type);};
 apiInstance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 apiInstance.class_is_enum=[](const Il2CppClass* c){return enumStyle&&c==&styleClass;};
 apiInstance.class_enum_basetype=[](Il2CppClass*){return static_cast<const Il2CppType*>(&intType);};
 apiInstance.class_get_namespace=[](Il2CppClass*){return "System";};
 apiInstance.class_get_name=[](Il2CppClass* c){return c->name;};
 apiInstance.class_get_fields=[](Il2CppClass* c,void** iter)->FieldInfo*{auto index=reinterpret_cast<uintptr_t>(*iter);if(index>=c->fields.size())return nullptr;*iter=reinterpret_cast<void*>(index+1);return c->fields[index];};
 apiInstance.field_get_flags=[](FieldInfo* f){return f->flags;};
 apiInstance.field_get_offset=[](FieldInfo* f){return f->offset;};
 apiInstance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 apiInstance.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=8;return c->size;};
 apiInstance.class_instance_size=[](Il2CppClass* c){return static_cast<uint32_t>(c->size);};
 apiInstance.runtime_class_init=[](Il2CppClass* c){assert(c==&idClass);initialized=true;};
 apiInstance.field_static_get_value=[](FieldInfo*,void* out){assert(initialized);if(nullable)std::memcpy(out,&soundValue,sizeof(soundValue));else {void* ptr=&soundObject;std::memcpy(out,&ptr,sizeof(ptr));}};
 apiInstance.gc_wbarrier_set_field=[](Il2CppObject*,void** field,void* value){*field=value;barriers++;};
}

int main(int argc,char** argv){
 assert(bl::csharp::validSourceExecutedTypeCount(1));
 assert(bl::csharp::validSourceExecutedTypeCount(37));
 assert(bl::csharp::validSourceExecutedTypeCount(192));
 assert(bl::csharp::validSourceExecutedTypeCount(1024));
 assert(!bl::csharp::validSourceExecutedTypeCount(0));
 assert(bl::csharp::validSourceExecutedTypeCount(1025));
 assert(bl::csharp::validSourceExecutedTypeCount(8192));
 assert(!bl::csharp::validSourceExecutedTypeCount(8193));
 assert(argc>=2);
 const std::string mode=argc>=3?argv[2]:"--byte";
 narrowValue=mode=="--range";unboxed=mode=="--nullable-unboxed";nullable=mode=="--nullable"||unboxed;enumStyle=mode=="--enum";rejectStyle=mode=="--invalid";badBounds=mode=="--bounds";
 if(mode=="--int16") {styleClass.name="Int16";styleClass.size=2;}
 if(mode=="--int32"||enumStyle) {styleClass.name=enumStyle?"ItemUseStyle":"Int32";styleClass.size=4;}
 if(rejectStyle) {styleClass.name="Single";styleClass.size=4;}
 wallMode=mode=="--wall";
 configure();
 if(wallMode) {
  wallHelper.pointer=reinterpret_cast<void*>(originalEquip);
  additionalMethod=+[](Il2CppClass* cls,const bl::il2cpp::Signature& s)->const MethodInfo*{return cls==&itemClass&&s.name=="void DefaultToPlaceableWall(ushort wallToPlace)"?&wallHelper:nullptr;};
  apiInstance.method_is_instance=[](const MethodInfo* m){return m==&wallHelper;};
  apiInstance.runtime_invoke=[](const MethodInfo* m,void* object,void** args,Il2CppObject** error)->Il2CppObject* {
   assert(m==&wallHelper);*error=nullptr;++wallHelperCalls;
   auto* item=static_cast<FakeItem*>(object);item->createWall=*static_cast<uint16_t*>(args[0]);
   item->values[0]=12;item->values[1]=12;item->values[2]=9999;item->values[7]=7;item->values[8]=15;
   item->flags[2]=true;item->flags[3]=true;item->flags[4]=true;return nullptr;
  };
 }
 if(badBounds)bl::il2cpp::findField(&itemClass,"useStyle")->offset=sizeof(FakeItem);
 std::ifstream file(argv[1]);std::string line,error;std::getline(file,line);
 std::vector<bl::csharp::BasicItemEntry> entries;int next=6196;
 while(std::getline(file,line)){if(line.empty()||(wallMode&&!line.starts_with("ITEM\t")))continue;bl::csharp::BasicItemEntry entry;
  assert(bl::csharp::parseBasicItemWire(bl::csharp::basicSplit(line,'\t'),&entry,&error));
  entry.type=next++;identities[entry.type]="mod/"+entry.name;
  if(rejectStyle) {
   assert(!bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error));
   assert(error.find("Field=useStyle Expected=Integral8/16/32 Actual=Single")!=std::string::npos);
   std::cout<<"NATIVE_BASIC_ITEM_REJECT_INCOMPATIBLE_TYPE_PASS\n";return 0;
  }
  if(badBounds){assert(!bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error));assert(error.find("Field=useStyle")!=std::string::npos&&error.find("Reason=InvalidFieldBounds")!=std::string::npos);std::cout<<"NATIVE_BASIC_ITEM_REJECT_OFFSET_OUTSIDE_INSTANCE_PASS\n";return 0;}
  assert(bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error));entries.push_back(entry);
 }
 if(mode=="--capacity") {
  assert(entries.size()==192);assert(bl::csharp::applyNativeBasicItemDefaults("mod",entries,&error));
  FakeItem object{};assert(bl::csharp::applyBasicItemToObject(&object.header,entries.back().type));
  assert(object.values[bl::csharp::BasicMaxStack]==9999);
  auto over=entries;while(over.size()<4097)over.push_back(entries[0]);
  assert(!bl::csharp::applyNativeBasicItemDefaults("mod",over,&error));
  assert(error.find("Expected=4096 Actual=4097")!=std::string::npos);
  assert(error.find("ContentCountExceeded")!=std::string::npos);
  std::cout<<"NATIVE_BASIC_ITEM_192_AND_CATEGORY_4096_LIMIT_PASS\n";return 0;
 }
 if(wallMode) {
  assert(entries.size()==1&&entries[0].wallPlaceable);entries[0].recipes.clear();
  assert(bl::csharp::applyNativeBasicItemDefaults("mod",entries,&error));
  FakeItem item{};assert(bl::csharp::applyBasicItemToObject(&item.header,entries[0].type));
  assert(wallHelperCalls==1&&item.values[0]==0&&item.values[1]==12&&item.values[2]==9999&&item.values[7]==7&&item.values[8]==15);
  assert(!item.flags[2]&&item.flags[3]&&item.flags[4]&&item.wallSentinel[0]==0x5a&&item.wallSentinel[1]==0x5a);
  // Before dependency resolution an unsigned field gets zero, never -1/65535.
  assert(item.createWall==0);
  std::cout<<"NATIVE_WALL_HELPER_OVERRIDE_EQUAL_ZERO_FALSE_UNSIGNED_WIDTH_NO_QUICKJS_PASS\n";return 0;
 }
 assert(entries.size()==3);
 if(mode=="--recipes-only") {
  for(auto& entry:entries)entry.behavior.clear();
  entries[0].recipes[0].station=-1;entries[0].recipes[0].stationKey="late/Bench";
  entries[0].recipes[0].ingredients[0].key="mlate/Material";
  assert(bl::csharp::applyNativeBasicItemDefaults("mod",entries,&error));
  const auto queued=bl::csharp::pendingBasicRecipes();assert(queued.size()==3);
  bool delayed=false;
  for(const auto& recipe:queued)if(recipe.values.stationKey=="late/Bench") {
   assert(recipe.values.ingredients[0].key=="mlate/Material");
   assert(recipe.values.ingredients[0].type==-1);delayed=true;
  }
  assert(delayed);
  FakeItem item{};assert(bl::csharp::applyBasicItemToObject(&item.header,entries[0].type));
  assert(item.values[bl::csharp::BasicMaxStack]==entries[0].values[bl::csharp::BasicMaxStack]);
  std::cout<<"CSHARP_ITEM_DEFAULTS_AND_RECIPES_WITHOUT_QUICKJS_PASS\n";return 0;
 }
 if(narrowValue) {
  assert(!bl::csharp::applyNativeBasicItemDefaults("mod",entries,&error));
  assert(error.find("value=50000 fora do tipo nativo (16 bits)")!=std::string::npos);
  FakeItem object{};assert(bl::csharp::applyBasicItemToObject(&object.header,entries[0].type));
  assert(object.values[bl::csharp::BasicMaxStack]==1); // nenhum defaults parcial foi publicado
  std::cout<<"NATIVE_BASIC_ITEM_NATIVE_RANGE_PASS\n";return 0;
 }
 assert(bl::csharp::applyNativeBasicItemDefaults("mod",entries,&error));assert(initialized);
 for(const auto& entry:entries){FakeItem object{};
  // Sentinel ocupa bytes adjacentes ao Byte/Int16: escrita larga corromperia aqui.
  object.values[9]=static_cast<int>(0x5a5a5a5a);
  assert(bl::csharp::applyBasicItemToObject(&object.header,entry.type));
  for(size_t i=0;i<13;i++)if(i!=9)assert(object.values[i]==entry.values[i]);
  const auto* style=reinterpret_cast<const unsigned char*>(&object.values[9]);
  const int width=enumStyle?4:styleClass.size;
  uint32_t value=0;std::memcpy(&value,style,static_cast<size_t>(width));
  assert(value==static_cast<uint32_t>(entry.values[9]));
  for(int i=width;i<4;i++)assert(style[i]==0x5a);
  assert(object.knockBack==entry.knockBack);assert(object.flags[0]==!!(entry.values[bl::csharp::BasicFlags]&1));
  if(!entry.useSound.empty()){
   void* ptr=nullptr;std::memcpy(&ptr,object.sound+(nullable?16:0),sizeof(ptr));
   assert(ptr==(nullable?static_cast<void*>(&pathObject):static_cast<void*>(&soundObject)));
   if(nullable)assert(object.sound[0]==1);
  }
 }
 assert(barriers==2);
 const int equippedType=entries[0].type;
 assert(bl::csharp::setBasicItemDefense(equippedType,6,&error));
 assert(!bl::csharp::setBasicItemDefense(equippedType,-1,&error));
 assert(!bl::csharp::setBasicItemDefense(equippedType,1001,&error));
 assert(!bl::csharp::setBasicItemDefense(-1,6,&error));
 auto* defenseField=bl::il2cpp::findField(&itemClass,"defense");
 defenseField->type=&floatType;assert(!bl::csharp::setBasicItemDefense(equippedType,7,&error));assert(error.find("Actual=Single")!=std::string::npos);defenseField->type=&shortType;

 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Head",350,&error));
 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Body",260,&error));
 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Legs",220,&error));
 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Wings",70,&error));
 assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Wings",128,&error));
 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Beard",127,&error));
 assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Beard",128,&error));assert(error.find("SlotOutOfRange")!=std::string::npos);
 assert(bl::csharp::setBasicItemEquipSlot(equippedType,"Back",1,&error));
 auto* backField=bl::il2cpp::findField(&itemClass,"backSlot");
 backField->name="missingBackSlot";
 assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Back",2,&error));assert(error.find("Actual=Missing")!=std::string::npos);
 backField->name="backSlot";
 assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Other",1,&error));
 assert(!bl::csharp::setBasicItemEquipSlot(-1,"Head",1,&error));
 auto* headField=bl::il2cpp::findField(&itemClass,"headSlot");
 headField->flags=0x10;assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Head",12,&error));headField->flags=0;
 headField->type=&floatType;assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Head",12,&error));assert(error.find("Actual=Single")!=std::string::npos);headField->type=&intType;
 const auto savedOffset=headField->offset;headField->offset=0;assert(!bl::csharp::setBasicItemEquipSlot(equippedType,"Head",12,&error));assert(error.find("InvalidFieldOffset")!=std::string::npos);headField->offset=savedOffset;
 FakeItem recreated{};std::memset(recreated.sentinel,0x5a,sizeof(recreated.sentinel));std::memset(recreated.defenseSentinel,0x6b,sizeof(recreated.defenseSentinel));
 assert(bl::csharp::applyBasicItemToObject(&recreated.header,equippedType));
 assert(recreated.wingSlot==70&&recreated.backSlot==1);
 assert(recreated.headSlot==350&&recreated.bodySlot==260&&recreated.legSlot==220&&recreated.beardSlot==127);
 FakeItem savedWing{};
 savedWing.type=equippedType;
 savedWing.wingSlot=-1;
 savedWing.flags[5]=false;
 std::memset(savedWing.sentinel,0x5a,sizeof(savedWing.sentinel));
 assert(savedWing.wingSlot<0&&!savedWing.flags[5]);
 assert(bl::csharp::applyBasicItemToObject(&savedWing.header,equippedType));
 assert(savedWing.type==equippedType);
 assert(savedWing.wingSlot==70&&savedWing.flags[5]==!!(entries[0].values[bl::csharp::BasicFlags]&32));
 for(auto value:savedWing.sentinel)assert(value==0x5a);
 for(auto value:recreated.sentinel)assert(value==0x5a);
 assert(recreated.defense==6);for(auto value:recreated.defenseSentinel)assert(value==0x6b);
 auto pending=bl::csharp::pendingBasicRecipes();assert(pending.size()==3);
 for(const auto& recipe:pending){for(const auto& ingredient:recipe.values.ingredients)assert(ingredient.key.front()=='m'?ingredient.type==-1:ingredient.type>0);
  assert(bl::csharp::acknowledgeBasicRecipe(recipe.id,17));assert(!bl::csharp::acknowledgeBasicRecipe(recipe.id,17));}
 assert(bl::csharp::pendingBasicRecipes().empty());
 auto stationItem=entries[0];stationItem.type=7199;stationItem.name="StationFixture";
 identities[stationItem.type]="mod/StationFixture";
 assert(bl::csharp::registerBasicItemPlaceholder(stationItem.type,stationItem.name,&error));
 stationItem.recipes[0].station=-1;stationItem.recipes[0].stationKey="ContractBench";
 assert(bl::csharp::applyNativeBasicItemDefaults("mod",{stationItem},&error));
 const auto stationPending=bl::csharp::pendingBasicRecipes();assert(stationPending.size()==1);
 assert(stationPending[0].values.stationKey=="mod/ContractBench");
 assert(bl::csharp::acknowledgeBasicRecipe(stationPending[0].id,18));

 std::vector<bl::csharp::BasicItemEntry> retryItems;
 for(int i=0;i<9;++i) {
  auto item=entries[0];item.type=7300+i;item.name="RetryFixture"+std::to_string(i);item.recipes[0].station=-1;item.recipes[0].stationKey="MissingStation";
  item.behavior="{\"v\":1}";identities[item.type]="mod/"+item.name;
  assert(bl::csharp::registerBasicItemPlaceholder(item.type,item.name,&error,"/mod","content/"+item.name+".cs"));retryItems.push_back(item);
 }
 // Malformed descriptors are rejected by the native parser with or without QuickJS.
 assert(!bl::csharp::applyNativeBasicItemDefaults("mod",retryItems,&error));
 assert(error.find("Stage=Decode Reason=InvalidFlag")!=std::string::npos);
 for(auto& item:retryItems)item.behavior.clear();
 assert(bl::csharp::applyNativeBasicItemDefaults("mod",retryItems,&error));
 assert(bl::csharp::pendingBasicItemBehaviors().empty());
 assert(bl::csharp::basicItemPolicyReady(7300));
 assert(bl::csharp::basicItemPolicyReady(entries[0].type)&&bl::csharp::basicItemPolicyReady(2));
 const auto recipesFirst=bl::csharp::pendingBasicRecipes(),recipesSecond=bl::csharp::pendingBasicRecipes();assert(recipesFirst.size()==8&&recipesSecond.size()==8);
 bool lastRecipe=false;for(const auto& recipe:recipesSecond)if(recipe.result==7308)lastRecipe=true;assert(lastRecipe);
 assert(bl::csharp::reportBasicRecipeError(recipesSecond[0].id,"missing station"));
 assert(!bl::csharp::reportBasicRecipeError("not-registered","error"));
 bool persisted=false;for(const auto& recipe:bl::csharp::pendingBasicRecipes())if(recipe.id==recipesSecond[0].id){assert(recipe.error=="missing station");assert(!recipe.source.empty());persisted=true;}
 if(!persisted)for(const auto& recipe:bl::csharp::pendingBasicRecipes())if(recipe.id==recipesSecond[0].id){assert(recipe.error=="missing station");persisted=true;}
 assert(persisted);
 auto bad=entries;bad[0].values[bl::csharp::BasicDamage]=999999;
 assert(!bl::csharp::applyNativeBasicItemDefaults("mod",bad,&error));
 FakeItem object{};assert(bl::csharp::applyBasicItemToObject(&object.header,entries[0].type));assert(object.values[bl::csharp::BasicDamage]==entries[0].values[bl::csharp::BasicDamage]);
 bad=entries;bad[0].useSound="DoesNotExist";assert(!bl::csharp::applyNativeBasicItemDefaults("mod",bad,&error));
 assert(error.find("SoundID.DoesNotExist")!=std::string::npos);
 std::cout<<"NATIVE_BASIC_ITEM_BINDING_PASS="<<mode<<"\n";
 return 0;
}
