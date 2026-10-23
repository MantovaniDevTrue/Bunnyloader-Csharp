#define main existingNativeItemFixtureMain
#include "native_item_binding_test.cpp"
#undef main
#include "experiments/csharp/NativeItemPolicies.h"
#include "native_policy_support.h"
#include "experiments/csharp/NativeVisualRenderer.h"
#include "experiments/csharp/NativeStaticValue.h"
#include <algorithm>
#include <filesystem>
#include <memory>

namespace {
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
std::map<int,Il2CppObject*> animations;
FakePlayer localPlayer;
unsigned nextRoot=1;
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
void originalVoid(const MethodInfo*){}
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
  if(name=="bool get_active()")return 0u;
  return name=="Color GetAlpha(Color newColor)"?1u:propertySetter.at(m)?1u:0u;
 };
 apiInstance.method_get_param=[](const MethodInfo* m,uint32_t)->const Il2CppType* {
  return methodNames.at(m).first=="Color GetAlpha(Color newColor)"?type(alphaParameter):propertyFields.at(m)->type;
 };
 apiInstance.method_get_return_type=[](const MethodInfo* m)->const Il2CppType* {
  if(methodNames.at(m).first=="bool get_active()")return activeReturnWrong?&intType:&boolType;
  if(methodNames.at(m).first=="Color GetAlpha(Color newColor)")return type(alphaReturn);
  return propertySetter.at(m)?type(cls("Void",true,0)):propertyFields.at(m)->type;
 };
 apiInstance.class_get_namespace=[](Il2CppClass* c){return c==cls("Color")?colorNamespace.c_str():"System";};
 apiInstance.field_get_name=[](FieldInfo* f){return f->name;};apiInstance.type_get_name=[](const Il2CppType* t){return ::strdup(t->cls->name);};apiInstance.il2cpp_free=std::free;
 apiInstance.object_new=[](Il2CppClass* c){auto* o=static_cast<Il2CppObject*>(std::calloc(1,static_cast<size_t>(c->size)));o->klass=c;++allocations;return o;};
 apiInstance.runtime_class_init=[](Il2CppClass*){};
 apiInstance.method_is_instance=[](const MethodInfo* m){return methodNames.at(m).second;};
 apiInstance.gchandle_new=[](Il2CppObject* o,bool){roots[nextRoot]=o;return nextRoot++;};apiInstance.gchandle_free=[](unsigned h){roots.erase(h);};apiInstance.gchandle_get_target=[](unsigned h){return roots.at(h);};
 apiInstance.field_static_get_value=[](FieldInfo* f,void* out){if(f->name==std::string("Item1")){void* value=&soundObject;std::memcpy(out,&value,sizeof(value));return;}const auto& v=statics.at(f);std::memcpy(out,v.data(),v.size());};
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
  if(n.find("void Copy(")==0){auto* from=static_cast<Il2CppArray*>(args[0]);auto* to=static_cast<Il2CppArray*>(args[1]);const auto count=*static_cast<int*>(args[2]);std::memcpy(arrayData(to),arrayData(from),static_cast<size_t>(count)*width(from->obj.klass->element));++arrayWrites;}
  else if(n.find("void SetValue(")==0){if(failSet){failSet=false;*error=&exception;return nullptr;}auto* a=static_cast<Il2CppArray*>(self);const int index=*static_cast<int*>(args[1]);assert(index>=0&&static_cast<uintptr_t>(index)<a->length);std::memcpy(static_cast<char*>(arrayData(a))+sizeof(void*)*static_cast<size_t>(index),&args[0],sizeof(void*));++arrayWrites;}
  else if(n.find("void SetDefaults(")==0){assert(*static_cast<int*>(args[0])==547);auto* item=static_cast<FakeItem*>(self);item->values[0]=28;item->values[1]=20;item->clonedReference=&soundObject;void* sound=&soundObject;std::memcpy(item->sound,&sound,sizeof(void*));}
  else if(n.find("get_LocalPlayer")!=std::string::npos)return &localPlayer.header;
  else if(n.find("void .ctor(int ticksperframe") == 0) {
   ++animationConstructors;assert(self&&*static_cast<int*>(args[0])>0&&*static_cast<int*>(args[1])>1);
   if(android145)assert(*static_cast<uint8_t*>(args[2])==0);
   if(animationCtorFault){*error=&exception;return nullptr;}
   animationParameters[static_cast<Il2CppObject*>(self)]={*static_cast<int*>(args[0]),*static_cast<int*>(args[1])};
  }
  else if(n.find("RegisterItemAnimation")!=std::string::npos){
   auto* object=static_cast<Il2CppObject*>(args[1]);assert(animationParameters.contains(object));animations[*static_cast<int*>(args[0])]=object;
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
  if(entry.defense>0)assert(setBasicItemDefense(entry.type,entry.defense,&error));
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
 std::cout<<"NATIVE_ITEM_POLICIES_FULL_EXAMPLE_WIRE_NO_QUICKJS_PASS items="<<all.size()<<" movement=10000 boxed="<<!rawOffsets<<" mode="<<mode<<"\n";
}
