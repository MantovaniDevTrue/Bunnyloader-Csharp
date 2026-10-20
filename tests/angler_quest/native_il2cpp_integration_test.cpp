#include "experiments/csharp/NativeAnglerQuestGameplay.h"
#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include "content/common/TypeTables.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Il2CppClass {
    const char* ns;
    const char* name;
    Il2CppClass* element = nullptr;
};
struct Il2CppType {
    const char* name;
    Il2CppClass* type = nullptr;
};
struct MethodInfo {
    const char* name;
    const char* returnType;
    const char* parameterType;
    bool instance;
};
struct FieldInfo {
    const char* name;
    const Il2CppType* type;
    void* value;
    size_t width;
};

namespace {
Il2CppClass kInt{"System","Int32"}, kBool{"System","Boolean"};
Il2CppClass kIntArray{"System","Int32[]",&kInt}, kBoolArray{"System","Boolean[]",&kBool};
Il2CppClass kMain{"Terraria","Main"}, kLang{"Terraria","Lang"};
Il2CppClass kSets{"Terraria.ID","Sets"}, kLanguage{"Terraria.Localization","Language"};
Il2CppClass kCultureClass{"Terraria.Localization","GameCulture"};
Il2CppType kIntType{"System.Int32",&kInt}, kBoolType{"System.Boolean",&kBool};
Il2CppType kIntArrayType{"System.Int32[]",&kIntArray};
Il2CppType kBoolArrayType{"System.Boolean[]",&kBoolArray};
MethodInfo kSwap{"AnglerQuestSwap","System.Void",nullptr,false};
MethodInfo kChat{"AnglerQuestChat","System.String","System.Boolean",false};
MethodInfo kName{"GetItemNameValue","System.String","System.Int32",false};
MethodInfo kCulture{"get_ActiveCulture","Terraria.Localization.GameCulture",nullptr,false};
MethodInfo kCultureName{"get_Name","System.String",nullptr,true};
Il2CppArray* gIds=nullptr;
Il2CppArray* gFlags=nullptr;
Il2CppArray* gRack=nullptr;
int32_t gSelected=0;
int32_t gCorner=0;
int32_t gNetMode=0;
uint8_t gFinished=0, gHard=0;
bool gBadStaticField=false;
bool gBadNetMode=false;
bool gFailRackResize=false;
bool gFailIdsAllocation=false;
int gHandleCalls=0;
int gFailHandleAt=-1;
uint32_t gNextHandle=1;
std::map<uint32_t,Il2CppObject*> gGcRoots;
int gSwapCalls=0;
std::string gCulture="en-US";
FieldInfo fIds{"anglerQuestItemNetIDs",&kIntArrayType,&gIds,sizeof(gIds)};
FieldInfo fFlags{"IsQuestFish",&kBoolArrayType,&gFlags,sizeof(gFlags)};
FieldInfo fRack{"CanBePlacedOnWeaponRacks",&kBoolArrayType,&gRack,sizeof(gRack)};
FieldInfo fIndex{"anglerQuest",&kIntType,&gSelected,sizeof(gSelected)};
FieldInfo fCorner{"npcChatCornerItem",&kIntType,&gCorner,sizeof(gCorner)};
FieldInfo fFinished{"anglerQuestFinished",&kBoolType,&gFinished,sizeof(gFinished)};
FieldInfo fHard{"hardMode",&kBoolType,&gHard,sizeof(gHard)};
FieldInfo fNetMode{"netMode",&kIntType,&gNetMode,sizeof(gNetMode)};
Il2CppObject kCultureObject{&kCultureClass,nullptr};
using Swap = void (*)(const MethodInfo*);
using Chat = Il2CppString* (*)(uint8_t,const MethodInfo*);
Swap hookedSwap=nullptr;
Chat hookedChat=nullptr;

Il2CppString* str(const std::string& value) {
    size_t bytes=sizeof(Il2CppString)+value.size()*sizeof(char16_t);
    void* mem=std::calloc(1,bytes);
    auto* s=static_cast<Il2CppString*>(mem);
    s->length=static_cast<int32_t>(value.size());
    for(size_t i=0;i<value.size();++i)s->chars[i]=static_cast<char16_t>(value[i]);
    return s;
}
std::string stringAscii(const Il2CppString* value) {
    std::string out;
    for(int i=0;i<value->length;++i)out.push_back(static_cast<char>(value->chars[i]));
    return out;
}
void vanillaSwap(const MethodInfo*) {
    ++gSwapCalls;
    gSelected=static_cast<int32_t>((gSelected+1)%gIds->length);
}
Il2CppString* vanillaChat(uint8_t,const MethodInfo*) { return str("VANILLA"); }
const MethodInfo* findMethod(Il2CppClass* cls,const char* name,int count) {
    if(cls==&kMain && std::strcmp(name,"AnglerQuestSwap")==0 && count==0)return &kSwap;
    if(cls==&kLang && std::strcmp(name,"AnglerQuestChat")==0 && count==1)return &kChat;
    if(cls==&kLang && std::strcmp(name,"GetItemNameValue")==0 && count==1)return &kName;
    if(cls==&kLanguage && std::strcmp(name,"get_ActiveCulture")==0 && count==0)return &kCulture;
    if(cls==&kCultureClass && std::strcmp(name,"get_Name")==0 && count==0)return &kCultureName;
    return nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if (cls == &kMain && name == "netMode" && gBadNetMode) return nullptr;
    FieldInfo* fields[]={&fIds,&fFlags,&fRack,&fIndex,&fCorner,&fFinished,&fHard,&fNetMode};
    for(auto* f:fields)if(name==f->name &&
        ((cls==&kMain && f!=&fFlags)||(cls==&kSets && (f==&fFlags || f==&fRack))))
            return f;
    return nullptr;
}
Il2CppArray* array(Il2CppClass* element,uintptr_t size) {
    if (gFailIdsAllocation && element == &kInt) return nullptr;
    const size_t width=element==&kBool?1u:sizeof(int32_t);
    auto* a=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+width*size));
    a->obj.klass=element==&kBool?&kBoolArray:&kIntArray;
    a->length=size;
    return a;
}
void setupApi() {
    auto& a=bl::il2cpp::api();
    a.class_get_method_from_name=findMethod;
    a.class_get_parent=[](Il2CppClass*)->Il2CppClass*{return nullptr;};
    a.field_get_type=[](FieldInfo* field){return field->type;};
    a.field_get_flags=[](FieldInfo* field)->uint32_t{return gBadStaticField && field==&fCorner ? 0u : 0x10u;};
    a.class_from_il2cpp_type=[](const Il2CppType* t){return t->type;};
    a.class_get_element_class=[](Il2CppClass* cls){return cls->element;};
    a.class_get_rank=[](const Il2CppClass* cls) {return cls==&kBoolArray||cls==&kIntArray?1:0;};
    a.class_is_valuetype=[](const Il2CppClass* cls) {return cls==&kBool||cls==&kInt;};
    a.class_value_size=[](Il2CppClass* cls,uint32_t* align)->int32_t {
        if(align)*align=cls==&kBool?1u:4u;
        return cls==&kBool?1:cls==&kInt?4:0;
    };
    a.class_get_name=[](Il2CppClass* c){return c->name;};
    a.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    a.class_get_method_from_name=findMethod;
    a.field_static_get_value=[](FieldInfo* f,void* destination){std::memcpy(destination,f->value,f->width);};
    a.field_static_set_value=[](FieldInfo* f,void* source){
        if(f==&fIds||f==&fFlags||f==&fRack) {
            Il2CppArray* next=nullptr;
            std::memcpy(&next,source,sizeof(next));
            Il2CppArray* current=nullptr;
            std::memcpy(&current,f->value,sizeof(current));
            if(next!=current)
                assert(std::any_of(gGcRoots.begin(),gGcRoots.end(),
                    [&](const auto& root){return root.second==reinterpret_cast<Il2CppObject*>(next);}));
        }
        std::memcpy(f->value,source,f->width);
    };
    a.method_get_param_count=[](const MethodInfo* m)->uint32_t{return m->parameterType?1:0;};
    a.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{
        static Il2CppType ret{};
        ret.name=m->returnType;
        return &ret;
    };
    a.method_get_param=[](const MethodInfo* m,uint32_t)->const Il2CppType*{
        static Il2CppType argument{};
        argument.name=m->parameterType;
        return &argument;
    };
    a.type_get_name=[](const Il2CppType* type){return ::strdup(type->name);};
    a.il2cpp_free=std::free;
    a.method_is_instance=[](const MethodInfo* m){return m->instance;};
    a.object_get_class=[](Il2CppObject* object){return object->klass;};
    a.array_new=array;
    a.gchandle_new=[](Il2CppObject* object,bool pinned)->uint32_t {
        assert(pinned);
        ++gHandleCalls;
        if(gHandleCalls==gFailHandleAt)return 0;
        const uint32_t handle=gNextHandle++;
        gGcRoots[handle]=object;
        return handle;
    };
    a.gchandle_free=[](uint32_t handle){assert(gGcRoots.erase(handle)==1);};
    a.runtime_class_init=[](Il2CppClass*){};
    a.string_new=[](const char* value){return str(value);};
    a.runtime_invoke=[](const MethodInfo* method,void*,void**,Il2CppObject** error) -> Il2CppObject*{
        *error=nullptr;
        if(method==&kCulture)return &kCultureObject;
        if(method==&kCultureName)return reinterpret_cast<Il2CppObject*>(str(gCulture));
        if(method==&kName)return reinterpret_cast<Il2CppObject*>(str("FISH"));
        return nullptr;
    };
}
std::vector<int> ids() {
    const auto* p=static_cast<const int32_t*>(arrayData(gIds));
    return std::vector<int>(p,p+gIds->length);
}
}

