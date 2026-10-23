#define main existingNativeItemFixtureMain
#include "native_item_binding_test.cpp"
#undef main
#include "experiments/csharp/NativeArmorPolicies.h"
#include <array>
#include <algorithm>
#include <limits>
#include <memory>
#include <thread>

namespace {
struct NativeSet { Il2CppObject obj;int head=0,body=0,legs=0;Il2CppObject* effect=nullptr; };
struct SetValue { int head=0,body=0,legs=0;Il2CppObject* effect=nullptr; };
Il2CppClass boolArray{"Boolean[]",false,0,{},&boolClass,1},itemArray{"Item[]",false,0,{},&itemClass,1},
 setClass{"ArmorSetBonus",false,sizeof(NativeSet),{}},rowClass{"ArmorSetBonus[]",false,0,{},&setClass,1},
 outerClass{"ArmorSetBonus[][]",false,0,{},&rowClass,1},allClass{"SetList",false,sizeof(Il2CppObject),{}},
 bonusesClass{"ArmorSetBonuses",false,0,{}},benefitsClass{"Benefits",false,0,{}},arrayClass{"Array",false,0,{}},
 buffIdClass{"BuffID",false,0,{}},itemIdClass{"ItemID",false,0,{}},languageClass{"Language",false,0,{}},wideBoolClass{"Boolean",true,4,{}};
Il2CppType wideBoolType{&wideBoolClass};
bool localeAvailable=false,localeException=false,localeNull=false,localeWrongResult=false,localeWrongType=false,localeInstance=false,localeMissing=false,localeWide=false;
struct BoolBox { Il2CppObject object{&boolClass,nullptr};uint8_t value=0; } localeResult;
std::map<std::string,std::map<std::string,std::string>> localeCatalog;
std::string activeCulture="en-US";
std::string description(const std::string& key) {
 auto row=localeCatalog.find(key);if(row==localeCatalog.end())return key;
 const auto current=row->second.find(activeCulture);if(current!=row->second.end())return current->second;
 return row->second.at("en-US");
}
std::string unhex(const std::string& s) {
 std::string value;assert(s.size()%2==0);
 for(size_t i=0;i<s.size();i+=2)value.push_back(static_cast<char>(std::stoul(s.substr(i,2),nullptr,16)));
 return value;
}
Il2CppClass partTypeClass{"PartType",true,1,{}};Il2CppType partType{&partTypeClass};
bool badAbi=false;
const std::thread::id gameThread=std::this_thread::get_id();
bool failRemove=false;
Il2CppType boolArrayType{&boolArray},itemArrayType{&itemArray},rowType{&outerClass},allType{&allClass},delegateType{&legacyClass};
std::vector<FieldInfo> extraFields;
std::map<std::string,MethodInfo> methods;
std::map<const MethodInfo*,std::string> names;
struct Chain { void* outer=nullptr;std::vector<void**> originals; };
std::map<const MethodInfo*,Chain> chains;
std::vector<NativeSet*> all;
Il2CppArray* lookup=nullptr;
Il2CppObject allObject{&allClass,nullptr},delegateObject{&legacyClass,nullptr},exception{};
std::map<Il2CppObject*,std::string> strings;
std::map<unsigned,Il2CppObject*> roots;
unsigned nextRoot=1;
bool valueMode=false,boxedOffsets=false,failArraySet=false,failAdd=false,failAfterAdd=false,throwAfterAppend=false;
int adds=0,vanillaPumpkinCalls=0,grantCalls=0,updates=0,writes=0,allocations=0;
int buffId=24,pumpkinId=900;
std::vector<std::string> texts;
FieldInfo* lookupField=nullptr;
size_t elementSize(Il2CppClass* cls){return cls->value?static_cast<size_t>(cls->size):sizeof(void*);}
Il2CppArray* array(Il2CppClass* element,uintptr_t size) {
 auto* cls=element==&rowClass?&outerClass:element==&setClass?&rowClass:element==&boolClass?&boolArray:&itemArray;
 auto* result=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+size*elementSize(element)));
 result->obj.klass=cls;result->length=size;++allocations;return result;
}
NativeSet* makeSet(int head,int body,int legs,Il2CppObject* effect){auto* s=new NativeSet;s->obj.klass=&setClass;s->head=head;s->body=body;s->legs=legs;s->effect=effect;return s;}
void put(Il2CppArray* a,int index,Il2CppObject* value) {
 auto* element=a->obj.klass->element;auto* at=static_cast<unsigned char*>(arrayData(a))+elementSize(element)*static_cast<size_t>(index);
 if(element->value)std::memcpy(at,reinterpret_cast<unsigned char*>(value)+sizeof(Il2CppObject),elementSize(element));
 else std::memcpy(at,&value,sizeof(value));
 ++writes;
}
Il2CppObject* at(Il2CppArray* a,int index) {
 auto* element=a->obj.klass->element;auto* data=static_cast<unsigned char*>(arrayData(a))+elementSize(element)*static_cast<size_t>(index);
 if(element->value){auto* boxed=static_cast<Il2CppObject*>(std::calloc(1,sizeof(Il2CppObject)+elementSize(element)));boxed->klass=element;std::memcpy(boxed+1,data,elementSize(element));return boxed;}
 Il2CppObject* value=nullptr;std::memcpy(&value,data,sizeof(value));return value;
}
void originalGrantArmor(Il2CppObject* object,Il2CppObject* piece,const MethodInfo*) {
 ++grantCalls;auto* p=reinterpret_cast<FakePlayer*>(object);auto* item=reinterpret_cast<FakeItem*>(piece);p->statDefense+=item->defense;
}
void originalSets(Il2CppObject*,int32_t,const MethodInfo*){++updates;}
void originalPumpkinBonus(Il2CppObject*,const MethodInfo*){++vanillaPumpkinCalls;}
const MethodInfo* method(const std::string& signature) {return &methods.at(signature);}
void configureArmor() {
 configure();
 apiInstance.runtime_class_init=[](Il2CppClass*){};extraFields.reserve(32);
 auto add=[](Il2CppClass& cls,const char* name,Il2CppType* type,size_t offset,uint32_t flags=0){extraFields.push_back({name,type,offset,flags});cls.fields.push_back(&extraFields.back());return &extraFields.back();};
 add(playerClass,"statManaMax2",&intType,offsetof(FakePlayer,statManaMax2));add(playerClass,"maxMinions",&intType,offsetof(FakePlayer,maxMinions));
 add(playerClass,"statDefense",&intType,offsetof(FakePlayer,statDefense));add(playerClass,"lifeRegen",&intType,offsetof(FakePlayer,lifeRegen));
 add(playerClass,"buffImmune",&boolArrayType,offsetof(FakePlayer,buffImmune));add(playerClass,"armor",&itemArrayType,offsetof(FakePlayer,armor));
 add(buffIdClass,"OnFire",&intType,0,0x10);add(itemIdClass,"PumpkinHelmet",&intType,0,0x10);
 lookupField=add(bonusesClass,"SetsContaining",&rowType,0,0x10);add(bonusesClass,"All",&allType,0,0x10);
 const size_t base=valueMode?(boxedOffsets?sizeof(Il2CppObject):0):sizeof(Il2CppObject);
 add(setClass,"Head",&intType,base+offsetof(SetValue,head));add(setClass,"Body",&intType,base+offsetof(SetValue,body));
 add(setClass,"Legs",&intType,base+offsetof(SetValue,legs));add(setClass,"Effect",&delegateType,base+offsetof(SetValue,effect));
 if(valueMode){setClass.value=true;setClass.size=sizeof(SetValue);}
 for(const char* name:{"void GrantArmorBenefits(Item armorPiece)","void ApplyEquipFunctional(int itemSlot, Item currentItem)","void UpdateArmorSets(int i)",
  "void Pumpkin(Player player)","object GetValue(int index)","void SetValue(object value, int index)",
  "void Add(ArmorSetEffect Effect, string TextKey, PartType PrimaryPart, int Head, int Body, int Legs)","ToArray","bool Exists(string key)","void RemoveAt(int index)"}){auto& m=methods[name];names[&m]=name;}
 additionalClass=[](const bl::il2cpp::TypeRef& r)->Il2CppClass* {
  if(r.ns=="System"&&r.name=="Boolean")return &boolClass;
  if(r.ns=="System"&&r.name=="Array")return &arrayClass;
  if(r.ns=="Terraria.ID"&&r.name=="BuffID")return &buffIdClass;
  if(r.ns=="Terraria.ID"&&r.name=="ItemID")return &itemIdClass;
  if(r.ns=="Terraria.Localization"&&r.name=="Language")return localeMissing?nullptr:&languageClass;
  if(r.name=="ArmorSetBonuses")return r.nested.empty()?&bonusesClass:&benefitsClass;
  return nullptr;
 };
 additionalMethod=[](Il2CppClass*,const bl::il2cpp::Signature& s)->const MethodInfo* {auto i=methods.find(s.name);return i==methods.end()?nullptr:&i->second;};
 additionalInstall=[](const MethodInfo* m,void* replacement,void** original) {
  if(rejectEquipHook)return false;
  auto& c=chains[m];
  if(c.outer){*original=*c.originals.back();*c.originals.back()=replacement;}
  else {
   const auto& n=names.at(m);
   *original=n.find("GrantArmorBenefits")!=std::string::npos?reinterpret_cast<void*>(originalGrantArmor):n.find("ApplyEquipFunctional")!=std::string::npos?reinterpret_cast<void*>(originalEquip):
    n.find("UpdateArmorSets")!=std::string::npos?reinterpret_cast<void*>(originalSets):reinterpret_cast<void*>(originalPumpkinBonus);
   c.outer=replacement;
  }
  c.originals.push_back(original);return true;
 };
 apiInstance.method_is_instance=[](const MethodInfo* m){const auto& n=names.at(m);return (localeInstance&&n=="bool Exists(string key)")||(badAbi&&n.find("void Add(")==0)||n=="void RemoveAt(int index)"||n=="ToArray"||n.find("GetValue")!=std::string::npos||n.find("SetValue")!=std::string::npos||n.find("GrantArmorBenefits")!=std::string::npos||n.find("ApplyEquipFunctional")!=std::string::npos||n.find("UpdateArmorSets")!=std::string::npos;};
 apiInstance.method_get_return_type=[](const MethodInfo*)->const Il2CppType*{return localeWrongType?&intType:localeWide?&wideBoolType:&boolType;};
 apiInstance.method_get_param=[](const MethodInfo* m,uint32_t index)->const Il2CppType*{if(names.at(m)=="void RemoveAt(int index)"){assert(index==0);return &intType;}assert(names.at(m).find("void Add(")==0);return index==0?&delegateType:index==2?&partType:&intType;};
 apiInstance.class_is_enum=[](const Il2CppClass* c){return c==&partTypeClass;};
 apiInstance.object_get_class=[](Il2CppObject* o){return o->klass;};
 apiInstance.class_get_rank=[](const Il2CppClass* c){return c->rank;};
 apiInstance.class_get_element_class=[](Il2CppClass* c){return c->element;};
 apiInstance.class_get_method_from_name=[](Il2CppClass*,const char* name,int n)->const MethodInfo*{assert(std::string(name)=="ToArray"&&n==0);return method("ToArray");};
 apiInstance.array_new=[](Il2CppClass* c,uintptr_t n){assert(std::this_thread::get_id()==gameThread);return array(c,n);};
 apiInstance.string_new=[](const char* text)->Il2CppString*{auto* obj=new Il2CppObject{&stringClass,nullptr};strings[obj]=text;return reinterpret_cast<Il2CppString*>(obj);};
 apiInstance.gchandle_new=[](Il2CppObject* o,bool)->uint32_t{assert(std::this_thread::get_id()==gameThread);const auto id=nextRoot++;roots[id]=o;return id;};
 apiInstance.gchandle_free=[](uint32_t h){assert(std::this_thread::get_id()==gameThread);assert(roots.erase(h)==1);};
 apiInstance.gchandle_get_target=[](uint32_t h){return roots.at(h);};
 apiInstance.field_static_get_value=[](FieldInfo* f,void* out) {
  if(f->name==std::string("OnFire")){std::memcpy(out,&buffId,static_cast<size_t>(f->type->cls->size));return;}
  if(f->name==std::string("PumpkinHelmet")){std::memcpy(out,&pumpkinId,static_cast<size_t>(f->type->cls->size));return;}
  if(f==lookupField){*static_cast<Il2CppArray**>(out)=lookup;return;}
  if(f->name==std::string("All")){*static_cast<Il2CppObject**>(out)=&allObject;return;}
  assert(false);
 };
 apiInstance.field_static_set_value=[](FieldInfo* f,void* value){assert(f==lookupField);lookup=static_cast<Il2CppArray*>(value);};
 apiInstance.runtime_invoke=[](const MethodInfo* m,void* object,void** args,Il2CppObject** error)->Il2CppObject* {
  assert(std::this_thread::get_id()==gameThread);
  *error=nullptr;const auto& n=names.at(m);
  if(n=="bool Exists(string key)") {
   if(localeException){*error=&exception;return nullptr;}
   if(localeNull)return nullptr;
   localeResult.object.klass=localeWrongResult?&intClass:&boolClass;
   localeResult.value=localeAvailable&&localeCatalog.contains(strings.at(static_cast<Il2CppObject*>(args[0])));
   return &localeResult.object;
  }
  if(n=="void RemoveAt(int index)") {
   if(failRemove){failRemove=false;*error=&exception;return nullptr;}
   const auto index=*static_cast<int*>(args[0]);assert(index>=0&&static_cast<size_t>(index)<all.size());
   all.erase(all.begin()+index);return nullptr;
  }
  if(n=="ToArray") {
   if(failAfterAdd&&adds){failAfterAdd=false;*error=&exception;return nullptr;}
   auto* a=array(&setClass,all.size());for(size_t i=0;i<all.size();++i)put(a,static_cast<int>(i),&all[i]->obj);return &a->obj;
  }
  if(n=="object GetValue(int index)"){auto* a=static_cast<Il2CppArray*>(object);int index=*static_cast<int*>(args[0]);assert(index>=0&&static_cast<uintptr_t>(index)<a->length);return at(a,index);}
  if(n=="void SetValue(object value, int index)") {
   if(failArraySet){failArraySet=false;*error=&exception;return nullptr;}
   auto* a=static_cast<Il2CppArray*>(object);int index=*static_cast<int*>(args[1]);assert(index>=0&&static_cast<uintptr_t>(index)<a->length);put(a,index,static_cast<Il2CppObject*>(args[0]));return nullptr;
  }
  if(n.find("void Add(")==0) {
   if(failAdd){failAdd=false;*error=&exception;return nullptr;}
   auto* s=makeSet(*static_cast<int*>(args[3]),*static_cast<int*>(args[4]),*static_cast<int*>(args[5]),static_cast<Il2CppObject*>(args[0]));
   all.push_back(s);texts.push_back(strings.at(static_cast<Il2CppObject*>(args[1])));++adds;
   if(throwAfterAppend){throwAfterAppend=false;*error=&exception;}return nullptr;
  }
  assert(false);return nullptr;
 };
 auto* pumpkin=makeSet(900,901,902,&delegateObject);all.push_back(pumpkin);
 lookup=array(&rowClass,6197);auto* vanilla=array(&setClass,1);put(vanilla,0,&pumpkin->obj);put(lookup,900,&vanilla->obj);
}
}
int main(int argc,char** argv) {
 using namespace bl::csharp;
 assert(argc>=2);const std::string mode=argc>2?argv[2]:"";valueMode=mode=="--struct"||mode=="--boxed"||mode=="--full-example-boxed"||mode=="--lifecycle-boxed";boxedOffsets=mode=="--boxed"||mode=="--full-example-boxed"||mode=="--lifecycle-boxed";
 const bool fullExample=mode.starts_with("--full-example");
 auto slotFor=[&](const std::string& name){return name==(fullExample?"ExampleHelmet":"ContractHelmet")?0:name==(fullExample?"ExampleBreastplate":"ContractPlate")?1:2;};
 configureArmor();badAbi=mode=="--abi";
 if(mode=="--narrow-ids") {
  bl::il2cpp::findField(&buffIdClass,"OnFire")->type=&shortType;
  bl::il2cpp::findField(&itemIdClass,"PumpkinHelmet")->type=&shortType;
 }
 std::ifstream input(argv[1]);assert(input);std::string line,error;std::getline(input,line);
 std::vector<BasicItemEntry> entries;int next=6400;
 while(std::getline(input,line)) {
  if(fullExample) {
   const auto row=basicSplit(line,'\t');
   if(row.size()==4&&row[0]=="LOCALE") {
    auto key=unhex(row[2]);const std::string prefix="Mods.source.";
    if(key.starts_with(prefix))key="Mods.armor."+key.substr(prefix.size());
    localeCatalog[key][row[1]]=unhex(row[3]);
   }
   if(row.size()<2 || row[0]!="ITEM" || (row[1]!="ExampleHelmet" && row[1]!="ExampleBreastplate" && row[1]!="ExampleLeggings"))continue;
  }
  BasicItemEntry e;assert(parseBasicItemWire(basicSplit(line,'\t'),&e,&error));assert(e.armor.present);
  if(!fullExample)assert(e.behavior.empty());
  else assert(e.values[BasicWidth]==18 && e.values[BasicHeight]==18 && e.values[BasicValue]==50000 && e.values[BasicRare]==2);
  e.type=next++;identities[e.type]="armor/"+e.name;assert(registerBasicItemPlaceholder(e.type,e.name,&error));entries.push_back(e);
 }
 assert(entries.size()==3);
 // O modo full executa a camada real de armadura com o wire do ExampleMod
 // inteiro. Texturas são exercitadas pela suíte NativeItemPolicies separada.
 auto apply=[&] {
  if(!fullExample)return applyNativeBasicItemDefaults("armor",entries,&error);
  auto batch=prepareNativeArmorPolicies("armor",entries,&error);
  if(!batch)return false;
  commitNativeArmorPolicies(batch);return true;
 };
 if(mode=="--type"||mode=="--bounds"||mode=="--static") {
  auto* field=bl::il2cpp::findField(&playerClass,"statManaMax2");
  if(mode=="--type")field->type=&floatType;else if(mode=="--bounds")field->offset=sizeof(FakePlayer);else field->flags=0x10;
  assert(!apply());assert(pendingNativeArmorPolicies()==0);
  std::cout<<"NATIVE_ARMOR_INVALID_METADATA_REJECTED "<<mode<<"\n";return 0;
 }
 const auto vanilla=at(lookup,900);
 // Missing dependencies remain pending and never publish a success/bonus.
 const auto saved=identities[entries[1].type];
 if(!apply()){std::cerr<<error<<"\n";assert(false);}
 identities.erase(entries[1].type);assert(pendingNativeArmorPolicies()==1);
 pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&adds==0);
 identities[entries[1].type]=saved;
 if(fullExample) {
  pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&adds==0&&roots.empty());
  for(const auto& status:nativeArmorPolicyStatuses())if(!status.ready)assert(status.error=="Reason=ArmorLocalizationKeyPending");
  localeAvailable=true;
  localeException=mode=="--full-example-locale-exception";localeNull=mode=="--full-example-locale-null";
  localeWrongResult=mode=="--full-example-locale-result";localeWrongType=mode=="--full-example-locale-type";
  localeInstance=mode=="--full-example-locale-instance";localeMissing=mode=="--full-example-locale-missing";
  localeWide=mode=="--full-example-locale-wide";
  if(mode.starts_with("--full-example-locale-")) {
   pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&adds==0&&roots.empty());
   for(const auto& status:nativeArmorPolicyStatuses())if(!status.ready)
    assert(status.error==((localeException||localeNull||localeWrongResult)?"Reason=ArmorLocalizationLookupFailed":"Reason=ArmorLocalizationContractUnavailable"));
   localeException=localeNull=localeWrongResult=localeWrongType=localeInstance=localeMissing=localeWide=false;
  }
 }
 if(mode=="--abi") {
  pumpNativeArmorPoliciesOnGameThread();const auto statuses=nativeArmorPolicyStatuses();
  bool found=false;for(const auto& status:statuses)if(!status.ready){assert(status.error=="Reason=ArmorRegistryAbiMismatch");found=true;}
  assert(found&&adds==0);std::cout<<"NATIVE_ARMOR_INVALID_REGISTRY_ABI_REJECTED\n";return 0;
 }
 if(mode=="--set-retry")failArraySet=true;
 if(mode=="--add-retry")failAdd=true;
 if(mode=="--result-retry")failAfterAdd=true;
 if(mode=="--add-side-effect")throwAfterAppend=true;
 if(mode=="--lookup") {auto* savedLookup=lookup;lookup=nullptr;pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&adds==0);lookup=savedLookup;}
 pumpNativeArmorPoliciesOnGameThread();
 if(mode=="--set-retry") {
  FakePlayer p;p.armor=array(&itemClass,3);std::array<FakeItem,3> partialPieces{};
  for(const auto& e:entries){int slot=slotFor(e.name);partialPieces[slot].type=e.type;put(p.armor,slot,&partialPieces[slot].header);}
  auto skip=reinterpret_cast<void(*)(Il2CppObject*,const MethodInfo*)>(chains.at(method("void Pumpkin(Player player)")).outer);
  skip(&p.header,nullptr);assert(vanillaPumpkinCalls==0);
 }
 for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
 assert(pendingNativeArmorPolicies()==0&&adds==1&&texts[0]==(fullExample?"Mods.armor.ArmorSetBonus.ExampleArmorDisplay":"+20% damage"));assert(roots.size()==1);
 if(fullExample) {
  assert(description(texts[0])=="Increases dealt damage by 20%");
  const auto calls=adds;const auto key=texts[0];
  activeCulture="pt-BR";assert(description(key)=="Aumenta o dano causado em 20%");
  activeCulture="fr-FR";assert(description(key)=="Increases dealt damage by 20%");
  activeCulture="en-US";assert(adds==calls&&texts[0]==key);
 }
 assert(at(lookup,900)==vanilla);assert(lookup->length==6403);
 for(const auto& e:entries){auto* row=reinterpret_cast<Il2CppArray*>(at(lookup,e.type));assert(row&&row->length==1);}
 for(uintptr_t i=6197;i<lookup->length;++i)assert(at(lookup,static_cast<int>(i))!=nullptr);
 const auto before=adds;for(int i=0;i<250;++i)pumpNativeArmorPoliciesOnGameThread();assert(adds==before);
 auto grant=reinterpret_cast<void(*)(Il2CppObject*,Il2CppObject*,const MethodInfo*)>(chains.at(method("void GrantArmorBenefits(Item armorPiece)")).outer);
 auto update=reinterpret_cast<void(*)(Il2CppObject*,int32_t,const MethodInfo*)>(chains.at(method("void UpdateArmorSets(int i)")).outer);
 auto pumpkin=reinterpret_cast<void(*)(Il2CppObject*,const MethodInfo*)>(chains.at(method("void Pumpkin(Player player)")).outer);
 std::array<FakeItem,3> pieces{};
 for(const auto& e:entries) {
  const int slot=slotFor(e.name);
  auto& item=pieces[static_cast<size_t>(slot)];item.type=e.type;item.defense=static_cast<int16_t>(e.defense);
 }
 auto fresh=[&] {FakePlayer p;p.meleeDamage=p.rangedDamage=p.magicDamage=p.minionDamage=1;p.armor=array(&itemClass,10);p.buffImmune=array(&boolClass,100);
  for(size_t i=0;i<3;++i)put(p.armor,static_cast<int>(i),&pieces[i].header);
  return p;};
 auto player=fresh();for(auto& item:pieces)grant(&player.header,&item.header,nullptr);
 assert(player.statDefense==16&&player.statManaMax2==220&&player.maxMinions==2&&std::abs(player.moveSpeed-1.05f)<0.00001f);
 assert(static_cast<unsigned char*>(arrayData(player.buffImmune))[24]);
 pumpkin(&player.header,nullptr);assert(vanillaPumpkinCalls==0);
 update(&player.header,0,nullptr);assert(std::abs(player.meleeDamage-1.2f)<0.00001f&&std::abs(player.rangedDamage-1.2f)<0.00001f&&std::abs(player.magicDamage-1.2f)<0.00001f&&std::abs(player.minionDamage-1.2f)<0.00001f);
 const auto allocs=allocations;
 for(int i=0;i<10000;++i) {
  player.statDefense=0;player.statManaMax2=200;player.maxMinions=1;player.moveSpeed=1;
  for(auto& item:pieces)grant(&player.header,&item.header,nullptr);
  assert(player.statDefense==16&&player.statManaMax2==220&&player.maxMinions==2);
 }
 assert(allocations==allocs);
 auto partial=fresh();put(partial.armor,2,nullptr);update(&partial.header,0,nullptr);assert(partial.meleeDamage==1);
 pumpkin(&partial.header,nullptr);assert(vanillaPumpkinCalls==1);
 auto bad=fresh();bad.buffImmune->length=10;grant(&bad.header,&pieces[1].header,nullptr);
 assert(bad.statManaMax2==200&&bad.maxMinions==1); // Policy is atomic; vanilla defense already ran.
 bad=fresh();bad.statManaMax2=INT32_MAX;grant(&bad.header,&pieces[1].header,nullptr);assert(bad.maxMinions==1&&bad.statManaMax2==INT32_MAX);
 const auto oldLookup=lookup;lookup=array(&rowClass,6403);put(lookup,900,vanilla);
 for(int i=0;i<125;++i)pumpNativeArmorPoliciesOnGameThread();
 assert(lookup!=oldLookup&&adds==1);
 for(const auto& e:entries){auto* row=reinterpret_cast<Il2CppArray*>(at(lookup,e.type));assert(row&&row->length==1);}
 assert(pendingBasicItemBehaviors().empty()&&pendingNativeArmorPolicies()==0);
 if(fullExample||mode=="--lifecycle"||mode=="--lifecycle-boxed") {
  auto other=entries;
  for(auto& e:other){e.type+=100;identities[e.type]="other/"+e.name;assert(registerBasicItemPlaceholder(e.type,e.name,&error));}
  auto copiedLocales=localeCatalog;
  for(const auto& [key,value]:copiedLocales)if(key.starts_with("Mods.armor."))localeCatalog["Mods.other."+key.substr(11)]=value;
  auto otherBatch=prepareNativeArmorPolicies("other",other,&error);assert(otherBatch);commitNativeArmorPolicies(otherBatch);
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2&&all.size()==3);
  const auto otherSet=all.back();
  std::array<FakeItem,3> otherPieces{};
  for(const auto& e:other){auto& item=otherPieces[static_cast<size_t>(slotFor(e.name))];item.type=e.type;item.defense=static_cast<int16_t>(e.defense);}
  auto equippedOther=[&]{auto p=fresh();for(size_t i=0;i<3;++i)put(p.armor,static_cast<int>(i),&otherPieces[i].header);return p;};
  auto verifyOther=[&]{auto p=equippedOther();for(auto& item:otherPieces)grant(&p.header,&item.header,nullptr);update(&p.header,0,nullptr);assert(p.statManaMax2==220&&p.maxMinions==2&&std::abs(p.meleeDamage-1.2f)<0.00001f);};
  verifyOther();
  Il2CppObject foreignEffect{&legacyClass,nullptr};
  std::array<int,3> ownIds{};for(const auto& e:entries)ownIds[static_cast<size_t>(slotFor(e.name))]=e.type;
  auto* foreign=makeSet(ownIds[0],ownIds[1],ownIds[2],&foreignEffect);all.push_back(foreign);
  auto* shared=array(&setClass,2);auto* old=reinterpret_cast<Il2CppArray*>(at(lookup,ownIds[0]));
  put(shared,0,at(old,0));put(shared,1,&foreign->obj);put(lookup,ownIds[0],&shared->obj);
  auto* otherRow=at(lookup,other.front().type);
  std::thread([&]{retireNativeArmorPoliciesForMod("armor");}).join();
  assert(pendingNativeArmorPolicies()==1&&roots.size()==2);
  auto revoked=fresh();for(auto& item:pieces)grant(&revoked.header,&item.header,nullptr);update(&revoked.header,0,nullptr);
  assert(revoked.statManaMax2==200&&revoked.maxMinions==1&&revoked.meleeDamage==1&&revoked.moveSpeed==1);
  const auto pumpkinBefore=vanillaPumpkinCalls;pumpkin(&revoked.header,nullptr);assert(vanillaPumpkinCalls==pumpkinBefore);
  verifyOther();
  failArraySet=true;pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&roots.size()==2);
  failRemove=true;pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==1&&roots.size()==2);
  pumpkin(&revoked.header,nullptr);assert(vanillaPumpkinCalls==pumpkinBefore);
  for(const auto& status:nativeArmorPolicyStatuses())if(status.phase=="retiring")assert(status.mod=="armor"&&!status.error.empty());
  pumpNativeArmorPoliciesOnGameThread();assert(pendingNativeArmorPolicies()==0&&roots.size()==1&&all.size()==3);
  assert(all[1]==otherSet&&all[2]==foreign&&at(lookup,other.front().type)==otherRow&&at(lookup,900)==vanilla);
  auto* remains=reinterpret_cast<Il2CppArray*>(at(lookup,ownIds[0]));assert(remains->length==1);
  if(!valueMode)assert(at(remains,0)==&foreign->obj);
  verifyOther();
  auto ownBatch=prepareNativeArmorPolicies("armor",entries,&error);assert(ownBatch);commitNativeArmorPolicies(ownBatch);
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2&&all.size()==4);
  auto restored=fresh();update(&restored.header,0,nullptr);assert(std::abs(restored.meleeDamage-1.2f)<0.00001f);
  remains=reinterpret_cast<Il2CppArray*>(at(lookup,ownIds[0]));assert(remains->length==2);
  auto changed=entries;
  std::string replacementText;
  for(auto& e:changed)if(e.armor.hasSet) {
   e.armor.text+=" (reload)";replacementText=e.armor.text;
   if(!e.armor.textKey.empty()) {
    const auto previousKey="Mods.armor."+e.armor.textKey;e.armor.textKey+="Reload";
    replacementText="Mods.armor."+e.armor.textKey;localeCatalog[replacementText]=localeCatalog.at(previousKey);
   }
  }
  auto changedBatch=prepareNativeArmorPolicies("armor",changed,&error);assert(changedBatch);commitNativeArmorPolicies(changedBatch);
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2&&all.size()==4&&texts.back()==replacementText);verifyOther();
  commitNativeArmorPolicies(ownBatch);for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2&&all.size()==4);
  // A bounded queue absorbs many source replacements before the next frame.
  const auto beforeBurst=adds;
  std::thread([&]{for(int i=0;i<100;++i){retireNativeArmorPoliciesForMod("armor");commitNativeArmorPolicies(ownBatch);}}).join();
  assert(roots.size()==2);
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2&&all.size()==4&&adds==beforeBurst+1);verifyOther();
  // Full source snapshot removes a part even while its native ID remains reserved.
  std::vector<int> retained;for(const auto& e:entries)if(slotFor(e.name)!=1)retained.push_back(e.type);
  retainNativeArmorPoliciesForMod("armor",retained);pumpNativeArmorPoliciesOnGameThread();
  assert(roots.size()==1&&all.size()==3&&pendingNativeArmorPolicies()>0);verifyOther();
  commitNativeArmorPolicies(ownBatch);for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==2);
  // Add may append before throwing. Recover by identity after an external shift.
  commitNativeArmorPolicies(ownBatch);throwAfterAppend=true;pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()>0&&roots.size()==2);
  auto pos=std::find(all.begin(),all.end(),foreign);assert(pos!=all.end());all.erase(pos);
  std::thread([&]{retireNativeArmorPoliciesForMod("armor");}).join();
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==1&&all.size()==2&&all[1]==otherSet);verifyOther();
  commitNativeArmorPolicies(ownBatch);for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  auto dependent=entries.front();for(const auto& e:entries)if(slotFor(e.name)==0)dependent=e;
  dependent.type=6600;dependent.name="DependentHelmet";dependent.armor.head=dependent.name;
  for(const auto& e:entries) {
   if(slotFor(e.name)==1)dependent.armor.body="armor/"+e.name;
   if(slotFor(e.name)==2)dependent.armor.legs="armor/"+e.name;
  }
  identities[dependent.type]="dependent/"+dependent.name;assert(registerBasicItemPlaceholder(dependent.type,dependent.name,&error));
  for(const auto& [key,value]:copiedLocales)if(key.starts_with("Mods.armor."))localeCatalog["Mods.dependent."+key.substr(11)]=value;
  auto dependentBatch=prepareNativeArmorPolicies("dependent",{dependent},&error);assert(dependentBatch);commitNativeArmorPolicies(dependentBatch);
  for(int i=0;i<4&&pendingNativeArmorPolicies();++i)pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.size()==3&&all.size()==4);
  FakeItem dependentHead{};dependentHead.type=6600;auto dependentPlayer=fresh();put(dependentPlayer.armor,0,&dependentHead.header);
  update(&dependentPlayer.header,0,nullptr);assert(std::abs(dependentPlayer.meleeDamage-1.2f)<0.00001f);
  retireNativeArmorPoliciesForMod("armor");
  dependentPlayer.meleeDamage=1;update(&dependentPlayer.header,0,nullptr);assert(dependentPlayer.meleeDamage==1);
  for(int i=0;i<4;++i)pumpNativeArmorPoliciesOnGameThread();
  assert(roots.size()==1&&all.size()==2&&all[1]==otherSet&&pendingNativeArmorPolicies()==1);verifyOther();
  retireNativeArmorPoliciesForMod("dependent");
  retireNativeArmorPoliciesForMod("other");pumpNativeArmorPoliciesOnGameThread();
  assert(pendingNativeArmorPolicies()==0&&roots.empty()&&all.size()==1&&at(lookup,900)==vanilla);
  std::cout<<"NATIVE_ARMOR_SELECTIVE_RETIREMENT_RELOAD_AND_EMPTY_SNAPSHOT_PASS "<<mode<<"\n";
 }
 BasicArmorPolicy policy;
 assert(!parseBasicArmorPolicy("A1|statManaMax2:int:1.5|-|-|-|-|-|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|meleeDamage:mul:-1|-|-|-|-|-|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|-|OnFire,OnFire|-|-|-|-|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|-|-|bad//Head|Body|Legs|41|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|-|-|Head|Body|Legs|00|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|-|-|Head|Body|Legs|FF|-|-",&policy));
 assert(!parseBasicArmorPolicy("A1|-|-|Head|Body|Legs|C080|-|-",&policy));
 assert(parseBasicArmorPolicy("A1|-|-|Head|Body|Legs|43C3A920C3A167C3A36F|-|-",&policy));
 assert(parseBasicArmorPolicy("A2|-|-|Head|Body|Legs|41|-|-|ArmorSetBonus.Display",&policy)&&policy.textKey=="ArmorSetBonus.Display");
 for(const std::string& key:std::vector<std::string>{"", ".bad", "bad.", "bad..key", "bad/key", "bad key",std::string(73,'a')})
  assert(!parseBasicArmorPolicy("A2|-|-|Head|Body|Legs|41|-|-|"+key,&policy));
 assert(!parseBasicArmorPolicy("A2|-|-|-|-|-|-|-|-|ArmorSetBonus.Display",&policy));
 assert(!validArmorText(std::string(513,'x')));
 std::cout<<"NATIVE_ARMOR_REAL_WIRE_NO_QUICKJS_PIECES_IMMUNITY_SETS_TOOLTIP_RETRY_AND_RESET_PASS "<<mode<<"\n";
}
