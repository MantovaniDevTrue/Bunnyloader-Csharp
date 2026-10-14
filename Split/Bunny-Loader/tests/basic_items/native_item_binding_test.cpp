#include "experiments/csharp/NativeBasicItems.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

struct Il2CppClass { const char* name; bool value; int size; std::vector<FieldInfo*> fields; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType* type; size_t offset; uint32_t flags; };
struct FakeItem { Il2CppObject header; int values[13]; bool flags[5]; float knockBack; alignas(void*) unsigned char sound[64]; };
struct SoundValue { int volume; void* path; };
Il2CppClass intClass{"Int32",true,4,{}},boolClass{"Boolean",true,1,{}},floatClass{"Single",true,4,{}},
    legacyClass{"LegacySoundStyle",false,0,{}},stringClass{"String",false,0,{}},
    soundClass{"SoundStyle",true,static_cast<int>(sizeof(SoundValue)),{}},
    nullableClass{"Nullable`1",true,24,{}},itemClass{"Item",false,0,{}},idClass{"SoundID",false,0,{}};
Il2CppType intType{&intClass},boolType{&boolClass},floatType{&floatClass},legacyType{&legacyClass},
    stringType{&stringClass},soundType{&soundClass},nullableType{&nullableClass};
Il2CppClass styleClass{"Byte",true,1,{}};
Il2CppType styleType{&styleClass};
bool enumStyle=false,rejectStyle=false,narrowValue=false;
Il2CppClass shortClass{"Int16",true,2,{}};Il2CppType shortType{&shortClass};
std::vector<FieldInfo> storage;std::map<int,std::string> identities;
std::map<int,std::string> tooltips;bl::il2cpp::Api apiInstance;
bool nullable=false,initialized=false;int barriers=0;
Il2CppObject soundObject{},pathObject{};
SoundValue soundValue{7,&pathObject};

namespace bl::il2cpp {
Api& api(){return apiInstance;}
Il2CppClass* findClass(const TypeRef& type){return type.name=="Item"?&itemClass:type.name=="SoundID"?&idClass:nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){for(auto* f:cls->fields)if(name==f->name)return f;return nullptr;}
int32_t fieldOffset(Il2CppClass* cls,std::string_view name){auto* f=findField(cls,name);return f?static_cast<int32_t>(f->offset):-1;}
}
namespace bl::script {size_t structFieldOffset(Il2CppClass*,size_t offset){return offset-sizeof(Il2CppObject);}}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::runtime {
std::string modItemKey(int type){return identities[type];}
int modItemTypeByKey(const std::string& key){for(auto& p:identities)if(p.second==key)return p.first;return -1;}
void setModItemTooltip(int type,std::vector<std::pair<std::string,std::string>> values){tooltips[type]=values.at(0).second;}
}

