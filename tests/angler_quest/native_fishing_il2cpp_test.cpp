#include "experiments/csharp/NativeAnglerQuestFishing.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

struct Il2CppClass {
    const char* ns;
    const char* name;
    Il2CppClass* element = nullptr;
};
struct Il2CppType {
    const char* name;
    Il2CppClass* cls;
};
struct MethodInfo {
    const char* name;
};
struct FieldInfo {
    const char* name;
    const Il2CppType* type;
    size_t offset;
    bool isStatic = false;
};

namespace {
Il2CppClass kProjectile{"Terraria","Projectile"};
Il2CppClass kPlayer{"Terraria","Player"};
Il2CppClass kMain{"Terraria","Main"};
Il2CppClass kFishing{"Terraria.DataStructures","FishingAttempt"};
Il2CppClass kInt{"System","Int32"};
Il2CppClass kBool{"System","Boolean"};
Il2CppClass kFloat{"System","Single"};
Il2CppClass kPlayersArray{"Terraria","Player[]",&kPlayer};
Il2CppClass kFakePlayersArray{"System","Int32[]",&kInt};
Il2CppClass kWrongPlayer{"Terraria","NPC"};
Il2CppType tInt{"System.Int32",&kInt};
Il2CppType tBool{"System.Boolean",&kBool};
Il2CppType tFloat{"System.Single",&kFloat};
Il2CppType tPlayerArray{"Terraria.Player[]",&kPlayersArray};
Il2CppType tFishingRef{"Terraria.DataStructures.FishingAttempt&",&kFishing};
Il2CppType tVoid{"System.Void",nullptr};
MethodInfo kRoll{"FishingCheck_RollItemDrop"};
FieldInfo fQuest{"questFish",&tInt,sizeof(Il2CppObject)};
FieldInfo fUncommon{"uncommon",&tBool,sizeof(Il2CppObject)+4};
FieldInfo fRolled{"rolledItemDrop",&tInt,sizeof(Il2CppObject)+8};
FieldInfo fLast{"testTail",&tInt,sizeof(Il2CppObject)+88};
FieldInfo fOwner{"owner",&tInt,sizeof(Il2CppObject)};
FieldInfo fGravity{"gravDir",&tFloat,sizeof(Il2CppObject)};
FieldInfo fPlayers{"player",&tPlayerArray,0,true};
FieldInfo fNetMode{"netMode",&tInt,0,true};
int32_t netMode=0;
struct ProjectileObject { Il2CppObject object; int32_t owner = 0; };
struct PlayerObject { Il2CppObject object; float gravDir = 1.f; };
ProjectileObject projectile{};
PlayerObject player{};
Il2CppArray* players=nullptr;
using Roll = void (*)(Il2CppObject*,void*,const MethodInfo*);
Roll hookedRoll=nullptr;
int vanillaCalls=0;
bool badPlayersStatic=false;
bool badPlayersRank=false;
bool badNetModeStatic=false;
bool badNetModeType=false;
bool badFishingSize=false;
bool badFishingParameter=false;
bool badQuestFieldType=false;
bool badFieldOverlap=false;

template<typename T>
void put(std::array<unsigned char,92>* data,size_t offset,T value) {
    std::memcpy(data->data()+offset,&value,sizeof(T));
}
int32_t get(const std::array<unsigned char,92>& data,size_t offset) {
    int32_t value=0;
    std::memcpy(&value,data.data()+offset,sizeof(value));
    return value;
}
void vanillaRoll(Il2CppObject*,void* attempt,const MethodInfo*) {
    ++vanillaCalls;
    const int32_t fish=13;
    std::memcpy(static_cast<char*>(attempt)+8,&fish,sizeof(fish));
}
void configure() {
    auto& api=bl::il2cpp::api();
    api.class_get_method_from_name=[](Il2CppClass* cls,const char* name,int count)
        ->const MethodInfo* {
        return cls==&kProjectile&&std::strcmp(name,"FishingCheck_RollItemDrop")==0
            &&count==1?&kRoll:nullptr;
    };
    api.method_get_param_count=[](const MethodInfo*)->uint32_t{return 1;};
    api.method_get_param=[](const MethodInfo*,uint32_t)->const Il2CppType*{
        return badFishingParameter ? &tInt : &tFishingRef;
    };
    api.method_get_return_type=[](const MethodInfo*)->const Il2CppType*{
        return &tVoid;
    };
    api.method_is_instance=[](const MethodInfo*){return true;};
    api.type_get_name=[](const Il2CppType* type){return ::strdup(type->name);};
    api.il2cpp_free=std::free;
    api.class_is_valuetype=[](const Il2CppClass* cls) {
        return cls==&kFishing||cls==&kInt||cls==&kBool||cls==&kFloat;
    };
    api.class_value_size=[](Il2CppClass* cls,uint32_t* alignment)->int32_t {
        if(alignment)*alignment=4;
        if(cls==&kFishing)return badFishingSize ? 88 : 92;
        if(cls==&kBool)return 1;
        return 4;
    };
    api.class_get_fields=[](Il2CppClass* cls,void** iterator)->FieldInfo* {
        if(cls!=&kFishing)return nullptr;
        FieldInfo* fields[]={&fQuest,&fUncommon,&fRolled,&fLast};
        const size_t cursor=reinterpret_cast<uintptr_t>(*iterator);
        if(cursor>=4)return nullptr;
        *iterator=reinterpret_cast<void*>(cursor+1);
        return fields[cursor];
    };
    api.field_get_flags=[](FieldInfo* field)->uint32_t{
        return field->isStatic &&
            !(badPlayersStatic && field==&fPlayers) &&
            !(badNetModeStatic && field==&fNetMode) ? 0x10u : 0u;
    };
    api.field_get_type=[](FieldInfo* field){
        return ((badQuestFieldType && field==&fQuest) ||
                (badNetModeType && field==&fNetMode)) ? &tBool : field->type;
    };
    api.field_get_offset=[](FieldInfo* field){
        return badFieldOverlap && field==&fRolled ? fQuest.offset : field->offset;
    };
    api.class_from_il2cpp_type=[](const Il2CppType* type){return type->cls;};
    api.class_get_namespace=[](Il2CppClass* cls){return cls->ns;};
    api.class_get_name=[](Il2CppClass* cls){return cls->name;};
    api.class_instance_size=[](Il2CppClass* cls)->uint32_t{
        if(cls==&kProjectile)return sizeof(ProjectileObject);
        if(cls==&kPlayer)return sizeof(PlayerObject);
        return sizeof(Il2CppObject);
    };
    api.field_static_get_value=[](FieldInfo* field,void* dest){
        if(field==&fPlayers)std::memcpy(dest,&players,sizeof(players));
        if(field==&fNetMode)std::memcpy(dest,&netMode,sizeof(netMode));
    };
    api.object_get_class=[](Il2CppObject* object){return object->klass;};
    api.class_get_element_class=[](Il2CppClass* cls){return cls->element;};
    api.class_get_rank=[](const Il2CppClass* cls)->int {
        if(badPlayersRank && cls==&kPlayersArray)return 2;
        return cls==&kPlayersArray || cls==&kFakePlayersArray ? 1 : 0;
    };
}
void initPlayers() {
    players=static_cast<Il2CppArray*>(std::calloc(
        1,sizeof(Il2CppArray)+sizeof(Il2CppObject*)*2));
    players->obj.klass=&kPlayersArray;
    players->length=2;
    player.object.klass=&kPlayer;
    projectile.object.klass=&kProjectile;
    static_cast<Il2CppObject**>(arrayData(players))[0]=&player.object;
}
}
namespace bl::csharp {
bool gManagedAllowed = true;
bool gManagedReady = true;
bool gUnloadDuringCall = false;
bool gChangeOwnerDuringCall = false;
bool gChangePlayersDuringCall = false;
int gManagedCalls = 0;
int gLastPlayerIndex = -999;
bool invokeManagedAnglerQuestCallback(const std::string&,const std::string&,
                                      int,int,int,int,bool* allowed,int playerIndex) {
    ++gManagedCalls;
    gLastPlayerIndex = playerIndex;
    if (!gManagedReady) return false;
    if (gUnloadDuringCall) bl::csharp::angler::registry().unload("examplemodcsharp");
    if (gChangeOwnerDuringCall) projectile.owner=1;
    if (gChangePlayersDuringCall)
        static_cast<Il2CppObject**>(arrayData(players))[0]=nullptr;
    *allowed = gManagedAllowed;
    return true;
}
}
namespace bl::il2cpp {
Api& api(){static Api value;return value;}
Il2CppClass* findClass(const TypeRef& type) {
    if(type.ns=="Terraria"&&type.name=="Projectile")return &kProjectile;
    if(type.ns=="Terraria"&&type.name=="Player")return &kPlayer;
    if(type.ns=="Terraria"&&type.name=="Main")return &kMain;
    if(type.ns=="Terraria.DataStructures"&&type.name=="FishingAttempt")return &kFishing;
    return nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(cls==&kProjectile&&name=="owner")return &fOwner;
    if(cls==&kPlayer&&name=="gravDir")return &fGravity;
    if(cls==&kMain&&name=="player")return &fPlayers;
    if(cls==&kMain&&name=="netMode")return &fNetMode;
    if(cls==&kFishing) {
        for(auto* field:{&fQuest,&fUncommon,&fRolled,&fLast})
            if(name==field->name)return field;
    }
    return nullptr;
}
}
namespace bl::hook {
bool install(const MethodInfo* method,void* replacement,void** original) {
    if(method!=&kRoll)return false;
    hookedRoll=reinterpret_cast<Roll>(replacement);
    *original=reinterpret_cast<void*>(vanillaRoll);
    return true;
}
}
namespace bl::log {
void write(int,const char*,...){}
}

