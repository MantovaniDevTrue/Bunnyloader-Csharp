#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
struct Il2CppClass {std::string ns,name;uint32_t size=0;bool value=false;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {std::string name;Il2CppType type;size_t offset=0;uint32_t flags=0;void* storage=nullptr;};
struct MethodInfo {void* pointer=nullptr;std::string signature;bool instance=false;};
struct Player {Il2CppObject obj;int life=100,max=200;bool desert=true,water=false,snow=false;Il2CppArray* adjacent=nullptr;};
struct Entry {Il2CppObject obj;int type=22,stack=20;};
struct RawEntry8{int type=22,stack=20;};
struct RawEntry12{int type=22,stack=20,tail=123;};
struct RawEntry16{int type=22,stack=20,tail=123,pad=456;};
Il2CppClass floatClass{"System","Single",4,true},ushortClass{"System","UInt16",2,true},integer{"System","Int32",4,true},booleanClass{"System","Boolean",1,true},
    mainClass{"Terraria","Main"},recipeClass{"Terraria","Recipe",sizeof(Il2CppObject)},
    playerClass{"Terraria","Player",sizeof(Player)},itemClass{"Terraria","Item",sizeof(Il2CppObject)},
    entryClass{"Terraria","RequiredItemEntry",sizeof(Entry)},groupClass{"Terraria","RecipeGroup",sizeof(Il2CppObject)},
    tileClass{"Terraria.ID","TileID"},boolArray{"System","Boolean[]",0,false,&booleanClass},
    playerArray{"Terraria","Player[]",0,false,&playerClass},intArray{"System","Int32[]",0,false,&integer};
bl::il2cpp::Api instance;
std::vector<FieldInfo> fields;std::map<std::string,MethodInfo> methods;
std::map<std::string,void*> hooks;
int sinks=1;
int playerIndex=0,availableCount=4,findCalls=0,craftCalls=0,grantCalls=0,grantedType=0,grantedCount=0;
bool day=false,rpcReady=true,sourceNameDrift=false,grantNameDrift=false,sourceTypeMismatch=false;
std::string conditionReply="0",consumeReply="0",craftReply="OK|v2:3",lastSnapshot;
Player player{{&playerClass,nullptr}};Il2CppObject recipe{&recipeClass,nullptr},source{&itemClass,nullptr};
Il2CppArray *players=nullptr,*available=nullptr;
namespace bl::il2cpp {
Api& api(){return instance;}
Il2CppClass* findClass(const TypeRef& ref){if(ref.nested=="RequiredItemEntry")return &entryClass;
 for(auto* cls:{&mainClass,&recipeClass,&playerClass,&itemClass,&groupClass,&tileClass})if(cls->ns==ref.ns&&cls->name==ref.name)return cls;
 return nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){for(auto& f:fields)if(f.name==cls->name+"."+std::string(name))return &f;return nullptr;}
Signature parseSignature(std::string_view value){Signature s;s.name=std::string(value);return s;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo** nameMismatch){
 auto it=methods.find(sig.name);if(it==methods.end())return nullptr;
 if(sig.name=="IEntitySource GetItemSource_Misc(int context)"){
   if(sourceTypeMismatch)return nullptr;
   if (sourceNameDrift) {
     if (nameMismatch) {
       *nameMismatch = &it->second;
     }
     return nullptr;
   }
 }
 if(sig.name=="void QuickSpawnItem(IEntitySource source, int item, int stack)"&&grantNameDrift){
   if (nameMismatch) {
     *nameMismatch = &it->second;
   }
   return nullptr;
 }
 return &it->second;
}
std::vector<std::string> listOverloads(Il2CppClass*,std::string_view name){
 std::vector<std::string> results;
 for(const auto& entry:methods)if(entry.first.find(" "+std::string(name)+"(")!=std::string::npos)results.push_back(entry.second.signature);
 return results;
}
}
namespace bl::hook {bool install(const MethodInfo* method,void* replacement,void** original){hooks[method->signature]=replacement;*original=method->pointer;return true;}}
namespace bl::log {void write(int,const char*,...){}}
namespace bl::runtime {int modItemTypeByKey(const std::string& key){return key=="mod/Material"?6196:-1;}int itemTypeCount(){return 6197;}}
namespace bl::csharp {
bool managedRecipeRuntimeReady(){return rpcReady;}
bool invokeManagedRecipe(const std::string& mod,const std::string&,const std::string& owner,int index,int kind,
                         const std::string& snapshot,std::string* output,int budget){
 assert(mod=="mod"&&owner=="Bench"&&index==0&&budget>0&&budget<=3000);lastSnapshot=snapshot;
 if(!rpcReady)return false;
 *output=kind==0?conditionReply:kind==1?consumeReply:craftReply;return true;
}
}
#include "experiments/csharp/NativeRecipeCallbacks.cpp"
void originalFind(bool,const MethodInfo*){++findCalls;}
void originalCraft(Il2CppObject*,Il2CppObject*,bool,const MethodInfo*){++craftCalls;}
int originalDiscount(Il2CppObject*,Il2CppObject*,void*,const MethodInfo*){return 5;}
int originalDiscount8(Il2CppObject*,Il2CppObject*,RawEntry8 e,const MethodInfo*){assert(e.type==22&&e.stack==20);return 5;}
int originalDiscount12(Il2CppObject*,Il2CppObject*,RawEntry12 e,const MethodInfo*){assert(e.type==22&&e.stack==20&&e.tail==123);return 5;}
int originalDiscount16(Il2CppObject*,Il2CppObject*,RawEntry16 e,const MethodInfo*){assert(e.type==22&&e.stack==20&&e.tail==123&&e.pad==456);return 5;}
int owned8(RawEntry8 e,const MethodInfo*){assert(e.stack==20);return 64;}
int owned12(RawEntry12 e,const MethodInfo*){assert(e.tail==123);return 64;}
int owned16(RawEntry16 e,const MethodInfo*){assert(e.pad==456);return 64;}
bool isGroup(void*,const MethodInfo*){return false;}
Il2CppObject* group(void*,const MethodInfo*){return nullptr;}
int placeholder(Il2CppObject*,const MethodInfo*){return 9;}
int owned(void*,const MethodInfo*){return 64;}
Il2CppArray* array(Il2CppClass* cls,size_t count){auto* p=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+count*sizeof(void*)));p->obj.klass=cls;p->length=count;return p;}
void setup(){
 player.adjacent=array(&boolArray,4);
 players=array(&playerArray,1);static_cast<Il2CppObject**>(arrayData(players))[0]=&player.obj;available=array(&intArray,4);
 fields={{"Main.player",{&playerArray},0,0x10,&players},{"Main.dayTime",{&booleanClass},0,0x10,&day},
 {"Main.myPlayer",{&integer},0,0x10,&playerIndex},{"Main.availableRecipe",{&intArray},0,0x10,&available},
 {"Main.numAvailableRecipes",{&integer},0,0x10,&availableCount},
 {"Player.statLife",{&integer},offsetof(Player,life)},{"Player.statLifeMax2",{&integer},offsetof(Player,max)},
 {"Player.ZoneDesert",{&booleanClass},offsetof(Player,desert)},{"Player.adjWaterSource",{&booleanClass},offsetof(Player,water)},
 {"Player.ZoneSnow",{&booleanClass},offsetof(Player,snow)},
 {"Player.adjTile",{&boolArray},offsetof(Player,adjacent)},
 {"TileID.Sinks",{&integer},0,0x10,&sinks},
 {"RequiredItemEntry.stack",{&integer},offsetof(Entry,stack)},{"RequiredItemEntry.itemIdOrRecipeGroup",{&integer},offsetof(Entry,type)}};
 auto add=[](const char* name,void* pointer,bool instance){methods.emplace(name,MethodInfo{pointer,name,instance});};
 add("void FindRecipes(bool canDelayCheck)",reinterpret_cast<void*>(originalFind),false);
 add("void CraftItem_GrantItem(Recipe recipe, Item result, bool quickCraft)",reinterpret_cast<void*>(originalCraft),false);
 add("int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)",reinterpret_cast<void*>(originalDiscount),true);
 add("bool get_IsRecipeGroup()",reinterpret_cast<void*>(isGroup),true);add("RecipeGroup get_RecipeGroup()",reinterpret_cast<void*>(group),true);
 add("int GetPlaceholderItemType()",reinterpret_cast<void*>(placeholder),true);
 add("int GetAvailableItemCount(Recipe.RequiredItemEntry item)",reinterpret_cast<void*>(owned),false);
 add("IEntitySource GetItemSource_Misc(int context)",nullptr,true);add("void QuickSpawnItem(IEntitySource source, int item, int stack)",nullptr,true);
 instance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};instance.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 instance.class_get_name=[](Il2CppClass* cls){return cls->name.c_str();};instance.class_get_namespace=[](Il2CppClass* cls){return cls->ns.c_str();};
 instance.field_get_flags=[](FieldInfo* f){return f->flags;};instance.field_get_offset=[](FieldInfo* f){return f->offset;};
 instance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};instance.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=4;return static_cast<int>(c->size);};
 instance.class_instance_size=[](Il2CppClass* c){return c->value?static_cast<uint32_t>(sizeof(Il2CppObject))+c->size:c->size;};
 instance.class_get_fields=[](Il2CppClass* cls,void** cursor)->FieldInfo* {
    size_t index=reinterpret_cast<uintptr_t>(*cursor);
    for(;index<fields.size();++index)if(fields[index].name.rfind(cls->name+".",0)==0){*cursor=reinterpret_cast<void*>(index+1);return &fields[index];}
    return nullptr;
 };
 instance.class_get_rank=[](const Il2CppClass* c){return c->element?1:0;};instance.class_get_element_class=[](Il2CppClass* c){return c->element;};
 instance.object_get_class=[](Il2CppObject* o){return o->klass;};
 instance.method_is_instance=[](const MethodInfo* m){return m->instance;};
 instance.field_static_get_value=[](FieldInfo* f,void* value){std::memcpy(value,f->storage,f->type.cls->value?f->type.cls->size:sizeof(void*));};
 instance.field_static_set_value=[](FieldInfo* f,void* value){assert(f->type.cls==&integer);std::memcpy(f->storage,value,4);};
 instance.gchandle_new=[](Il2CppObject*,bool){return 1u;};instance.gchandle_free=[](uint32_t h){assert(h==1);};
 instance.runtime_invoke=[](const MethodInfo* m,void*,void** args,Il2CppObject** error)->Il2CppObject*{
  *error=nullptr;if(m->signature=="IEntitySource GetItemSource_Misc(int context)")return &source;
  assert(m->signature=="void QuickSpawnItem(IEntitySource source, int item, int stack)");
  assert(args[0]==&source);++grantCalls;grantedType=*static_cast<int*>(args[1]);grantedCount=*static_cast<int*>(args[2]);return nullptr;
 };
}
void resetAvailable(){availableCount=4;int data[]={10,11,12,13};std::memcpy(arrayData(available),data,sizeof data);}
int main(int argc,char** argv){
 setup();std::string error;const std::string mode=argc>1?argv[1]:"--callbacks";
 if(mode.rfind("--value",0)==0||mode=="--bad-aggregate") {
    const bool boxed=mode.find("boxed")!=std::string::npos;
    const int size=mode.find("12")!=std::string::npos?12:mode.find("16")!=std::string::npos||mode=="--bad-aggregate"?16:8;
    entryClass.value=true;entryClass.size=size;
    for(auto& f:fields)if(f.name=="RequiredItemEntry.stack")f.offset=4+(boxed?sizeof(Il2CppObject):0);
      else if(f.name=="RequiredItemEntry.itemIdOrRecipeGroup")f.offset=boxed?sizeof(Il2CppObject):0;
    if(size>=12)fields.push_back({"RequiredItemEntry.tail",{mode=="--bad-aggregate"?&floatClass:&integer},8+(boxed?sizeof(Il2CppObject):0)});
    if(size==16)fields.push_back({"RequiredItemEntry.pad",{&integer},12+(boxed?sizeof(Il2CppObject):0)});
    methods.at("int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)").pointer=
        size==8?reinterpret_cast<void*>(originalDiscount8):size==12?reinterpret_cast<void*>(originalDiscount12):reinterpret_cast<void*>(originalDiscount16);
    methods.at("int GetAvailableItemCount(Recipe.RequiredItemEntry item)").pointer=
        size==8?reinterpret_cast<void*>(owned8):size==12?reinterpret_cast<void*>(owned12):reinterpret_cast<void*>(owned16);
    if(mode=="--bad-aggregate") {
        assert(!bl::csharp::prepareNativeRecipeCallbacks(2,&error)&&error.find("UnsupportedAggregateAbi")!=std::string::npos&&hooks.empty());
        std::cout<<"NATIVE_RECIPE_FLOATING_AGGREGATE_REJECTION_PASS\n";return 0;
    }
    assert(bl::csharp::prepareNativeRecipeCallbacks(2,&error));
    bl::runtime::NativeRecipeRequest policy;policy.key="mod/Bench#0";policy.mod="mod";policy.name="Bench";policy.result=6196;policy.callbacks=2;
    bl::csharp::bindNativeRecipeAssembly("mod","test.dll");bl::csharp::trackNativeRecipeCallbacks(policy,&recipe,11);
    auto* hook=hooks.at("int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)");
    if(size==8)assert((reinterpret_cast<int(*)(Il2CppObject*,Il2CppObject*,RawEntry8,const MethodInfo*)>(hook)(&recipe,&player.obj,RawEntry8{},nullptr)==20));
    else if(size==12)assert((reinterpret_cast<int(*)(Il2CppObject*,Il2CppObject*,RawEntry12,const MethodInfo*)>(hook)(&recipe,&player.obj,RawEntry12{},nullptr)==20));
    else assert((reinterpret_cast<int(*)(Il2CppObject*,Il2CppObject*,RawEntry16,const MethodInfo*)>(hook)(&recipe,&player.obj,RawEntry16{},nullptr)==20));
    std::cout<<"NATIVE_RECIPE_REQUIRED_ENTRY_VALUE_ABI_PASS "<<mode<<'\n';return 0;
 }
 if(mode=="--wrong-type"){fields[6].type.cls=&booleanClass;assert(!bl::csharp::prepareNativeRecipeCallbacks(7,&error)&&error.find("NativeTypeOrStorageMismatch")!=std::string::npos&&hooks.empty());std::cout<<"RECIPE_CALLBACK_WRONG_TYPE_REJECTED_PASS\n";return 0;}
 if(mode=="--wrong-offset"){fields[5].offset=sizeof(Player)+4;assert(!bl::csharp::prepareNativeRecipeCallbacks(7,&error)&&error.find("InvalidFieldBounds")!=std::string::npos&&hooks.empty());std::cout<<"RECIPE_CALLBACK_WRONG_OFFSET_REJECTED_PASS\n";return 0;}
 if(mode=="--source-name-drift"||mode=="--grant-name-drift"||mode=="--source-type-mismatch"||mode=="--source-static-mismatch"){
   sourceNameDrift=mode=="--source-name-drift";grantNameDrift=mode=="--grant-name-drift";sourceTypeMismatch=mode=="--source-type-mismatch";
   if(mode=="--source-static-mismatch")methods.at("IEntitySource GetItemSource_Misc(int context)").instance=false;
   const bool valid=mode=="--source-name-drift"||mode=="--grant-name-drift";
   const bool ready=bl::csharp::prepareNativeRecipeCallbacks(4,&error);
   if(!valid){
     assert(!ready&&hooks.empty()&&error.find("NativeSignatureMismatch")!=std::string::npos&&error.find("HookInstallFailed")==std::string::npos);
     std::cout<<"NATIVE_RECIPE_GRANT_ABI_REJECTION_PASS "<<mode<<'\n';return 0;
   }
   assert(ready&&hooks.size()==1);
   bl::runtime::NativeRecipeRequest policy;policy.key="mod/Bench#0";policy.mod="mod";policy.name="Bench";policy.result=6196;policy.callbacks=4;
   bl::csharp::bindNativeRecipeAssembly("mod","test.dll");bl::csharp::trackNativeRecipeCallbacks(policy,&recipe,11);
   auto craft=reinterpret_cast<void(*)(Il2CppObject*,Il2CppObject*,bool,const MethodInfo*)>(hooks.at("void CraftItem_GrantItem(Recipe recipe, Item result, bool quickCraft)"));
   craft(&recipe,nullptr,false,nullptr);
   assert(craftCalls==1&&grantCalls==1&&grantedType==2&&grantedCount==3);
   std::cout<<"NATIVE_RECIPE_GRANT_PARAMETER_NAME_DRIFT_PASS "<<mode<<'\n';return 0;
 }
 bl::runtime::NativeRecipeRequest waiting;waiting.callbacks=7;
 rpcReady=false;assert(bl::csharp::prepareNativeRecipeCallbackProvider(waiting,&error)==bl::runtime::RecipeCallbackReadiness::Pending);rpcReady=true;
 if(mode=="--ushort-sink")for(auto& f:fields)if(f.name=="TileID.Sinks")f.type.cls=&ushortClass;
 assert(bl::csharp::prepareNativeRecipeCallbacks(7,&error));assert(hooks.size()==3);
 bl::runtime::NativeRecipeRequest policy;policy.key="mod/Bench#0";policy.mod="mod";policy.name="Bench";policy.result=6196;policy.callbacks=7;
 bl::csharp::bindNativeRecipeAssembly("mod","test.dll");bl::csharp::trackNativeRecipeCallbacks(policy,&recipe,11);
 auto find=reinterpret_cast<void(*)(bool,const MethodInfo*)>(hooks.at("void FindRecipes(bool canDelayCheck)"));
 resetAvailable();find(false,nullptr);assert(findCalls==1&&availableCount==3&&static_cast<int*>(arrayData(available))[1]==12);
 assert(lastSnapshot=="100,200,2,6196,0,0,0,0,0");
 conditionReply="PENDING|Stage=Invoke Reason=FixturePending";resetAvailable();find(false,nullptr);assert(availableCount==3);
 conditionReply="1";resetAvailable();find(false,nullptr);assert(availableCount==4);
 static_cast<uint8_t*>(arrayData(player.adjacent))[1]=1;resetAvailable();find(false,nullptr);assert(lastSnapshot=="100,200,6,6196,0,0,0,0,0");static_cast<uint8_t*>(arrayData(player.adjacent))[1]=0;
 rpcReady=false;resetAvailable();find(false,nullptr);assert(availableCount==3);rpcReady=true;
 auto consume=reinterpret_cast<int(*)(Il2CppObject*,Il2CppObject*,void*,const MethodInfo*)>(hooks.at("int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)"));
 Entry entry{{&entryClass,nullptr}};assert(consume(&recipe,&player.obj,&entry,nullptr)==20);
 consumeReply="999999";assert(consume(&recipe,&player.obj,&entry,nullptr)==-44);
 consumeReply="-10";assert(consume(&recipe,&player.obj,&entry,nullptr)==20);
 consumeReply="ERROR|FixtureFailure";assert(consume(&recipe,&player.obj,&entry,nullptr)==5);
 rpcReady=false;assert(consume(&recipe,&player.obj,&entry,nullptr)==5);rpcReady=true;
 auto craft=reinterpret_cast<void(*)(Il2CppObject*,Il2CppObject*,bool,const MethodInfo*)>(hooks.at("void CraftItem_GrantItem(Recipe recipe, Item result, bool quickCraft)"));
 craft(&recipe,nullptr,false,nullptr);assert(craftCalls==1&&grantCalls==1&&grantedType==2&&grantedCount==3);
 craftReply="OK|mmod/Material:2";craft(&recipe,nullptr,true,nullptr);assert(craftCalls==2&&grantCalls==2&&grantedType==6196&&grantedCount==2);
 craftReply="OK|v2:1;mmod/Missing:3";craft(&recipe,nullptr,false,nullptr);assert(craftCalls==3&&grantCalls==2);
 craftReply="ERROR|FixtureFailure";craft(&recipe,nullptr,false,nullptr);assert(craftCalls==4&&grantCalls==2);
 std::cout<<"NATIVE_RECIPE_CALLBACK_HOOKS_FILTER_CONSUME_CRAFT_GRANT_BOUNDS_ISOLATION_PASS\n";
}