void configure(){
 storage.reserve(24);
 const char* names[]={"width","height","maxStack","value","rare","damage","crit","useTime","useAnimation","useStyle","pick","axe","hammer"};
 for(size_t i=0;i<13;i++){storage.push_back({names[i],i==9?&styleType:i==3&&narrowValue?&shortType:&intType,offsetof(FakeItem,values)+i*sizeof(int),0});itemClass.fields.push_back(&storage.back());}
 const char* flags[]={"melee","noMelee","autoReuse","useTurn","consumable"};
 for(size_t i=0;i<5;i++){storage.push_back({flags[i],&boolType,offsetof(FakeItem,flags)+i,0});itemClass.fields.push_back(&storage.back());}
 storage.push_back({"knockBack",&floatType,offsetof(FakeItem,knockBack),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"UseSound",nullable?&nullableType:&legacyType,offsetof(FakeItem,sound),0});itemClass.fields.push_back(&storage.back());
 storage.push_back({"Item1",nullable?&soundType:&legacyType,0,0x10});idClass.fields.push_back(&storage.back());
 if(nullable){
  storage.push_back({"hasValue",&boolType,sizeof(Il2CppObject),0});nullableClass.fields.push_back(&storage.back());
  storage.push_back({"value",&soundType,sizeof(Il2CppObject)+8,0});nullableClass.fields.push_back(&storage.back());
  storage.push_back({"path",&stringType,sizeof(Il2CppObject)+offsetof(SoundValue,path),0});soundClass.fields.push_back(&storage.back());
 }
 apiInstance.field_get_type=[](FieldInfo* f){return static_cast<const Il2CppType*>(f->type);};
 apiInstance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 apiInstance.class_is_enum=[](const Il2CppClass* c){return enumStyle&&c==&styleClass;};
 apiInstance.class_enum_basetype=[](Il2CppClass*){return static_cast<const Il2CppType*>(&intType);};
 apiInstance.class_get_name=[](Il2CppClass* c){return c->name;};
 apiInstance.class_get_fields=[](Il2CppClass* c,void** iter)->FieldInfo*{auto index=reinterpret_cast<uintptr_t>(*iter);if(index>=c->fields.size())return nullptr;*iter=reinterpret_cast<void*>(index+1);return c->fields[index];};
 apiInstance.field_get_flags=[](FieldInfo* f){return f->flags;};
 apiInstance.field_get_offset=[](FieldInfo* f){return f->offset;};
 apiInstance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 apiInstance.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=8;return c->size;};
 apiInstance.runtime_class_init=[](Il2CppClass* c){assert(c==&idClass);initialized=true;};
 apiInstance.field_static_get_value=[](FieldInfo*,void* out){assert(initialized);if(nullable)std::memcpy(out,&soundValue,sizeof(soundValue));else {void* ptr=&soundObject;std::memcpy(out,&ptr,sizeof(ptr));}};
 apiInstance.gc_wbarrier_set_field=[](Il2CppObject*,void** field,void* value){*field=value;barriers++;};
}

int main(int argc,char** argv){
 assert(argc>=2);
 const std::string mode=argc>=3?argv[2]:"--byte";
 narrowValue=mode=="--range";nullable=mode=="--nullable";enumStyle=mode=="--enum";rejectStyle=mode=="--invalid";
 if(mode=="--int16") {styleClass.name="Int16";styleClass.size=2;}
 if(mode=="--int32"||enumStyle) {styleClass.name=enumStyle?"ItemUseStyle":"Int32";styleClass.size=4;}
 if(rejectStyle) {styleClass.name="Single";styleClass.size=4;}
 configure();
 std::ifstream file(argv[1]);std::string line,error;std::getline(file,line);
 std::vector<bl::csharp::BasicItemEntry> entries;int next=6196;
 while(std::getline(file,line)){if(line.empty())continue;bl::csharp::BasicItemEntry entry;
  assert(bl::csharp::parseBasicItemWire(bl::csharp::basicSplit(line,'\t'),&entry,&error));
  entry.type=next++;identities[entry.type]="mod/"+entry.name;
  if(rejectStyle) {
   assert(!bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error));
   assert(error.find("useStyle (tipo=Single)")!=std::string::npos);
   std::cout<<"NATIVE_BASIC_ITEM_REJECT_INCOMPATIBLE_TYPE_PASS\n";return 0;
  }
  assert(bl::csharp::registerBasicItemPlaceholder(entry.type,entry.name,&error));entries.push_back(entry);
 }
 assert(entries.size()==3);
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
 assert(barriers==2);auto pending=bl::csharp::pendingBasicRecipes();assert(pending.size()==3);
 for(const auto& recipe:pending){for(const auto& ingredient:recipe.values.ingredients)assert(ingredient.type>0);
  assert(bl::csharp::acknowledgeBasicRecipe(recipe.id,17));assert(!bl::csharp::acknowledgeBasicRecipe(recipe.id,17));}
 assert(bl::csharp::pendingBasicRecipes().empty());
 auto bad=entries;bad[0].values[bl::csharp::BasicDamage]=999999;
 assert(!bl::csharp::applyNativeBasicItemDefaults("mod",bad,&error));
 FakeItem object{};assert(bl::csharp::applyBasicItemToObject(&object.header,entries[0].type));assert(object.values[bl::csharp::BasicDamage]==entries[0].values[bl::csharp::BasicDamage]);
 bad=entries;bad[0].useSound="DoesNotExist";assert(!bl::csharp::applyNativeBasicItemDefaults("mod",bad,&error));
 assert(error.find("SoundID.DoesNotExist")!=std::string::npos);
 std::cout<<"NATIVE_BASIC_ITEM_BINDING_PASS="<<mode<<"\n";
}