int main() {
    configure();
    initPlayers();
    using bl::csharp::angler::Quest;
    std::vector<Quest> quests{
        Quest{"examplemodcsharp","ExampleQuestFish",850,false,true,"","",3}
    };
    badPlayersStatic=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!bl::csharp::angler::fishingInstalled());
    assert(!hookedRoll);
    badPlayersStatic=false;
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedPlayerArrayContract")==0);
    badPlayersRank=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedPlayerArrayContract")==0);
    badPlayersRank=false;
    auto& exports=bl::il2cpp::api();
    const auto originalTypeName=exports.type_get_name;
    exports.type_get_name=nullptr;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "MissingIL2CPPExports")==0);
    exports.type_get_name=originalTypeName;
    badFishingSize=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedFishingAttemptLayout")==0);
    badFishingSize=false;
    badFishingParameter=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedFishingMethodSignature")==0);
    badFishingParameter=false;
    badQuestFieldType=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedFishingFieldLayout")==0);
    badQuestFieldType=false;
    badFieldOverlap=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedFishingFieldLayout")==0);
    badFieldOverlap=false;
    badNetModeStatic=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedNetModeContract")==0);
    badNetModeStatic=false;
    badNetModeType=true;
    assert(!bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(!hookedRoll);
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "UnexpectedNetModeContract")==0);
    badNetModeType=false;
    assert(bl::csharp::angler::installFishingOnGameThread(&quests));
    assert(std::strcmp(bl::csharp::angler::fishingInstallDiagnostic(),
                       "Installed")==0);
    assert(bl::csharp::angler::fishingInstalled());
    assert(hookedRoll);
    std::array<unsigned char,92> attempt{};
    put(&attempt,0,int32_t(850));
    put(&attempt,4,uint8_t(1));
    player.gravDir=1.f;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    player.gravDir=-1.f;
    put(&attempt,4,uint8_t(0));
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    put(&attempt,4,uint8_t(1));
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    for (const int mode : {0, 1, 2, 3, -1, 4}) {
        netMode=mode;
        hookedRoll(&projectile.object,attempt.data(),&kRoll);
        assert(get(attempt,8)==(mode >= 0 && mode <= 3 ? 850 : 13));
    }
    netMode=0;
    put(&attempt,0,int32_t(100));
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    put(&attempt,0,int32_t(850));
    projectile.owner=2;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    projectile.owner=0;
    quests[0].callbackMask=2;
    bl::csharp::gManagedAllowed=false;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    bl::csharp::gManagedReady=false;
    bl::csharp::gManagedAllowed=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    bl::csharp::gManagedReady=true;
    bl::csharp::gChangeOwnerDuringCall=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    projectile.owner=0;
    bl::csharp::gChangeOwnerDuringCall=false;
    bl::csharp::gChangePlayersDuringCall=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    static_cast<Il2CppObject**>(arrayData(players))[0]=&player.object;
    bl::csharp::gChangePlayersDuringCall=false;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    quests[0].callbackMask=0;
    assert(bl::csharp::gManagedCalls==5);
    assert(bl::csharp::gLastPlayerIndex==0);
    quests[0].catchRules=0;
    quests[0].catchEnabled=false;
    quests[0].callbackMask=2;
    bl::csharp::gManagedAllowed=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    assert(bl::csharp::gManagedCalls==6);
    quests[0].callbackMask=0;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    quests[0].catchEnabled=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    players->obj.klass=&kFakePlayersArray;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    players->obj.klass=&kPlayersArray;
    player.object.klass=&kWrongPlayer;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    player.object.klass=&kPlayer;
    players->length=257;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    players->length=2;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    std::string error;
    auto& active=bl::csharp::angler::registry();
    assert(active.publish("examplemodcsharp",quests,&error));
    bl::csharp::angler::activeGeneration().store(
        active.generation(),std::memory_order_release);
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==850);
    quests[0].callbackMask=2;
    assert(active.publish("examplemodcsharp",quests,&error));
    bl::csharp::angler::activeGeneration().store(
        active.generation(),std::memory_order_release);
    bl::csharp::gUnloadDuringCall=true;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    bl::csharp::gUnloadDuringCall=false;
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    quests.clear();
    hookedRoll(&projectile.object,attempt.data(),&kRoll);
    assert(get(attempt,8)==13);
    assert(vanillaCalls==27);
    std::cout<<"ANGLER_QUEST_FISHING_IL2CPP_HOST_OK\n";
}