namespace bl::il2cpp {
Api& api(){static Api api;return api;}
Il2CppClass* findClass(const TypeRef& ref) {
    if(ref.ns=="Terraria"&&ref.name=="Main")return &kMain;
    if(ref.ns=="Terraria"&&ref.name=="Lang")return &kLang;
    if(ref.ns=="Terraria.ID"&&ref.name=="ItemID"&&ref.nested=="Sets")return &kSets;
    if(ref.ns=="Terraria.Localization"&&ref.name=="Language")return &kLanguage;
    if(ref.ns=="Terraria.Localization"&&ref.name=="GameCulture")return &kCultureClass;
    return nullptr;
}
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name) {
    return findClass({ns,name,{}});
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return ::findField(cls,name);}
}
namespace bl::csharp {
bool gManagedAllowed = true;
bool gManagedReady = true;
bool gUnloadDuringCallback = false;
int gManagedCalls = 0;
int gLastPlayerIndex = -999;
bool invokeManagedAnglerQuestCallback(const std::string&,const std::string&,
                                      int,int,int,int,bool* allowed,int playerIndex) {
    ++gManagedCalls;
    gLastPlayerIndex = playerIndex;
    if (!gManagedReady) return false;
    if (gUnloadDuringCallback) bl::csharp::angler::registry().unload("examplemodcsharp");
    *allowed = gManagedAllowed;
    return true;
}
}
namespace bl::csharp::angler {
bool installFishingOnGameThread(const std::vector<Quest>*) { return true; }
const char* fishingInstallDiagnostic() { return "HostFixture"; }
}
namespace bl::runtime {
Il2CppArray* TypeTables::resizedCopy(Il2CppArray* old,uintptr_t len) {
    if (gFailRackResize && old == gRack) return nullptr;
    auto* next=array(old->obj.klass==&kBoolArray?&kBool:&kInt,len);
    const size_t width=old->obj.klass==&kBoolArray?1u:sizeof(int32_t);
    std::memcpy(arrayData(next),arrayData(old),std::min(old->length,len)*width);
    return next;
}
}
namespace bl::hook {
bool install(const MethodInfo* method,void* replacement,void** original) {
    if(method==&kSwap) {
        hookedSwap=reinterpret_cast<Swap>(replacement);
        *original=reinterpret_cast<void*>(vanillaSwap);
        return true;
    }
    if(method==&kChat) {
        hookedChat=reinterpret_cast<Chat>(replacement);
        *original=reinterpret_cast<void*>(vanillaChat);
        return true;
    }
    return false;
}
}
namespace bl::log {
void write(int,const char*,...){}
}

