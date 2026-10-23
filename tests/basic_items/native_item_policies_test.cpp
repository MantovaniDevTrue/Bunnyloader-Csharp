#define main existingNativeItemFixtureMain
#include "native_item_binding_test.cpp"
#undef main
#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/NativeDefaults.h"
#include "native_policy_support.h"
#include "experiments/csharp/NativeVisualRenderer.h"
#include "experiments/csharp/NativeStaticValue.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <thread>
#include <functional>

namespace {
struct AnimationIdList {Il2CppObject header;std::vector<int32_t> ids;};
struct World {Il2CppObject object;float position[2]{10,20};int width=20,height=30;bool active=true;FakeItem* inner=nullptr;};
struct Match {int part,head,body,legs;bool male;};
struct RenderPlayer {FakePlayer base;int who=0,width=20,height=42,direction=1;bool dead=false,invis=false;float rotation=0,position[2]{1010,1020};int frame[4]{0,112,40,56};Il2CppObject* mount=nullptr;};
struct SourceRectangle {bool hasValue=false;int value[4]{};};
struct DrawData {Il2CppObject* texture=nullptr;float position[2]{};SourceRectangle source;uint32_t color=0;float rotation=0,origin[2]{},scale[2]{};int effect=0,shader=0;};
struct DrawInfo {Il2CppObject object;Il2CppObject* player=nullptr;Il2CppArray* cache=nullptr;int count=0;float shadow=0,position[2]{1010,1020};int effect=0;};
std::map<std::string,Il2CppClass> classes;
std::map<Il2CppClass*,Il2CppType> types;
std::vector<FieldInfo> fields;
std::map<FieldInfo*,std::vector<unsigned char>> statics;
std::map<std::string,MethodInfo> methods;
std::map<const MethodInfo*,std::pair<std::string,bool>> methodNames;
std::map<const MethodInfo*,void*> hooks;
// Fake VM handles must outlive native policy/renderer globals during process teardown.
// Real IL2CPP owns this registry for the process lifetime too.
auto& roots=*new std::map<unsigned,Il2CppObject*>;
const auto nativePolicyGameThread=std::this_thread::get_id();
std::function<void()> duringCloneSetup,duringStaticRead;
std::map<int,Il2CppObject*> animations;
int animationRemovals=0;
std::function<void()> originalVoidAction;
bool animationRegisterAfterAddFault=false,animationRemoveBeforeFault=false,animationRemoveAfterFault=false,animationSnapshotFault=false,animationListReturnWrong=false;
FakePlayer localPlayer;
unsigned nextRoot=1;
Il2CppObject* failAnimationRootFor=nullptr;
int allocations=0,assetLoads=0,arrayWrites=0,movementCalls=0,lightCalls=0,animationConstructors=0;
bool android145=false,animationCtorFault=false,worldActiveProperty=true,activeReturnWrong=false,stopWorldOnUpdate=false;
int activeReads=0,worldUpdates=0;
std::string colorNamespace="Microsoft.Xna.Framework.Graphics";
Il2CppClass* alphaReturn=nullptr;Il2CppClass* alphaParameter=nullptr;
int wrongColorLookups=0;
std::map<Il2CppObject*,std::pair<int,int>> animationParameters;
bool failAsset=false,failSet=false,rawOffsets=true,screenPropertyOnly=false,androidFields=false,androidProperties=false,getterFault=false;
std::map<const MethodInfo*,FieldInfo*> propertyFields;
std::map<const MethodInfo*,bool> propertySetter;
float lastLight[3]{},lastPosition[2]{};
Il2CppObject exception{};
Il2CppClass* cls(const std::string& name,bool value=false,int size=0){auto [it,added]=classes.try_emplace(name,Il2CppClass{});if(added){it->second.name=it->first.c_str();it->second.value=value;it->second.size=size;}return &it->second;}
Il2CppType* type(Il2CppClass* c){auto& t=types[c];t.cls=c;return &t;}
Il2CppClass* arrayClass(Il2CppClass* element){auto* c=cls(std::string(element->name)+"[]");c->rank=1;c->element=element;return c;}
Il2CppArray* makeArray(Il2CppClass* element,uintptr_t length) {
 auto* array=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+length*(element->value?static_cast<size_t>(element->size):sizeof(void*))));
 array->obj.klass=arrayClass(element);array->length=length;++allocations;return array;
}
FieldInfo* field(Il2CppClass* c,const char* name,Il2CppClass* ft,size_t offset,bool isStatic=false){fields.push_back({name,type(ft),offset,isStatic?0x10u:0u});auto* f=&fields.back();c->fields.push_back(f);return f;}
template<class T>void constant(FieldInfo* f,const T& value){auto& bytes=statics[f];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));}
FieldInfo* arrayField(Il2CppClass* c,const char* name,Il2CppClass* element,int n){auto* f=field(c,name,arrayClass(element),0,true);auto* array=makeArray(element,static_cast<uintptr_t>(n));constant(f,array);return f;}
const MethodInfo* addMethod(Il2CppClass* c,const std::string& signature,bool instance){const auto key=std::string(c->name)+":"+signature;auto& m=methods[key];methodNames[&m]={signature,instance};return &m;}
const MethodInfo* getMethod(const std::string& c,const std::string& signature){return &methods.at(c+":"+signature);}
template<class Fn>Fn hook(const std::string& c,const std::string& signature){return reinterpret_cast<Fn>(hooks.at(getMethod(c,signature)));}
void originalMove(Il2CppObject*,const MethodInfo*){++movementCalls;}
void originalVoid(const MethodInfo*){if(originalVoidAction){auto action=std::move(originalVoidAction);originalVoidAction={};action();}}
void originalWorld(Il2CppObject* world,int32_t,const MethodInfo*){++worldUpdates;if(stopWorldOnUpdate)reinterpret_cast<World*>(world)->active=false;}
bool worldActive(Il2CppObject* world,const MethodInfo*){++activeReads;return reinterpret_cast<World*>(world)->active;}
void originalLayer(void*,const MethodInfo*){}
bool originalMounted(Il2CppObject*,const MethodInfo*){return true;}
uint32_t expectedNativeColor(uint32_t argb) {
 const uint8_t r=argb>>16,g=argb>>8,b=argb,a=argb>>24;
 return colorNamespace=="Microsoft.Xna.Framework"?uint32_t(r)|(uint32_t(g)<<8)|(uint32_t(b)<<16)|(uint32_t(a)<<24):
     uint32_t(a)|(uint32_t(b)<<8)|(uint32_t(g)<<16)|(uint32_t(r)<<24);
}
uint32_t originalColor(Il2CppObject*,uint32_t color,const MethodInfo*){return color;}
int32_t originalMatch(const void*,uint8_t*,const MethodInfo*){return 42;}
struct Vec2 {float x,y;};
Vec2 screenProperty(const MethodInfo*){return {1000.f,1000.f};}
void addLight(Vec2 position,float r,float g,float b,const MethodInfo*){++lightCalls;lastPosition[0]=position.x;lastPosition[1]=position.y;lastLight[0]=r;lastLight[1]=g;lastLight[2]=b;}
size_t width(Il2CppClass* c){return c->value?static_cast<size_t>(c->size):sizeof(void*);}
float jumpSpeedGetter(const MethodInfo*) {return 5.f;}
void property(Il2CppClass* c,const char* name,void* pointer=nullptr) {
 auto* f=bl::il2cpp::findField(c,name);assert(f);
 c->fields.erase(std::find(c->fields.begin(),c->fields.end(),f));
 auto* get=addMethod(c,std::string("property get_")+name+"()",false);
 const_cast<MethodInfo*>(get)->pointer=pointer;
 propertyFields[get]=f;propertySetter[get]=false;
 auto* set=addMethod(c,std::string("void set_")+name+"(property value)",false);
 propertyFields[set]=f;propertySetter[set]=true;
}
void testStaticIntegerWidths() {
 using namespace bl::csharp::native_static;
 struct Kind {const char* name;int bytes;int64_t low,high;};
 for(const auto& k:{Kind{"SByte",1,-128,127},{"Byte",1,0,255},{"Int16",2,-32768,32767},{"UInt16",2,0,65535},{"Int32",4,INT32_MIN,INT32_MAX},{"UInt32",4,0,UINT32_MAX}}) {
  auto* owner=cls(std::string("StaticTest")+k.name);auto* f=field(owner,"Value",cls(k.name,true,k.bytes),0,true);
  statics[f].resize(static_cast<size_t>(k.bytes));auto value=resolve(owner,"Value");int64_t number=0;
  assert(writeInteger(value,k.low)&&readInteger(value,&number)&&number==k.low);
  assert(writeInteger(value,k.high)&&readInteger(value,&number)&&number==k.high);
  const auto before=statics[f];assert(!writeInteger(value,k.low-1)&&!writeInteger(value,k.high+1)&&before==statics[f]);
 }
 auto* owner=cls("StaticWrongType");auto* f=field(owner,"Value",&floatClass,0,true);constant(f,2.f);
 int64_t number=123;assert(!readInteger(resolve(owner,"Value"),&number)&&number==123);
}
void configurePolicies() {
 configure();fields.reserve(200);storage.reserve(64);
 auto* byte=cls("Byte",true,1);auto* uint=cls("UInt32",true,4);auto* vector=cls("Vector2",true,8);auto* color=cls("Color",true,4);
 field(vector,"X",&floatClass,rawOffsets?0:sizeof(Il2CppObject));field(vector,"Y",&floatClass,(rawOffsets?0:sizeof(Il2CppObject))+4);
 alphaReturn=alphaParameter=color;
 const char* channelNames[]={"R","G","B","A"};
 for(size_t i=0;i<4;++i)field(color,channelNames[i],byte,(rawOffsets?0:sizeof(Il2CppObject))+(colorNamespace=="Microsoft.Xna.Framework"?i:3-i));
 field(&itemClass,"color",color,offsetof(FakeItem,color));field(&itemClass,"wornArmor",&legacyClass,offsetof(FakeItem,clonedReference));
 field(&playerClass,"hairColor",color,offsetof(FakePlayer,hairColor));field(&playerClass,"wingsLogic",&intClass,offsetof(FakePlayer,wingsLogic));
 field(&playerClass,"velocity",vector,offsetof(FakePlayer,velocity));field(&playerClass,"gravDir",&floatClass,offsetof(FakePlayer,gravDir));
 field(&playerClass,"wingTime",&floatClass,offsetof(FakePlayer,wingTime));field(&playerClass,"wingTimeMax",&intClass,offsetof(FakePlayer,wingTimeMax));
 field(&playerClass,"empressBrooch",&boolClass,offsetof(FakePlayer,empressBrooch));constant(field(&playerClass,"jumpSpeed",&floatClass,0,true),5.f);
 auto* main=cls("Main");constant(field(main,"essScale",&floatClass,0,true),2.f);
 auto* animationBase=cls("DrawAnimation",false,sizeof(Il2CppObject));
 arrayField(main,"itemAnimations",animationBase,7000);
 auto* animationListClass=cls("List`1",false,sizeof(AnimationIdList));
 auto* animationIds=new AnimationIdList;animationIds->header.klass=animationListClass;
 constant(field(main,"itemAnimationsRegistered",animationListClass,0,true),&animationIds->header);
 addMethod(animationListClass,"int[] ToArray()",true);addMethod(animationListClass,"void RemoveAt(int index)",true);
 auto* ids=cls("ItemID");constant(field(ids,"SoulofSight",&intClass,0,true),547);
 auto* itemSets=cls("ItemID/Sets");for(const char* n:{"AnimatesAsSoul","ItemIconPulse","ItemNoGravity"})arrayField(itemSets,n,&boolClass,7000);
 cls("ArmorIDs");
 auto* assets=cls("TextureAssets");auto* asset=cls("AssetTexture");bl::testing::assetClass=asset;
 auto* wingStats=cls("WingStats",true,24);
 const char* wingNames[]={"FlyTime","AccRunSpeedOverride","AccRunAccelerationMult","HasDownHoverStats","DownHoverSpeedOverride","DownHoverAccelerationMult"};
 const size_t offsets[]={0,4,8,12,16,20};
 for(size_t i=0;i<6;++i)field(wingStats,wingNames[i],i==0?&intClass:i==3?&boolClass:&floatClass,(rawOffsets?0:sizeof(Il2CppObject))+offsets[i]);
 std::array<unsigned char,24> defaults{};defaults[13]=0x5a;constant(field(wingStats,"Default",wingStats,0,true),defaults);
 for(const char* kind:{"Head","Body","Legs","Beard","Back","Wing","Shield","Shoe"}) {
  auto* k=cls(kind);constant(field(k,"Count",&intClass,0,true),2);auto* sets=cls(std::string(kind)+"/Sets");
  arrayField(sets,"UsesNewFramingCode",&boolClass,2);arrayField(sets,"UseHairColor",&boolClass,2);auto* hides=arrayField(sets,android145?"MissingHand":"HidesHands",&boolClass,2);
  auto* table=*reinterpret_cast<Il2CppArray**>(statics[hides].data());std::memset(arrayData(table),android145?0:1,2);
  if(std::string(kind)=="Wing")arrayField(sets,"Stats",wingStats,2);
 }
 for(const char* n:{"ArmorHead","ArmorBodyComposite","ArmorBody","FemaleBody","ArmorArm","ArmorLeg","AccBeard","AccBack","Wings","AccShield","AccShoes"})arrayField(assets,n,asset,2);
 for(const char* n:{"headType","bodyType","legType"})arrayField(&itemClass,n,&intClass,2);
 auto* request=cls("Player/SetMatchRequest",true,sizeof(Match));
 for(const auto& pair:std::initializer_list<std::pair<const char*,size_t>>{{"ArmorSlotRequested",offsetof(Match,part)},{"Head",offsetof(Match,head)},{"Body",offsetof(Match,body)},{"Legs",offsetof(Match,legs)}})field(request,pair.first,&intClass,(rawOffsets?0:sizeof(Il2CppObject))+pair.second);
 field(request,"Male",&boolClass,(rawOffsets?0:sizeof(Il2CppObject))+offsetof(Match,male));
 auto* entity=cls("Entity",false,offsetof(World,inner));
 field(entity,"position",vector,offsetof(World,position));field(entity,"width",&intClass,offsetof(World,width));field(entity,"height",&intClass,offsetof(World,height));
 auto* world=cls("WorldItem",false,sizeof(World));world->parent=entity;field(world,"inner",&itemClass,offsetof(World,inner));
 if(worldActiveProperty){auto* active=addMethod(world,"bool get_active()",true);const_cast<MethodInfo*>(active)->pointer=reinterpret_cast<void*>(worldActive);}
 else field(world,"active",&boolClass,offsetof(World,active));
 auto* array=cls("Array");
 addMethod(array,"void Copy(Array sourceArray, Array destinationArray, int length)",false);addMethod(array,"void SetValue(object value, int index)",true);
 addMethod(&itemClass,"void .ctor()",true);addMethod(&itemClass,"void SetDefaults(int Type, ItemVariant variant)",true);addMethod(&itemClass,"Color GetAlpha(Color newColor)",true);
 addMethod(&playerClass,"void WingMovement()",true);addMethod(&playerClass,"int SetMatch(Player.SetMatchRequest request, ref bool somethingSpecial)",false);
 addMethod(main,"Player get_LocalPlayer()",false);addMethod(main,"void RegisterItemAnimation(int index, DrawAnimation animation)",false);addMethod(main,"void InitializeItemAnimations()",false);
 cls("DrawAnimationVertical",false,sizeof(Il2CppObject))->parent=animationBase;
 addMethod(cls("DrawAnimationVertical",false,sizeof(Il2CppObject)),android145?"void .ctor(int ticksperframe, int frameCount, bool pingPong)":"void .ctor(int ticksperframe, int frameCount)",true);
 addMethod(cls("WingStatsInitializer"),"void Load()",false);addMethod(world,"void UpdateItem(int i)",true);
 const auto* light=addMethod(cls("Lighting"),"void AddLight(Vector2 position, float r, float g, float b)",false);const_cast<MethodInfo*>(light)->pointer=reinterpret_cast<void*>(addLight);
 additionalClass=[](const bl::il2cpp::TypeRef& r)->Il2CppClass* {
  if(r.name=="Color"&&r.ns!=colorNamespace){++wrongColorLookups;return nullptr;}
  if(r.name=="Int32")return &intClass;
  if(r.name=="Boolean")return &boolClass;
  if(r.name=="Item")return &itemClass;
  if(r.name=="Player"&&r.nested.empty())return &playerClass;
  const auto key=r.name=="ArmorIDs"?(r.nested.empty()?std::string("ArmorIDs"):std::string(r.nested)):std::string(r.name)+(r.nested.empty()?"":"/"+std::string(r.nested));
  const auto it=classes.find(key);return it==classes.end()?nullptr:&it->second;
 };
 additionalMethod=[](Il2CppClass* c,const bl::il2cpp::Signature& s)->const MethodInfo* {const auto it=methods.find(std::string(c->name)+":"+s.name);return it==methods.end()?nullptr:&it->second;};
 additionalInstall=[](const MethodInfo* m,void* replacement,void** original) {
  if(m==&functionalMethod){*original=reinterpret_cast<void*>(originalEquip);functionalHook=reinterpret_cast<EquipCallback>(replacement);++hookInstalls;return true;}
  hooks[m]=replacement;const auto& n=methodNames.at(m).first;
  if(n.find("DrawPlayer_")!=std::string::npos){*original=reinterpret_cast<void*>(originalLayer);return true;}
  *original=n.find("WingMovement")!=std::string::npos?reinterpret_cast<void*>(originalMove):n.find("SetMatch")!=std::string::npos?reinterpret_cast<void*>(originalMatch):
   n.find("GetAlpha")!=std::string::npos?reinterpret_cast<void*>(originalColor):n.find("UpdateItem")!=std::string::npos?reinterpret_cast<void*>(originalWorld):reinterpret_cast<void*>(originalVoid);
  return true;
 };
 apiInstance.class_get_rank=[](const Il2CppClass* c){return c->rank;};apiInstance.class_get_element_class=[](Il2CppClass* c){return c->element;};
 apiInstance.object_get_class=[](Il2CppObject* o){return o->klass;};apiInstance.array_new=[](Il2CppClass* c,uintptr_t n){return makeArray(c,n);};
 apiInstance.class_get_method_from_name=[](Il2CppClass* c,const char* name,int n)->const MethodInfo* {
  for(const auto& [m,f]:propertyFields){(void)f;const auto& key=methodNames.at(m).first;
   if(methods.contains(std::string(c->name)+":"+key)&&key.find(std::string(name)+"(")!=std::string::npos&&n==(propertySetter.at(m)?1:0))return m;}
  return nullptr;
 };
 apiInstance.method_get_param_count=[](const MethodInfo* m){
  const auto& name=methodNames.at(m).first;
  if(name=="int[] ToArray()")return 0u;
  if(name=="void RemoveAt(int index)")return 1u;
  if(name=="bool get_active()")return 0u;
  return name=="Color GetAlpha(Color newColor)"?1u:propertySetter.at(m)?1u:0u;
 };
 apiInstance.method_get_param=[](const MethodInfo* m,uint32_t)->const Il2CppType* {
  if(methodNames.at(m).first=="void RemoveAt(int index)")return &intType;
  return methodNames.at(m).first=="Color GetAlpha(Color newColor)"?type(alphaParameter):propertyFields.at(m)->type;
 };
 apiInstance.method_get_return_type=[](const MethodInfo* m)->const Il2CppType* {
  if(methodNames.at(m).first=="int[] ToArray()")return type(arrayClass(animationListReturnWrong?&boolClass:&intClass));
  if(methodNames.at(m).first=="void RemoveAt(int index)")return type(cls("Void",true,0));
  if(methodNames.at(m).first=="bool get_active()")return activeReturnWrong?&intType:&boolType;
  if(methodNames.at(m).first=="Color GetAlpha(Color newColor)")return type(alphaReturn);
  return propertySetter.at(m)?type(cls("Void",true,0)):propertyFields.at(m)->type;
 };
 apiInstance.class_get_namespace=[](Il2CppClass* c){if(c==cls("List`1"))return "System.Collections.Generic";return c==cls("Color")?colorNamespace.c_str():"System";};
 apiInstance.class_get_parent=[](Il2CppClass* c){return c->parent;};
 apiInstance.field_get_name=[](FieldInfo* f){return f->name;};apiInstance.type_get_name=[](const Il2CppType* t){return ::strdup(t->cls->name);};apiInstance.il2cpp_free=std::free;
 apiInstance.object_new=[](Il2CppClass* c){auto* o=static_cast<Il2CppObject*>(std::calloc(1,static_cast<size_t>(c->size)));o->klass=c;++allocations;return o;};
 apiInstance.runtime_class_init=[](Il2CppClass*){};
 apiInstance.method_is_instance=[](const MethodInfo* m){return methodNames.at(m).second;};
 apiInstance.gchandle_new=[](Il2CppObject* o,bool){assert(std::this_thread::get_id()==nativePolicyGameThread);if(o&&o==failAnimationRootFor)return 0u;roots[nextRoot]=o;return nextRoot++;};apiInstance.gchandle_free=[](unsigned h){assert(std::this_thread::get_id()==nativePolicyGameThread);roots.erase(h);};apiInstance.gchandle_get_target=[](unsigned h){assert(std::this_thread::get_id()==nativePolicyGameThread);return roots.at(h);};
 apiInstance.field_static_get_value=[](FieldInfo* f,void* out){if(duringStaticRead&&std::string(f->name)=="AnimatesAsSoul"){auto action=std::move(duringStaticRead);duringStaticRead={};action();}if(f->name==std::string("Item1")){void* value=&soundObject;std::memcpy(out,&value,sizeof(value));return;}const auto& v=statics.at(f);std::memcpy(out,v.data(),v.size());};
 apiInstance.field_static_set_value=[](FieldInfo* f,void* value){auto* c=f->type->cls;auto& v=statics[f];v.resize(width(c));if(c->value)std::memcpy(v.data(),value,v.size());else std::memcpy(v.data(),&value,v.size());};
 apiInstance.runtime_invoke=[](const MethodInfo* m,void* self,void** args,Il2CppObject** error)->Il2CppObject* {
  *error=nullptr;const auto& n=methodNames.at(m).first;
  if(propertyFields.contains(m)) {
   if(getterFault&&!propertySetter.at(m)){*error=&exception;return nullptr;}
   auto* f=propertyFields.at(m);const size_t bytes=width(f->type->cls);auto& storage=statics.at(f);
   if(propertySetter.at(m)) {
    if(f->type->cls->value)std::memcpy(storage.data(),args[0],bytes);
    else std::memcpy(storage.data(),&args[0],bytes);
    return nullptr;
   }
   if(!f->type->cls->value){Il2CppObject* value=nullptr;std::memcpy(&value,storage.data(),sizeof(value));return value;}
   auto* box=static_cast<Il2CppObject*>(std::calloc(1,sizeof(Il2CppObject)+bytes));box->klass=f->type->cls;++allocations;
   std::memcpy(reinterpret_cast<char*>(box)+sizeof(Il2CppObject),storage.data(),bytes);return box;
  }
  if(n=="int[] ToArray()") {
   if(animationSnapshotFault){*error=&exception;return nullptr;}
   const auto& values=static_cast<AnimationIdList*>(self)->ids;
   auto* snapshot=makeArray(&intClass,values.size());
   if(!values.empty())std::memcpy(arrayData(snapshot),values.data(),values.size()*sizeof(int32_t));
   return reinterpret_cast<Il2CppObject*>(snapshot);
  }
  else if(n=="void RemoveAt(int index)") {
   if(animationRemoveBeforeFault){animationRemoveBeforeFault=false;*error=&exception;return nullptr;}
   auto& values=static_cast<AnimationIdList*>(self)->ids;const auto index=*static_cast<int*>(args[0]);
   assert(index>=0&&static_cast<size_t>(index)<values.size());values.erase(values.begin()+index);++animationRemovals;
   if(animationRemoveAfterFault){animationRemoveAfterFault=false;*error=&exception;}
  }
  else if(n.find("void Copy(")==0){auto* from=static_cast<Il2CppArray*>(args[0]);auto* to=static_cast<Il2CppArray*>(args[1]);const auto count=*static_cast<int*>(args[2]);std::memcpy(arrayData(to),arrayData(from),static_cast<size_t>(count)*width(from->obj.klass->element));++arrayWrites;}
  else if(n.find("void SetValue(")==0){if(failSet){failSet=false;*error=&exception;return nullptr;}auto* a=static_cast<Il2CppArray*>(self);const int index=*static_cast<int*>(args[1]);assert(index>=0&&static_cast<uintptr_t>(index)<a->length);std::memcpy(static_cast<char*>(arrayData(a))+sizeof(void*)*static_cast<size_t>(index),&args[0],sizeof(void*));++arrayWrites;}
  else if(n.find("void SetDefaults(")==0){assert(*static_cast<int*>(args[0])==547);auto* item=static_cast<FakeItem*>(self);item->values[0]=28;item->values[1]=20;item->values[2]=99;item->values[3]=777;item->values[4]=3;item->values[5]=23;item->values[6]=7;item->defense=2;item->knockBack=3.5f;item->flags[0]=true;item->flags[2]=true;item->flags[5]=true;item->clonedReference=&soundObject;void* sound=&soundObject;std::memcpy(item->sound,&sound,sizeof(void*));if(duringCloneSetup){auto action=std::move(duringCloneSetup);duringCloneSetup={};action();}}
  else if(n.find("get_LocalPlayer")!=std::string::npos)return &localPlayer.header;
  else if(n.find("void .ctor(int ticksperframe") == 0) {
   ++animationConstructors;assert(self&&*static_cast<int*>(args[0])>0&&*static_cast<int*>(args[1])>1);
   if(android145)assert(*static_cast<uint8_t*>(args[2])==0);
   if(animationCtorFault){*error=&exception;return nullptr;}
   animationParameters[static_cast<Il2CppObject*>(self)]={*static_cast<int*>(args[0]),*static_cast<int*>(args[1])};
  }
  else if(n.find("RegisterItemAnimation")!=std::string::npos){
   auto* object=static_cast<Il2CppObject*>(args[1]);assert(animationParameters.contains(object));const int contentId=*static_cast<int*>(args[0]);
   Il2CppObject* list=nullptr;apiInstance.field_static_get_value(bl::il2cpp::findField(cls("Main"),"itemAnimationsRegistered"),&list);
   auto& ids=reinterpret_cast<AnimationIdList*>(list)->ids;
   if(std::find(ids.begin(),ids.end(),contentId)==ids.end())ids.push_back(contentId);
   if(animationRegisterAfterAddFault){*error=&exception;return nullptr;}
   Il2CppArray* table=nullptr;apiInstance.field_static_get_value(bl::il2cpp::findField(cls("Main"),"itemAnimations"),&table);
   assert(table&&table->length>static_cast<uintptr_t>(contentId));
   reinterpret_cast<Il2CppObject**>(arrayData(table))[contentId]=object;animations[contentId]=object;
  }
  return nullptr;
 };
 bl::testing::loadAsset=[](const std::string&)->Il2CppObject* {++assetLoads;if(failAsset){failAsset=false;return nullptr;}auto* object=new Il2CppObject{bl::testing::assetClass,nullptr};
  bl::testing::assetSizes[object]={52,72};return object;};
 (void)byte;(void)uint;
}
void configureRenderer() {
 const size_t box=rawOffsets?0:sizeof(Il2CppObject);playerClass.size=sizeof(RenderPlayer);
 const auto base=offsetof(RenderPlayer,base);
 (void)base;
 for(const auto& pair:std::initializer_list<std::pair<const char*,size_t>>{{"whoAmI",offsetof(RenderPlayer,who)},{"width",offsetof(RenderPlayer,width)},
  {"height",offsetof(RenderPlayer,height)},{"direction",offsetof(RenderPlayer,direction)}})field(&playerClass,pair.first,&intClass,pair.second);
 field(&playerClass,"dead",&boolClass,offsetof(RenderPlayer,dead));field(&playerClass,"invis",&boolClass,offsetof(RenderPlayer,invis));field(&playerClass,"bodyRotation",&floatClass,offsetof(RenderPlayer,rotation));
 field(&playerClass,"position",cls("Vector2"),offsetof(RenderPlayer,position));
 auto* rectangle=cls("Rectangle",true,16);
 const char* names[]={"X","Y","Width","Height"};for(size_t i=0;i<4;++i)field(rectangle,names[i],&intClass,box+i*4);
 field(&playerClass,"bodyFrame",rectangle,offsetof(RenderPlayer,frame));auto* mount=cls("Mount");field(&playerClass,"mount",mount,offsetof(RenderPlayer,mount));
 auto* active=addMethod(mount,"bool get_Active()",true);const_cast<MethodInfo*>(active)->pointer=reinterpret_cast<void*>(originalMounted);
 auto* source=cls("NullableRectangle",true,sizeof(SourceRectangle));field(source,"hasValue",&boolClass,box+offsetof(SourceRectangle,hasValue));field(source,"value",rectangle,box+offsetof(SourceRectangle,value));
 auto* data=cls("DrawData",true,sizeof(DrawData));field(data,"texture",bl::testing::assetClass,box+offsetof(DrawData,texture));
 field(data,"position",cls("Vector2"),box+offsetof(DrawData,position));field(data,"sourceRect",source,box+offsetof(DrawData,source));
 field(data,"color",cls("Color"),box+offsetof(DrawData,color));field(data,"rotation",&floatClass,box+offsetof(DrawData,rotation));
 field(data,"origin",cls("Vector2"),box+offsetof(DrawData,origin));field(data,"scale",cls("Vector2"),box+offsetof(DrawData,scale));
 field(data,"effect",&intClass,box+offsetof(DrawData,effect));field(data,"shader",&intClass,box+offsetof(DrawData,shader));
 auto* info=cls("PlayerDrawSet",false,sizeof(DrawInfo));field(info,"drawPlayer",&playerClass,offsetof(DrawInfo,player));field(info,"DrawDataCache",arrayClass(data),offsetof(DrawInfo,cache));
 field(info,"DrawDataCacheCount",&intClass,offsetof(DrawInfo,count));field(info,"shadow",&floatClass,offsetof(DrawInfo,shadow));field(info,"Position",cls("Vector2"),offsetof(DrawInfo,position));field(info,"playerEffect",&intClass,offsetof(DrawInfo,effect));
 if(screenPropertyOnly){
   auto* getter=addMethod(cls("Main"),"Vector2 get_screenPosition()",false);
   const_cast<MethodInfo*>(getter)->pointer=reinterpret_cast<void*>(screenProperty);
  }else{
   std::array<float,2> screen{1000,1000};
   constant(field(cls("Main"),"screenPosition",cls("Vector2"),0,true),screen);
  }
 auto* layers=cls("PlayerDrawLayers");for(const char* signature:{"void DrawPlayer_10_BackAcc(PlayerDrawSet drawinfo)","void DrawPlayer_25_Shield(PlayerDrawSet drawinfo)","void DrawPlayer_35_IceBarrier(PlayerDrawSet drawinfo)"})addMethod(layers,signature,false);
}
void testRenderer(const std::string& root) {
 using namespace bl::csharp;configureRenderer();bl::testing::modContentRoot=root+"/content";
 ManagedVisualCommand command;command.kind=ManagedVisualKind::PlayerOverlay;command.playerIndex=0;command.effectId=1;
 command.texture="mod:Content/Effects/MysticWardBarrier.png";command.color=0xff66ccff;command.opacity=0.78f;command.scale=1.1f;command.style=2;
 bl::testing::visualCommands={command};requestNativeVisualRenderer(command.texture);assert(pendingNativeVisualRenderer()==1);
 auto* shaderField=bl::il2cpp::findField(cls("DrawData"),"shader");const auto savedOffset=shaderField->offset;shaderField->offset=sizeof(DrawData)+sizeof(Il2CppObject)+4;
 pumpNativeVisualRendererOnGameThread();assert(!nativeVisualRendererStatus().ready);
 shaderField->offset=savedOffset;
 for(int retry=0;retry<31&&!nativeVisualRendererStatus().ready;++retry)pumpNativeVisualRendererOnGameThread();
 auto state=nativeVisualRendererStatus();if(!state.ready)std::cerr<<"RENDERER "<<state.error<<"\n";assert(state.ready&&state.assets==1&&pendingNativeVisualRenderer()==0);
 RenderPlayer player;player.base.header.klass=&playerClass;player.base.velocity[0]=3.2f;
 DrawInfo info;info.object.klass=cls("PlayerDrawSet");info.player=&player.base.header;info.cache=makeArray(cls("DrawData"),32);
 auto barrier=hook<void(*)(void*,const MethodInfo*)>("PlayerDrawLayers","void DrawPlayer_35_IceBarrier(PlayerDrawSet drawinfo)");
 barrier(&info,nullptr);assert(info.count==2);auto* first=static_cast<DrawData*>(arrayData(info.cache));
 assert(first[0].position[0]==20&&first[0].position[1]==41&&first[0].source.hasValue&&first[0].source.value[2]==52&&first[0].source.value[3]==72);
 assert(first[0].origin[0]==26&&first[0].origin[1]==36&&first[0].color==expectedNativeColor(0x2866ccff)&&first[1].color==expectedNativeColor(0xbb66ccff));
 const auto allocated=allocations,loaded=assetLoads;
 for(int i=0;i<10000;++i){info.count=0;barrier(&info,nullptr);assert(info.count==2);}
 assert(allocations==allocated&&assetLoads==loaded);
 player.dead=true;info.count=0;barrier(&info,nullptr);assert(info.count==0);player.dead=false;
 player.invis=true;barrier(&info,nullptr);assert(info.count==0);player.invis=false;
 info.shadow=0.5f;barrier(&info,nullptr);assert(info.count==0);info.shadow=0;
 info.count=31;barrier(&info,nullptr);assert(info.count==32&&nativeVisualRendererStatus().rejected>0);
 command.style=1;command.kind=ManagedVisualKind::EquipmentSprite;command.color=0xffffffff;command.opacity=1;
 bl::testing::visualCommands={command};info.count=0;
 auto shield=hook<void(*)(void*,const MethodInfo*)>("PlayerDrawLayers","void DrawPlayer_25_Shield(PlayerDrawSet drawinfo)");shield(&info,nullptr);
 assert(info.count==1&&first[0].position[0]==16&&first[0].position[1]==41);
 player.direction=-1;info.count=0;shield(&info,nullptr);assert(first[0].position[0]==24);
 command.color=0xff0000ff;bl::testing::visualCommands={command};info.count=0;shield(&info,nullptr);
 assert(info.count==1&&first[0].color==expectedNativeColor(0xff0000ff));
 command.color=0x00ff0000;bl::testing::visualCommands={command};info.count=0;shield(&info,nullptr);assert(info.count==0);
 command.color=0xffffffff;
 command.style=3;bl::testing::visualCommands={command};info.count=0;
 auto back=hook<void(*)(void*,const MethodInfo*)>("PlayerDrawLayers","void DrawPlayer_10_BackAcc(PlayerDrawSet drawinfo)");back(&info,nullptr);assert(info.count==1);
 player.dead=true;info.count=0;back(&info,nullptr);assert(info.count==0);
 player.dead=false;player.base.velocity[0]=0;back(&info,nullptr);assert(info.count==0);
 resetNativeVisualRenderer();bl::testing::visualCommands.clear();info.count=0;back(&info,nullptr);assert(info.count==0);
 std::cout<<"NATIVE_VISUAL_RENDERER_NO_QUICKJS_BARRIER_SHIELD_TRAIL_GC_AND_10000_DRAWS_PASS\n";
}
int slot(int type,const std::string& kind) {
 FakeItem item{};assert(bl::csharp::applyBasicItemToObject(&item.header,type));
 return kind=="Head"?item.headSlot:kind=="Body"?item.bodySlot:kind=="Legs"?item.legSlot:kind=="Wings"?item.wingSlot:item.beardSlot;
}
}
int main(int argc,char** argv) {
 using namespace bl::csharp;assert(argc>=3);
 const auto mode=argc>3?std::string(argv[3]):"";
 worldActiveProperty=mode!="--world-active-field";
 if(mode=="--framework-color")colorNamespace="Microsoft.Xna.Framework";
 android145=mode=="--android-145"||mode=="--android-145-boxed"||mode=="--animation-ctor-exception";
 animationCtorFault=mode=="--animation-ctor-exception";
 androidProperties=mode=="--android-properties"||mode=="--android-properties-boxed"||mode=="--getter-exception";
 androidFields=android145||mode=="--android-fields"||androidProperties||mode=="--count-overflow";
 rawOffsets=mode!="--boxed"&&mode!="--android-properties-boxed"&&mode!="--android-145-boxed";screenPropertyOnly=mode=="--screen-property";configurePolicies();
 if(androidFields) {
  auto* count=bl::il2cpp::findField(cls("Beard"),"Count");count->type=type(cls("Byte",true,1));constant<uint8_t>(count,2);
  auto* clone=bl::il2cpp::findField(cls("ItemID"),"SoulofSight");clone->type=type(cls("Int16",true,2));constant<int16_t>(clone,547);
  classes.erase("Shield/Sets");
  auto* hands=bl::il2cpp::findField(cls("Body/Sets"),android145?"MissingHand":"HidesHands");
  auto* small=makeArray(&boolClass,1);static_cast<uint8_t*>(arrayData(small))[0]=android145?0:1;constant(hands,small);
 }
 if(androidProperties) {
  property(cls("Beard"),"Count");property(cls("Body/Sets"),"HidesHands");property(cls("ItemID"),"SoulofSight");
  property(&playerClass,"jumpSpeed",reinterpret_cast<void*>(jumpSpeedGetter));
 }
 if(mode=="--color-argument-mismatch")alphaParameter=cls("WrongColor",true,4);
 if(mode=="--color-return-mismatch")alphaReturn=cls("WideColor",true,8);
 if(mode=="--world-active-static")methodNames.at(getMethod("WorldItem","bool get_active()")).second=false;
 if(mode=="--world-active-type")activeReturnWrong=true;
 if(mode=="--world-active-pointer")const_cast<MethodInfo*>(getMethod("WorldItem","bool get_active()"))->pointer=nullptr;
 testStaticIntegerWidths();
 getterFault=mode=="--getter-exception";
 if(mode=="--count-overflow") {auto* f=bl::il2cpp::findField(cls("Beard"),"Count");f->type=type(cls("SByte",true,1));constant<int8_t>(f,127);}
 std::ifstream input(argv[1]);assert(input);std::string line,error;std::getline(input,line);std::vector<BasicItemEntry> all;int next=6400;
 while(std::getline(input,line)) {
  auto parts=basicSplit(line,'\t');if(parts[0]!="ITEM")continue;BasicItemEntry entry;assert(parseBasicItemWire(parts,&entry,&error));
  if(entry.behavior.empty())continue;
  entry.type=next++;entry.modId="mod";entry.modDir=argv[2];identities[entry.type]="mod/"+entry.name;
  assert(registerBasicItemPlaceholder(entry.type,entry.name,&error,argv[2]));auto base=entry;base.behavior.clear();base.armor={};base.recipes.clear();
  assert(applyNativeBasicItemDefaults("mod",{base},&error));
  // SourceLoader installs defense separately after the basic defaults batch.
  if(entry.defenseSpecified || entry.defense>0)assert(setBasicItemDefense(entry.type,entry.defense,&error));
  all.push_back(entry);
 }
 assert(!all.empty());std::shared_ptr<NativeItemPolicyBatch> batch;assert(prepareNativeItemPolicies(all,&batch,&error));commitNativeItemPolicies(batch);
 auto* headField=bl::il2cpp::findField(&itemClass,"headSlot");const auto savedHead=*headField;
 if(mode=="--bounds")headField->offset=sizeof(FakeItem);
 if(mode=="--type")headField->type=&floatType;
 if(mode=="--static")headField->flags=0x10;
 failAsset=true;failSet=true;
 for(int i=0;i<20&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 if(mode=="--bounds"||mode=="--type"||mode=="--static") {
  assert(pendingNativeItemPolicies()>0);
  bool rejected=false;for(const auto& status:nativeItemPolicyStatus())if(!status.ready){assert(status.error.find("Rejected")!=std::string::npos);rejected=true;}
  assert(rejected);*headField=savedHead;
  for(int i=0;i<10&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 }
 if(mode=="--getter-exception"||mode=="--count-overflow") {
  assert(pendingNativeItemPolicies()>0);
  if(mode=="--count-overflow") {
   bool found=false;for(const auto& status:nativeItemPolicyStatus())if(status.name=="ExampleBeard"){found=status.error.find("EquipmentCountOverflow")!=std::string::npos;}
   int8_t unchanged=0;apiInstance.field_static_get_value(bl::il2cpp::findField(cls("Beard"),"Count"),&unchanged);
   assert(found&&unchanged==127);constant<int8_t>(bl::il2cpp::findField(cls("Beard"),"Count"),2);
  }
  getterFault=false;for(int i=0;i<30&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 }
 if(animationCtorFault) {
  bool rejected=false;for(const auto& status:nativeItemPolicyStatus())if(status.name=="ExampleSoul"){
   assert(!status.ready&&status.error=="Reason=AnimationConstructor3Failed");rejected=true;
  }
  assert(rejected&&animations.empty());animationCtorFault=false;
  for(int i=0;i<30&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 }
 if(mode=="--color-argument-mismatch"||mode=="--color-return-mismatch") {
  bool rejected=false;for(const auto& status:nativeItemPolicyStatus())if(status.name=="ExampleSoul"){
   assert(!status.ready&&status.error=="Reason=ColorAbiMismatch");rejected=true;
  }
  assert(rejected&&!hooks.contains(getMethod("Item","Color GetAlpha(Color newColor)")));
  alphaReturn=alphaParameter=cls("Color");
  for(int i=0;i<30&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 }
 if(mode=="--world-active-static"||mode=="--world-active-type"||mode=="--world-active-pointer") {
  bool rejected=false;for(const auto& status:nativeItemPolicyStatus())if(status.name=="ExampleSoul"){
   assert(!status.ready&&status.error=="Reason=WorldItemActiveGetterContract");rejected=true;
  }
  assert(rejected&&!hooks.contains(getMethod("WorldItem","void UpdateItem(int i)")));
  auto* getter=getMethod("WorldItem","bool get_active()");methodNames.at(getter).second=true;activeReturnWrong=false;
  const_cast<MethodInfo*>(getter)->pointer=reinterpret_cast<void*>(worldActive);
  for(int i=0;i<30&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 }
 for(const auto& status:nativeItemPolicyStatus())if(!status.ready)std::cerr<<status.name<<" "<<status.error<<"\n";
 assert(pendingNativeItemPolicies()==0&&wrongColorLookups==0);
 auto find=[&](const std::string& name)->const BasicItemEntry& {for(const auto& e:all)if(e.name==name)return e;assert(false);return all[0];};
 const auto& wings=find("ExampleWings");const int wing=slot(wings.type,"Wings");assert(wing==2);
 FakePlayer p;p.wingsLogic=wing;p.velocity[1]=2;const int before=allocations;
 auto movement=hook<void(*)(Il2CppObject*,const MethodInfo*)>("Player","void WingMovement()");
 for(int i=0;i<10000;++i){p.velocity[1]=2;p.wingTime=180;movement(&p.header,nullptr);assert(std::abs(p.velocity[1]-1.015f)<0.00001f&&p.wingTime==179);}
 assert(allocations==before&&movementCalls==0);
 p.gravDir=-1;p.velocity[1]=-2;movement(&p.header,nullptr);assert(std::abs(p.velocity[1]+1.015f)<0.00001f);
 p.wingsLogic=1;movement(&p.header,nullptr);assert(movementCalls==1);
 const auto& robe=find("ExampleRobe");Match request{1,0,slot(robe.type,"Body"),0,true};uint8_t special=0;
 auto match=hook<int32_t(*)(const void*,uint8_t*,const MethodInfo*)>("Player","int SetMatch(Player.SetMatchRequest request, ref bool somethingSpecial)");
 assert(match(&request,&special,nullptr)==slot(robe.type,"Legs")&&special);request.part=2;special=0;assert(match(&request,&special,nullptr)==42&&!special);
 auto* hands=bl::il2cpp::findField(cls("Body/Sets"),android145?"MissingHand":"HidesHands");
 if(androidProperties){for(const auto& [m,f]:propertyFields)if(methodNames.at(m).first=="property get_HidesHands()")hands=f;}
 assert(hands);auto* handTable=*reinterpret_cast<Il2CppArray**>(statics.at(hands).data());
 assert(handTable->length>static_cast<uintptr_t>(request.body));
 assert(static_cast<uint8_t*>(arrayData(handTable))[request.body]==(android145?1:0));
 assert(static_cast<uint8_t*>(arrayData(handTable))[0]==(android145?0:1));
 const auto& beard=find("ExampleBeard");localPlayer.hairColor=0xff123456;FakeItem colored{};assert(applyBasicItemToObject(&colored.header,beard.type)&&colored.color==localPlayer.hairColor);
 for(const char* name:{"ExampleHelmet","ExampleBreastplate","ExampleLeggings"}) {
  const auto& armor=find(name);FakeItem actual{};actual.type=armor.type;
  assert(applyBasicItemToObject(&actual.header,armor.type));
  assert(actual.values[BasicValue]==50000 && actual.values[BasicRare]==2 && actual.values[BasicWidth]==18 && actual.values[BasicHeight]==18);
  assert(actual.defense==(std::string(name)=="ExampleBreastplate"?6:5));
  const int oldDefense=actual.defense;
  std::memset(actual.defenseSentinel,0x6b,sizeof(actual.defenseSentinel));
  assert(setBasicItemDefense(armor.type,0,&error));
  assert(applyBasicItemToObject(&actual.header,armor.type)&&actual.defense==0);
  for(auto value:actual.defenseSentinel)assert(value==0x6b);
  assert(setBasicItemDefense(armor.type,oldDefense,&error));
 }
 const auto& soul=find("ExampleSoul");FakeItem item{};item.type=soul.type;assert(applyBasicItemToObject(&item.header,soul.type));assert(item.clonedReference==&soundObject&&barriers>0);
 auto alpha=hook<uint32_t(*)(Il2CppObject*,uint32_t,const MethodInfo*)>("Item","Color GetAlpha(Color newColor)");assert(alpha(&item.header,0,nullptr)==expectedNativeColor(0x32ffffffu));
 const auto alphaAllocations=allocations;for(int i=0;i<10000;++i)assert(alpha(&item.header,0x01020304u,nullptr)==expectedNativeColor(0x32ffffffu));
 FakeItem vanilla{};vanilla.type=1;assert(alpha(&vanilla.header,0x12345678u,nullptr)==0x12345678u);
 assert(allocations==alphaAllocations);
 World world;world.inner=&item;auto update=hook<void(*)(Il2CppObject*,int32_t,const MethodInfo*)>("WorldItem","void UpdateItem(int i)");update(&world.object,0,nullptr);
 assert(lightCalls==1&&worldUpdates==1&&lastPosition[0]==20&&lastPosition[1]==35&&std::abs(lastLight[0]-0.477f)<0.00001f&&std::abs(lastLight[1]-0.729f)<0.00001f&&std::abs(lastLight[2]-0.828f)<0.00001f);
 const auto worldAllocations=allocations;for(int i=0;i<10000;++i)update(&world.object,0,nullptr);
 assert(lightCalls==10001&&worldUpdates==10001&&allocations==worldAllocations);
 world.active=false;update(&world.object,0,nullptr);assert(lightCalls==10001);
 world.active=true;stopWorldOnUpdate=true;update(&world.object,0,nullptr);assert(!world.active&&lightCalls==10001);stopWorldOnUpdate=false;
 world.active=true;FakeItem ordinary{};ordinary.type=1;world.inner=&ordinary;update(&world.object,0,nullptr);assert(lightCalls==10001);
 world.inner=nullptr;update(&world.object,0,nullptr);assert(lightCalls==10001);
 assert(!worldActiveProperty||activeReads>=10001);
 BasicItemPolicy soulPolicy;assert(item_policy_wire::read(soul,&soulPolicy,&error));
 assert(soulPolicy.clone=="SoulofSight" && soulPolicy.frames==4 && soulPolicy.ticks==6);
 assert(animations.contains(soul.type)&&animationConstructors>0);
 assert(item.values[BasicMaxStack]==9999 && item.values[BasicValue]==10000 && item.values[BasicRare]==5);
 for(const char* name:{"AnimatesAsSoul","ItemIconPulse","ItemNoGravity"}) {
  auto* f=bl::il2cpp::findField(cls("ItemID/Sets"),name);assert(f);
  auto* table=*reinterpret_cast<Il2CppArray**>(statics.at(f).data());
  assert(table->length>static_cast<uintptr_t>(soul.type) && static_cast<uint8_t*>(arrayData(table))[soul.type]==1);
 }
 assert(animationParameters.at(animations.at(soul.type))==std::make_pair(soulPolicy.ticks,soulPolicy.frames));
 const auto constructed=animationConstructors;
 auto animation=hook<void(*)(const MethodInfo*)>("Main","void InitializeItemAnimations()");animations.clear();animation(nullptr);assert(animations.contains(soul.type)&&animationConstructors==constructed);
 auto wingStats=hook<void(*)(const MethodInfo*)>("WingStatsInitializer","void Load()");
 const auto statsField=bl::il2cpp::findField(cls("Wing/Sets"),"Stats");auto* replaced=makeArray(cls("WingStats"),2);constant(statsField,replaced);wingStats(nullptr);
 Il2CppArray* stats=nullptr;apiInstance.field_static_get_value(statsField,&stats);assert(stats->length>static_cast<uintptr_t>(wing));
 int flyTime=0;std::memcpy(&flyTime,static_cast<char*>(arrayData(stats))+24*static_cast<size_t>(wing),4);assert(flyTime==180);
 for(const auto& bad:{std::string("{}"),std::string("{\"v\":1,\"v\":1}"),std::string("{\"v\":1,\"wings\":NaN}")}){auto invalid=soul;invalid.behavior=bad;std::shared_ptr<NativeItemPolicyBatch> rejected;assert(!prepareNativeItemPolicies({invalid},&rejected,&error));}
 const auto& wasp=find("WaspNest");FakePlayer bees;FakeItem waspItem{};waspItem.type=wasp.type;waspItem.flags[5]=true;
 functionalHook(&bees.header,3,&waspItem.header,&functionalMethod);assert(bees.strongBees&&nativeItemPolicyReady(wasp.type));
 bl::testing::modContentRoot=std::string(argv[2])+"/content";
 bl::testing::legacyEquips={{7200,1,"mod:Content/Items/Accessories/ExampleGuardianShieldAccessory_Shield.png"},
                           {7201,3,"mod:Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png"}};
 for(int i=0;i<20&&pendingManagedNativeEquipTextures();++i)pumpNativeItemPoliciesOnGameThread();
 assert(pendingManagedNativeEquipTextures()==0&&bl::testing::legacySlots.at(7200)==2&&bl::testing::legacySlots.at(7201)==2);
 testRenderer(argv[2]);
 const auto loads=assetLoads;for(int i=0;i<250;++i)pumpNativeItemPoliciesOnGameThread();assert(assetLoads==loads);
 // The three rows come from separate Roslyn compilations of the same class.
 assert(argc>=5);std::ifstream revisionsFile(argv[4]);assert(revisionsFile);
 std::string revision;std::getline(revisionsFile,revision);assert(revision=="BUNNY_CLONE_RELOAD_FIXTURE_V1");
 std::vector<BasicItemEntry> revisions;
 while(std::getline(revisionsFile,revision))if(!revision.empty()) {
  BasicItemEntry entry;assert(parseBasicItemWire(basicSplit(revision,'\t'),&entry,&error));
  entry.type=8400;revisions.push_back(entry);
 }
 assert(revisions.size()==3);
 identities[8400]="clone_mod/CloneContract";identities[8401]="other_clone_mod/CloneContract";
 assert(registerBasicItemPlaceholder(8400,"CloneContract",&error));
 assert(registerBasicItemPlaceholder(8401,"CloneContract",&error));
 auto applyRevision=[&](size_t number,int type,const std::string& owner) {
  auto entry=revisions[number];entry.type=type;
  if(type==8402)entry.name="CloneContractSecond";
  assert(applyNativeBasicItemDefaults(owner,{entry},&error));
  if(entry.defenseSpecified)assert(setBasicItemDefense(type,entry.defense,&error));
  for(int i=0;i<5&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
  assert(nativeItemPolicyReady(type));
 };
 applyRevision(0,8400,"clone_mod");applyRevision(0,8401,"other_clone_mod");
 FakeItem inherited{};inherited.type=8400;
 assert(applyBasicItemToObject(&inherited.header,8400));
 assert(inherited.values[0]==28&&inherited.values[1]==20&&inherited.values[2]==99);
 assert(inherited.values[3]==777&&inherited.values[4]==3&&inherited.values[5]==23&&inherited.values[6]==7);
 assert(inherited.defense==2&&inherited.knockBack==3.5f&&inherited.flags[0]&&inherited.flags[2]);
 assert(basicItemIsAccessory(8400)&&!basicItemHasDamage(8400));
 void* inheritedSound=nullptr;std::memcpy(&inheritedSound,inherited.sound,sizeof(inheritedSound));assert(inheritedSound==&soundObject);
 applyRevision(1,8400,"clone_mod");
 std::memset(inherited.defenseSentinel,0x6b,sizeof(inherited.defenseSentinel));
 assert(applyBasicItemToObject(&inherited.header,8400));
 for(size_t i=0;i<13;++i)assert(inherited.values[i]==(i==2?1:0));
 for(bool flag:inherited.flags)assert(!flag);
 assert(inherited.defense==0&&inherited.knockBack==0);
 assert(!basicItemIsAccessory(8400)&&!basicItemHasDamage(8400));
 for(auto sentinel:inherited.defenseSentinel)assert(sentinel==0x6b);
 inheritedSound=&soundObject;std::memcpy(&inheritedSound,inherited.sound,sizeof(inheritedSound));assert(!inheritedSound);
 // An omitted defense assignment after an explicit zero inherits the clone again.
 applyRevision(0,8400,"clone_mod");assert(applyBasicItemToObject(&inherited.header,8400)&&inherited.defense==2);
 // Positive -> omitted follows the same path, independently of a cached zero.
 assert(setBasicItemDefense(8400,9,&error));
 assert(applyBasicItemToObject(&inherited.header,8400)&&inherited.defense==9);
 applyRevision(0,8400,"clone_mod");assert(applyBasicItemToObject(&inherited.header,8400)&&inherited.defense==2);
 // Drop the clone before the next pump: a stale snapshot must not copy it.
 auto plain=revisions[2];plain.type=8400;
 std::thread replace([&]{assert(applyNativeBasicItemDefaults("clone_mod",{plain},&error));});replace.join();
 inherited.clonedReference=nullptr;
 assert(applyBasicItemToObject(&inherited.header,8400)&&inherited.defense==0&&!inherited.clonedReference);
 pumpNativeItemPoliciesOnGameThread(); // GC roots are only released here.
 FakeItem other{};other.type=8401;assert(applyBasicItemToObject(&other.header,8401));
 assert(other.defense==2&&other.values[5]==23&&other.clonedReference==&soundObject);
 // The last replacement wins, even when ingress queues many revisions before a frame.
 std::thread many([&]{for(int i=0;i<100;++i) {
  auto entry=revisions[i%2];entry.type=8400;
  assert(applyNativeBasicItemDefaults("clone_mod",{entry},&error));
 }});many.join();
 for(int i=0;i<5&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(8400)&&pendingNativeItemPolicies()==0);
 // Prefix eligibility follows the native clone, then explicit NativeDefaults.
 auto clonedWeapon=revisions[0];clonedWeapon.type=8400;clonedWeapon.values[BasicMaxStack]=1;
 clonedWeapon.cloneIntegerOverrides|=1<<BasicMaxStack;
 assert(applyNativeBasicItemDefaults("clone_mod",{clonedWeapon},&error));
 pumpNativeItemPoliciesOnGameThread();
 assert(applyBasicItemToObject(&inherited.header,8400)&&basicItemHasDamage(8400));
 NativeDefaultsRequest damageOverride{"CloneContract","clone_mod","CloneContract.cs","damage:i:0",8400,false};
 assert(publishNativeDefaults({damageOverride},&error,nullptr));
 assert(applyBasicItemToObject(&inherited.header,8400)&&!basicItemHasDamage(8400));
 retireNativeDefaultsForMod("clone_mod");
 assert(applyBasicItemToObject(&inherited.header,8400)&&basicItemHasDamage(8400));
 // Same JSON but a newer defaults revision must reject the old installation ack.
 auto nextRevision=revisions[0];nextRevision.type=8400;
 assert(applyNativeBasicItemDefaults("clone_mod",{nextRevision},&error));
 auto waiting=pendingBasicItemBehaviors();
 auto oldAck=std::find_if(waiting.begin(),waiting.end(),[](const auto& entry){return entry.type==8400;});
 assert(oldAck!=waiting.end());const auto oldGeneration=oldAck->defaultsGeneration;
 assert(applyNativeBasicItemDefaults("clone_mod",{nextRevision},&error));
 assert(!acknowledgeBasicItemBehavior(8400,"",oldGeneration));
 // Replace that source while its native clone constructor is executing.
 duringCloneSetup=[&]{std::thread writer([&]{assert(applyNativeBasicItemDefaults("clone_mod",{plain},&error));});writer.join();};
 pumpNativeItemPoliciesOnGameThread();
 FakeItem revoked{};revoked.type=8400;
 assert(applyBasicItemToObject(&revoked.header,8400)&&!revoked.clonedReference&&revoked.defense==0);
 // A concurrent replacement also queues static cleanup for the next pump.
 assert(pendingNativeItemPolicies()>0&&!nativeItemPolicyReady(8400));
 pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(8400)&&pendingNativeItemPolicies()==0);
 assert(applyBasicItemToObject(&other.header,8401)&&other.defense==2);
 // Whole-mod retirement revokes published and queued policies without calling
 // the fake GC on ingress. Another UID and retained types stay usable.
 applyRevision(0,8400,"clone_mod");
 identities[8402]="clone_mod/CloneContractSecond";
 assert(registerBasicItemPlaceholder(8402,"CloneContractSecond",&error));
 applyRevision(0,8402,"clone_mod");
 const auto rootsBeforeRetain=roots.size();
 std::thread retain([&]{retainNativeItemPoliciesForMod("clone_mod",{8400});});retain.join();
 assert(roots.size()==rootsBeforeRetain);
 FakeItem removedPart{};removedPart.type=8402;
 assert(!applyNativeItemPolicyDefaults(&removedPart.header,8402));
 assert(applyNativeItemPolicyDefaults(&inherited.header,8400));
 pumpNativeItemPoliciesOnGameThread();assert(roots.size()<rootsBeforeRetain);
 const auto rootsBeforeRetire=roots.size();
 std::thread retire([&]{retireNativeItemPoliciesForMod("clone_mod");});retire.join();
 assert(roots.size()==rootsBeforeRetire);
 assert(!applyNativeItemPolicyDefaults(&inherited.header,8400));
 assert(applyNativeItemPolicyDefaults(&other.header,8401));
 for(const auto& status:nativeItemPolicyStatus())if(status.mod=="clone_mod")assert(status.phase=="waiting-static-retirement");
 pumpNativeItemPoliciesOnGameThread();assert(roots.size()<rootsBeforeRetire);
 // An admitted but not yet installed clone must never allocate/publish after unload.
 assert(applyNativeBasicItemDefaults("clone_mod",{nextRevision},&error));
 retireNativeItemPoliciesForMod("clone_mod");
 const auto rootsBeforePending=roots.size();pumpNativeItemPoliciesOnGameThread();
 assert(roots.size()==rootsBeforePending&&!applyNativeItemPolicyDefaults(&inherited.header,8400));
 // Unload while the native clone constructor is running rejects its result.
 assert(applyNativeBasicItemDefaults("clone_mod",{nextRevision},&error));
 duringCloneSetup=[&]{std::thread writer([&]{retireNativeItemPoliciesForMod("clone_mod");});writer.join();};
 pumpNativeItemPoliciesOnGameThread();
 assert(!applyNativeItemPolicyDefaults(&inherited.header,8400));
 for(const auto& status:nativeItemPolicyStatus())if(status.mod=="clone_mod")assert(status.phase=="waiting-static-retirement");
 pumpNativeItemPoliciesOnGameThread();
 // Empty full snapshots and repeated reload/unload coalesce before a frame.
 std::thread reloadUnload([&]{for(int i=0;i<100;++i) {
  assert(applyNativeBasicItemDefaults("clone_mod",{nextRevision},&error));
  retainNativeItemPoliciesForMod("clone_mod",{});
 }});reloadUnload.join();pumpNativeItemPoliciesOnGameThread();
 assert(!applyNativeItemPolicyDefaults(&inherited.header,8400));
 applyRevision(0,8400,"clone_mod");
 assert(applyNativeItemPolicyDefaults(&inherited.header,8400));
 assert(applyNativeItemPolicyDefaults(&other.header,8401));
 // Retiring an unknown UID is a no-op, including ready roots and callbacks.
 retireNativeItemPoliciesForMod("missing_mod");pumpNativeItemPoliciesOnGameThread();
 assert(applyNativeItemPolicyDefaults(&inherited.header,8400));
 // Retiring the original compiled ExampleMod batch removes every dynamic
 // callback view, including the derived wing/robe lookup maps.
 std::thread retireOriginal([&]{retireNativeItemPoliciesForMod("mod");});retireOriginal.join();
 p.wingsLogic=wing;p.gravDir=1;p.velocity[1]=2;
 const int retiredMovementCalls=movementCalls;movement(&p.header,nullptr);
 assert(movementCalls==retiredMovementCalls+1);
 request.part=1;special=0;assert(match(&request,&special,nullptr)==42&&!special);
 assert(alpha(&item.header,0x12345678u,nullptr)==0x12345678u);
 world.active=true;world.inner=&item;const int retiredLightCalls=lightCalls;
 update(&world.object,0,nullptr);assert(lightCalls==retiredLightCalls);
 animations.clear();animation(nullptr);assert(!animations.contains(soul.type));
 auto* retiredWingTable=makeArray(cls("WingStats"),2);constant(statsField,retiredWingTable);
 wingStats(nullptr);apiInstance.field_static_get_value(statsField,&stats);
 assert(stats==retiredWingTable&&stats->length==2);
 assert(applyNativeItemPolicyDefaults(&inherited.header,8400));
 assert(applyNativeItemPolicyDefaults(&other.header,8401));
 pumpNativeItemPoliciesOnGameThread();
 // Owned primitive static cells restore the observed baseline, resolving the
 // current table each time; a foreign value is never replaced by an old baseline.
 auto soulFlagField=[&](const char* name){return bl::il2cpp::findField(cls("ItemID/Sets"),name);};
 auto soulFlagTable=[&](const char* name){Il2CppArray* t=nullptr;apiInstance.field_static_get_value(soulFlagField(name),&t);return t;};
 auto soulFlag=[&](const char* name,int contentId)->uint8_t& {return static_cast<uint8_t*>(arrayData(soulFlagTable(name)))[contentId];};
 for(const char* name:{"AnimatesAsSoul","ItemIconPulse","ItemNoGravity"})assert(soulFlag(name,soul.type)==0);
 auto installStaticSoul=[&]{assert(applyNativeBasicItemDefaults("mod",{soul},&error));
  for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
  assert(nativeItemPolicyReady(soul.type));
 };
 identities[8403]="other_soul_mod/OtherSoul";
 assert(registerBasicItemPlaceholder(8403,"OtherSoul",&error,argv[2]));
 auto otherSoul=soul;otherSoul.name="OtherSoul";otherSoul.type=8403;
 assert(applyNativeBasicItemDefaults("other_soul_mod",{otherSoul},&error));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(8403));
 soulFlag("ItemIconPulse",soul.type)=1; // Original is true; a later foreign false must survive.
 installStaticSoul();
 auto* detachedFlagTable=soulFlagTable("AnimatesAsSoul");
 auto* copiedFlagTable=makeArray(&boolClass,detachedFlagTable->length+1);
 std::memcpy(arrayData(copiedFlagTable),arrayData(detachedFlagTable),detachedFlagTable->length);
 constant(soulFlagField("AnimatesAsSoul"),copiedFlagTable);
 soulFlag("ItemIconPulse",soul.type)=0;
 std::thread staticRetire([&]{retireNativeItemPoliciesForMod("mod");});staticRetire.join();
 assert(soulFlag("AnimatesAsSoul",soul.type)==1&&pendingNativeItemPolicies()>0);
 pumpNativeItemPoliciesOnGameThread();
 assert(soulFlag("AnimatesAsSoul",soul.type)==0&&soulFlag("ItemNoGravity",soul.type)==0);
 assert(soulFlag("ItemIconPulse",soul.type)==0&&soulFlag("AnimatesAsSoul",8403)==1);
 assert(static_cast<uint8_t*>(arrayData(detachedFlagTable))[soul.type]==1);
 // Missing or wrong-ABI tables keep cleanup pending and block replacement
 // installation, until the valid table is available again.
 installStaticSoul();auto* savedFlagTable=soulFlagTable("AnimatesAsSoul");
 retireNativeItemPoliciesForMod("mod");constant(soulFlagField("AnimatesAsSoul"),static_cast<Il2CppArray*>(nullptr));
 pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()>0&&!nativeItemPolicyReady(soul.type));
 bool cleanupReported=false;for(const auto& status:nativeItemPolicyStatus())if(status.type==soul.type) {
  cleanupReported=status.phase=="waiting-static-retirement"&&status.mod=="mod"&&!status.ready;
 }assert(cleanupReported);
 auto* wrongFlagTable=makeArray(&intClass,savedFlagTable->length);constant(soulFlagField("AnimatesAsSoul"),wrongFlagTable);
 pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()>0);
 assert(applyNativeBasicItemDefaults("mod",{soul},&error));
 pumpNativeItemPoliciesOnGameThread();assert(!nativeItemPolicyReady(soul.type));
 constant(soulFlagField("AnimatesAsSoul"),savedFlagTable);
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(soul.type)&&soulFlag("AnimatesAsSoul",soul.type)==1);
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(soulFlag("AnimatesAsSoul",soul.type)==0);
 // A newer request delivered while a getter is executing cannot be cleared by
 // the acknowledgement for an earlier cleanup generation.
 installStaticSoul();retireNativeItemPoliciesForMod("mod");
 std::shared_ptr<NativeItemPolicyBatch> lateStaticBatch;
 auto lateSoul=soul;lateSoul.modId="mod";
 assert(prepareNativeItemPolicies({lateSoul},&lateStaticBatch,&error));
 duringStaticRead=[&]{std::thread late([&]{commitNativeItemPolicies(lateStaticBatch);});late.join();};
 pumpNativeItemPoliciesOnGameThread();assert(!nativeItemPolicyReady(soul.type));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(soul.type));
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(soulFlag("AnimatesAsSoul",soul.type)==0);
 // Restore the entire WingStats struct, including bytes not authored by Bunny.
 auto* wingBaselineTable=makeArray(cls("WingStats"),static_cast<size_t>(wing)+1);
 std::array<unsigned char,24> wingBaseline{};wingBaseline.fill(0x3b);
 std::memcpy(static_cast<char*>(arrayData(wingBaselineTable))+24*static_cast<size_t>(wing),wingBaseline.data(),24);
 constant(statsField,wingBaselineTable);
 assert(applyNativeBasicItemDefaults("mod",{wings},&error));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(wings.type));
 apiInstance.field_static_get_value(statsField,&stats);
 auto* wingCell=static_cast<unsigned char*>(arrayData(stats))+24*static_cast<size_t>(wing);
 assert(std::memcmp(wingCell,wingBaseline.data(),24)!=0);
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(std::memcmp(wingCell,wingBaseline.data(),24)==0);
 assert(applyNativeBasicItemDefaults("mod",{wings},&error));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 apiInstance.field_static_get_value(statsField,&stats);
 wingCell=static_cast<unsigned char*>(arrayData(stats))+24*static_cast<size_t>(wing);
 wingCell[23]=0x7d;std::array<unsigned char,24> foreignWing;std::memcpy(foreignWing.data(),wingCell,24);
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(std::memcmp(wingCell,foreignWing.data(),24)==0);
 // Actual native cleanup is limited to eight content types per frame; UID
 // revocation is immediate even while the byte restorations are still queued.
 std::vector<BasicItemEntry> budgetSouls;
 for(int i=0;i<17;++i) {
  auto entry=soul;entry.type=8500+i;entry.name="BudgetSoul"+std::to_string(i);
  identities[entry.type]="budget_static/"+entry.name;
  assert(registerBasicItemPlaceholder(entry.type,entry.name,&error,argv[2]));budgetSouls.push_back(entry);
 }
 assert(applyNativeBasicItemDefaults("budget_static",budgetSouls,&error));
 for(int i=0;i<12&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 for(const auto& entry:budgetSouls)assert(nativeItemPolicyReady(entry.type)&&soulFlag("AnimatesAsSoul",entry.type)==1);
 retireNativeItemPoliciesForMod("budget_static");pumpNativeItemPoliciesOnGameThread();
 int restoredBudget=0;for(const auto& entry:budgetSouls)if(soulFlag("AnimatesAsSoul",entry.type)==0)++restoredBudget;
 assert(restoredBudget==8&&pendingNativeItemPolicies()>0);
 pumpNativeItemPoliciesOnGameThread();restoredBudget=0;
 for(const auto& entry:budgetSouls)if(soulFlag("AnimatesAsSoul",entry.type)==0)++restoredBudget;
 assert(restoredBudget==16);pumpNativeItemPoliciesOnGameThread();
 for(const auto& entry:budgetSouls)assert(soulFlag("AnimatesAsSoul",entry.type)==0);
 assert(pendingNativeItemPolicies()==0&&soulFlag("AnimatesAsSoul",8403)==1);
 // A shortened table discards the retired slot; cleanup must not regrow it.
 installStaticSoul();retireNativeItemPoliciesForMod("mod");
 auto* shortFlags=makeArray(&boolClass,2);constant(soulFlagField("AnimatesAsSoul"),shortFlags);
 pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()==0&&shortFlags->length==2);
 assert(soulFlag("ItemIconPulse",8403)==1&&nativeItemPolicyReady(8403));
 // The actual production ownership code now sees both native registration
 // structures. The fixture implements the Android Contains/Add/assign order.
 auto animationArray=[&]{Il2CppArray* value=nullptr;apiInstance.field_static_get_value(bl::il2cpp::findField(cls("Main"),"itemAnimations"),&value);return value;};
 auto animationList=[&]{Il2CppObject* value=nullptr;apiInstance.field_static_get_value(bl::il2cpp::findField(cls("Main"),"itemAnimationsRegistered"),&value);return reinterpret_cast<AnimationIdList*>(value);};
 auto animationCell=[&](int contentId)->Il2CppObject*& {auto* a=animationArray();assert(a&&a->length>static_cast<uintptr_t>(contentId));return reinterpret_cast<Il2CppObject**>(arrayData(a))[contentId];};
 auto listed=[&](int contentId){const auto& ids=animationList()->ids;return std::count(ids.begin(),ids.end(),contentId);};
 auto eraseForeignIndex=[&](int contentId){auto& ids=animationList()->ids;ids.erase(std::remove(ids.begin(),ids.end(),contentId),ids.end());};
 auto retireSoul=[&]{retireNativeItemPoliciesForMod("mod");for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()==0);};
 assert(animationCell(soul.type)==nullptr&&listed(soul.type)==0);
 const auto otherAnimation=animationCell(8403);assert(otherAnimation&&listed(8403)==1);
 const auto initialAnimationIds=animationList()->ids;
 installStaticSoul();auto* ownedAnimation=animationCell(soul.type);assert(ownedAnimation&&listed(soul.type)==1);
 const auto rootsBeforeAnimationRetire=roots.size();
 std::thread animationRetire([&]{retireNativeItemPoliciesForMod("mod");});animationRetire.join();
 assert(animationCell(soul.type)==ownedAnimation&&roots.size()==rootsBeforeAnimationRetire);
 pumpNativeItemPoliciesOnGameThread();
 assert(!animationCell(soul.type)&&listed(soul.type)==0&&animationList()->ids==initialAnimationIds);
 assert(animationCell(8403)==otherAnimation);
 // Restore both a pre-existing registered object and an unregistered object.
 Il2CppObject foreignOriginal{cls("DrawAnimation"),nullptr},foreignReplacement{cls("DrawAnimation"),nullptr};
 animationCell(soul.type)=&foreignOriginal;animationList()->ids.push_back(soul.type);
 installStaticSoul();retireSoul();assert(animationCell(soul.type)==&foreignOriginal&&listed(soul.type)==1);
 eraseForeignIndex(soul.type);installStaticSoul();retireSoul();
 assert(animationCell(soul.type)==&foreignOriginal&&listed(soul.type)==0);
 animationCell(soul.type)=nullptr;
 // Failure to root a pre-existing object must not publish an index or lose
 // the foreign baseline. The same pending installation can recover afterwards.
 animationCell(soul.type)=&foreignOriginal;failAnimationRootFor=&foreignOriginal;
 assert(applyNativeBasicItemDefaults("mod",{soul},&error));pumpNativeItemPoliciesOnGameThread();
 assert(!nativeItemPolicyReady(soul.type)&&animationCell(soul.type)==&foreignOriginal&&listed(soul.type)==0);
 failAnimationRootFor=nullptr;for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(soul.type));retireSoul();
 assert(animationCell(soul.type)==&foreignOriginal&&listed(soul.type)==0);animationCell(soul.type)=nullptr;
 // A replacement object owns the current index. Preserve that object and list.
 installStaticSoul();animationCell(soul.type)=&foreignReplacement;
 const auto foreignIds=animationList()->ids;retireSoul();
 assert(animationCell(soul.type)==&foreignReplacement&&animationList()->ids==foreignIds);
 eraseForeignIndex(soul.type);animationCell(soul.type)=nullptr;
 // Contains/Add can succeed before assignment throws. Unload and a subsequent
 // successful retry both remove exactly the index introduced by this attempt.
 animationRegisterAfterAddFault=true;assert(applyNativeBasicItemDefaults("mod",{soul},&error));
 pumpNativeItemPoliciesOnGameThread();assert(!nativeItemPolicyReady(soul.type)&&!animationCell(soul.type)&&listed(soul.type)==1);
 animationRegisterAfterAddFault=false;retireSoul();assert(!animationCell(soul.type)&&listed(soul.type)==0);
 animationRegisterAfterAddFault=true;assert(applyNativeBasicItemDefaults("mod",{soul},&error));pumpNativeItemPoliciesOnGameThread();
 animationRegisterAfterAddFault=false;for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(soul.type)&&animationCell(soul.type)&&listed(soul.type)==1);retireSoul();assert(listed(soul.type)==0);
 // Remove first. A failure before removal leaves the object/index intact; a
 // failure after mutation is retried without removing an unrelated element.
 installStaticSoul();ownedAnimation=animationCell(soul.type);animationRemoveBeforeFault=true;
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&animationCell(soul.type)==ownedAnimation&&listed(soul.type)==1);
 pumpNativeItemPoliciesOnGameThread();assert(!animationCell(soul.type)&&listed(soul.type)==0&&pendingNativeItemPolicies()==0);
 installStaticSoul();ownedAnimation=animationCell(soul.type);animationRemoveAfterFault=true;
 const auto removalsBeforeFault=animationRemovals;retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&animationCell(soul.type)==ownedAnimation&&listed(soul.type)==0);
 pumpNativeItemPoliciesOnGameThread();assert(!animationCell(soul.type)&&pendingNativeItemPolicies()==0&&animationRemovals==removalsBeforeFault+1);
 // Collection failures retain the last root and block a new installation.
 installStaticSoul();ownedAnimation=animationCell(soul.type);
 auto* savedAnimations=animationArray();auto* savedAnimationList=animationList();
 auto* animationField=bl::il2cpp::findField(cls("Main"),"itemAnimations");
 auto* animationListField=bl::il2cpp::findField(cls("Main"),"itemAnimationsRegistered");
 retireNativeItemPoliciesForMod("mod");constant(animationField,static_cast<Il2CppArray*>(nullptr));pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&listed(soul.type)==1);
 assert(applyNativeBasicItemDefaults("mod",{soul},&error));
 auto* wrongAnimationArray=makeArray(&intClass,savedAnimations->length);constant(animationField,wrongAnimationArray);
 pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()>0&&listed(soul.type)==1);
 constant(animationField,savedAnimations);constant(animationListField,static_cast<Il2CppObject*>(nullptr));pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&animationCell(soul.type)==ownedAnimation);
 constant(animationListField,&savedAnimationList->header);animationListReturnWrong=true;pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&listed(soul.type)==1);animationListReturnWrong=false;
 animationSnapshotFault=true;pumpNativeItemPoliciesOnGameThread();assert(pendingNativeItemPolicies()>0);animationSnapshotFault=false;
 assert(std::any_of(roots.begin(),roots.end(),[&](const auto& root){return root.second==ownedAnimation;}));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(soul.type)&&animationCell(soul.type)!=ownedAnimation&&listed(soul.type)==1);
 assert(std::none_of(roots.begin(),roots.end(),[&](const auto& root){return root.second==ownedAnimation;}));retireSoul();
 // An ambiguous externally duplicated index must be fixed before clearing its
 // reference; never guess an index or write null under a still-registered ID.
 installStaticSoul();ownedAnimation=animationCell(soul.type);animationList()->ids.push_back(soul.type);
 retireNativeItemPoliciesForMod("mod");pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()>0&&listed(soul.type)==2&&animationCell(soul.type)==ownedAnimation);
 animationList()->ids.pop_back();pumpNativeItemPoliciesOnGameThread();assert(!animationCell(soul.type)&&listed(soul.type)==0&&pendingNativeItemPolicies()==0);
 // Recover the index after another registration shifts its position.
 installStaticSoul();animationCell(1)=&foreignOriginal;animationList()->ids.insert(animationList()->ids.begin(),1);
 auto shiftedIds=animationList()->ids;shiftedIds.erase(std::find(shiftedIds.begin(),shiftedIds.end(),soul.type));
 retireSoul();assert(animationList()->ids==shiftedIds&&animationCell(1)==&foreignOriginal);
 eraseForeignIndex(1);animationCell(1)=nullptr;
 // Array and list copies retain the original ownership. Resolve their current
 // cells and leave the detached collections alone.
 installStaticSoul();auto* detachedAnimations=animationArray();auto* detachedList=animationList();ownedAnimation=animationCell(soul.type);
 auto* copiedAnimations=makeArray(cls("DrawAnimation"),detachedAnimations->length+1);
 std::memcpy(arrayData(copiedAnimations),arrayData(detachedAnimations),detachedAnimations->length*sizeof(void*));
 auto* copiedAnimationList=new AnimationIdList;copiedAnimationList->header.klass=cls("List`1");copiedAnimationList->ids=detachedList->ids;
 constant(animationField,copiedAnimations);constant(animationListField,&copiedAnimationList->header);retireSoul();
 assert(!animationCell(soul.type)&&listed(soul.type)==0&&animationCell(8403)==otherAnimation);
 assert(reinterpret_cast<Il2CppObject**>(arrayData(detachedAnimations))[soul.type]==ownedAnimation);
 assert(std::count(detachedList->ids.begin(),detachedList->ids.end(),soul.type)==1);
 // A real initializer replacing both collections creates a new baseline;
 // reapply existing animations without constructing another DrawAnimation.
 animationCell(soul.type)=&foreignOriginal;installStaticSoul();const auto constructorsBeforeReset=animationConstructors;
 originalVoidAction=[&]{auto* resetArray=makeArray(cls("DrawAnimation"),animationArray()->length);
  auto* resetList=new AnimationIdList;resetList->header.klass=cls("List`1");
  constant(animationField,resetArray);constant(animationListField,&resetList->header);
 };
 animation(nullptr);assert(animationConstructors==constructorsBeforeReset&&listed(soul.type)==1&&listed(8403)==1);
 retireSoul();assert(!animationCell(soul.type)&&listed(soul.type)==0&&animationCell(8403)==otherAnimation);
 // A shortened array may discard our slot. Remove our added index without
 // resurrecting that slot, retaining all lower registered entries.
 auto shortSoul=soul;shortSoul.type=9000;shortSoul.name="ShortAnimation";
 identities[9000]="short_animation/ShortAnimation";assert(registerBasicItemPlaceholder(9000,shortSoul.name,&error,argv[2]));
 assert(applyNativeBasicItemDefaults("short_animation",{shortSoul},&error));
 for(int i=0;i<8&&pendingNativeItemPolicies();++i)pumpNativeItemPoliciesOnGameThread();
 assert(nativeItemPolicyReady(9000));
 auto* shortAnimations=makeArray(cls("DrawAnimation"),9000);std::memcpy(arrayData(shortAnimations),arrayData(animationArray()),9000*sizeof(void*));
 constant(animationField,shortAnimations);retireNativeItemPoliciesForMod("short_animation");pumpNativeItemPoliciesOnGameThread();
 assert(pendingNativeItemPolicies()==0&&shortAnimations->length==9000&&listed(9000)==0&&animationCell(8403)==otherAnimation);
 std::cout<<"NATIVE_ITEM_ANIMATION_REFERENCE_AND_LIST_RETIREMENT_PARTIAL_FAILURE_FOREIGN_BASELINE_AND_RESET_PASS\n";
 std::cout<<"NATIVE_ITEM_STATIC_FLAG_AND_WING_CELL_RETIREMENT_RETRY_FOREIGN_VALUE_AND_GENERATION_PASS\n";
 std::cout<<"NATIVE_ITEM_POLICY_UID_RETIRE_FULL_SNAPSHOT_PENDING_INFLIGHT_AND_OTHER_OWNER_PASS\n";
 std::cout<<"NATIVE_CLONE_EXPLICIT_ZERO_FALSE_EMPTY_SOUND_INHERITED_DEFENSE_REVOCATION_AND_OTHER_OWNER_PASS\n";
 std::cout<<"NATIVE_ITEM_POLICIES_FULL_EXAMPLE_WIRE_NO_QUICKJS_PASS items="<<all.size()<<" movement=10000 boxed="<<!rawOffsets<<" mode="<<mode<<"\n";
}