int main() {
    setupApi();
    gIds=array(&kInt,3);
    auto* data=static_cast<int32_t*>(arrayData(gIds));
    data[0]=10;data[1]=500;data[2]=11;
    gFlags=array(&kBool,128);
    gRack=array(&kBool,128);
    std::string error;
    using bl::csharp::angler::Quest;
    auto& reg=bl::csharp::angler::registry();
    Quest quest{"examplemodcsharp","ExampleQuestFish",850,false,true,
        "The upside-down fish","Anywhere",3};
    quest.locales.emplace("pt-BR",std::make_pair("Peixe invertido","Qualquer lugar"));
    assert(reg.publish("examplemodcsharp",{quest},&error));
    gBadStaticField=true;
    bl::csharp::angler::pumpOnGameThread();
    assert(!bl::csharp::angler::hooksInstalled());
    assert((ids()==std::vector<int>{10,500,11}));
    gBadStaticField=false;
    gBadNetMode=true;
    bl::csharp::angler::pumpOnGameThread();
    assert(!bl::csharp::angler::hooksInstalled());
    gBadNetMode=false;
    bl::csharp::angler::pumpOnGameThread();
    assert(bl::csharp::angler::hooksInstalled());
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(static_cast<const uint8_t*>(arrayData(gFlags))[850]==1);
    assert(static_cast<const uint8_t*>(arrayData(gRack))[850]==1);
    assert(hookedSwap && hookedChat);
    assert(gGcRoots.empty());

    Quest extra{"examplemodcsharp","ExtraQuestFish",940,true,true,"Extra","Elsewhere",0};
    assert(reg.publish("examplemodcsharp",{quest,extra},&error));
    Il2CppArray* correctIds=gIds;
    gIds=array(&kBool,4);
    bl::csharp::angler::pumpOnGameThread();
    assert(gFlags->length==851&&gRack->length==851);
    assert(stringAscii(hookedChat(0,&kChat))=="VANILLA");
    gIds=correctIds;
    Il2CppArray* correctRack=gRack;
    gRack=array(&kInt,851);
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851);
    gRack=correctRack;
    auto& exports=bl::il2cpp::api();
    const auto originalHandleNew=exports.gchandle_new;
    exports.gchandle_new=nullptr;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851 && gRack->length==851 && gGcRoots.empty());
    exports.gchandle_new=originalHandleNew;
    gFailIdsAllocation=true;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851 && gRack->length==851);
    gFailIdsAllocation=false;
    gFailRackResize=true;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851 && gRack->length==851);
    assert(gSelected==0);
    assert(gGcRoots.empty());
    gFailRackResize=false;
    gFailHandleAt=gHandleCalls+2;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851 && gRack->length==851 && gGcRoots.empty());
    gFailHandleAt=gHandleCalls+3;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(gFlags->length==851 && gRack->length==851 && gGcRoots.empty());
    gFailHandleAt=-1;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850,940}));
    assert(gFlags->length==941 && gRack->length==941);
    assert(static_cast<const uint8_t*>(arrayData(gFlags))[940]==1);
    assert(static_cast<const uint8_t*>(arrayData(gRack))[940]==1);
    assert(gGcRoots.empty());
    assert(reg.publish("examplemodcsharp",{quest},&error));
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11,850}));
    assert(static_cast<const uint8_t*>(arrayData(gFlags))[940]==0);
    assert(static_cast<const uint8_t*>(arrayData(gRack))[940]==0);

    Quest dynamic=quest;
    dynamic.callbackMask=1;
    assert(reg.publish("examplemodcsharp",{dynamic},&error));
    bl::csharp::angler::pumpOnGameThread();
    gHard=1;
    gNetMode=0;
    bl::csharp::gManagedAllowed=false;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==2 && gSelected==0);
    bl::csharp::gManagedAllowed=true;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==1 && gSelected==3);
    bl::csharp::gManagedReady=false;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==2 && gSelected==0);
    bl::csharp::gManagedReady=true;
    assert(bl::csharp::gManagedCalls>=3);
    assert(bl::csharp::gLastPlayerIndex==-1);
    bl::csharp::gUnloadDuringCallback=true;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==1 && gSelected==3);
    assert(stringAscii(hookedChat(0,&kChat))=="VANILLA");
    bl::csharp::gUnloadDuringCallback=false;
    assert(reg.publish("examplemodcsharp",{dynamic},&error));
    bl::csharp::angler::pumpOnGameThread();
    std::vector<Quest> many;
    for (int i=0;i<6;++i) {
        Quest candidate=quest;
        candidate.type=850+i;
        candidate.name="Dynamic"+std::to_string(i);
        candidate.preHardmode=true;
        candidate.callbackMask=1;
        many.push_back(std::move(candidate));
    }
    assert(reg.publish("examplemodcsharp",many,&error));
    bl::csharp::angler::pumpOnGameThread();
    const int calledBefore=bl::csharp::gManagedCalls;
    gHard=1;
    gNetMode=0;
    gSelected=2;
    gSwapCalls=0;
    bl::csharp::gManagedAllowed=false;
    hookedSwap(&kSwap);
    assert(bl::csharp::gManagedCalls-calledBefore==4);
    assert(gSelected==0);
    bl::csharp::gManagedAllowed=true;
    assert(reg.publish("examplemodcsharp",{quest},&error));
    bl::csharp::angler::pumpOnGameThread();
    bl::csharp::angler::invalidateWorldSession();
    gHard=0;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==1 && gSelected==3);
    bl::csharp::angler::pumpOnGameThread();
    gHard=0;
    for (const int mode : {0, 1, 2, 3, -1, 4}) {
        gNetMode=mode;
        gSelected=2;
        gSwapCalls=0;
        hookedSwap(&kSwap);
        if (mode == 1 || mode < 0 || mode > 3) {
            assert(gSwapCalls==1);
            assert(gSelected==3);
        } else {
            assert(gSwapCalls==2);
            assert(gSelected==0);
        }
    }
    gNetMode=0;
    gSelected=3;
    gHard=1;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==1);
    gSelected=3;
    gCulture="pt-BR";
    assert(stringAscii(hookedChat(0,&kChat))=="Peixe invertido\n\n(Qualquer lugar)");
    assert(gCorner==850);
    assert(stringAscii(hookedChat(1,&kChat))=="VANILLA");
    gCulture="en-US";
    assert(stringAscii(hookedChat(0,&kChat))=="The upside-down fish\n\n(Anywhere)");

    bl::csharp::angler::invalidateWorldSession();
    gIds=array(&kInt,4);
    auto* newWorldIds=static_cast<int32_t*>(arrayData(gIds));
    newWorldIds[0]=10;
    newWorldIds[1]=500;
    newWorldIds[2]=850;
    newWorldIds[3]=11;
    gSelected=2;
    gFlags=array(&kBool,1000);
    gRack=array(&kBool,1000);
    static_cast<uint8_t*>(arrayData(gFlags))[850]=1;
    static_cast<uint8_t*>(arrayData(gRack))[850]=1;
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,850,11}));
    assert(gSelected==2);
    reg.unload("examplemodcsharp");
    gHard=0;
    gSelected=2;
    gSwapCalls=0;
    hookedSwap(&kSwap);
    assert(gSwapCalls==1 && gSelected==3);
    assert(stringAscii(hookedChat(0,&kChat))=="VANILLA");
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11}));
    assert(static_cast<const uint8_t*>(arrayData(gFlags))[850]==1);
    assert(static_cast<const uint8_t*>(arrayData(gRack))[850]==1);
    assert(gSelected==2);
    assert(ids()[gSelected]==11);
    assert(stringAscii(hookedChat(0,&kChat))=="VANILLA");
    bl::csharp::angler::invalidateWorldSession();
    bl::csharp::angler::pumpOnGameThread();
    assert((ids()==std::vector<int>{10,500,11}));
    assert(reg.publish("examplemodcsharp",{quest},&error));
    bl::csharp::angler::pumpOnGameThread();
    const auto preservedIds=ids();
    auto* preservedArray=gIds;
    gSelected=999;
    bl::csharp::angler::invalidateWorldSession();
    bl::csharp::angler::pumpOnGameThread();
    assert(gSelected==0 && gIds==preservedArray && ids()==preservedIds);
    gSelected=-17;
    bl::csharp::angler::invalidateWorldSession();
    bl::csharp::angler::pumpOnGameThread();
    assert(gSelected==0 && gIds==preservedArray && ids()==preservedIds);
    gSelected=2;
    bl::csharp::angler::invalidateWorldSession();
    bl::csharp::angler::pumpOnGameThread();
    assert(gSelected==2 && gIds==preservedArray && ids()==preservedIds);
    static_cast<uint8_t*>(arrayData(gFlags))[850]=0;
    static_cast<uint8_t*>(arrayData(gRack))[850]=0;
    reg.unload("examplemodcsharp");
    bl::csharp::angler::pumpOnGameThread();
    assert(static_cast<const uint8_t*>(arrayData(gFlags))[850]==0);
    assert(static_cast<const uint8_t*>(arrayData(gRack))[850]==0);
    std::cout<<"ANGLER_QUEST_IL2CPP_HOST_INTEGRATION_OK\n";
}
