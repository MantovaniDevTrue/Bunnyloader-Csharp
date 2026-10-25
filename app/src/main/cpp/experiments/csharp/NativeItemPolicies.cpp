#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeStaticValue.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/NativeContent.h"
#include "content/buffs/ModBuffs.h"
#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "mods/ModLoader.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <sys/stat.h>

namespace bl::csharp {
namespace item_native {
using namespace bl::il2cpp;
struct Root {
    uint32_t handle=0;
    explicit Root(Il2CppObject* object){auto& a=api();if(object&&a.gchandle_new)handle=a.gchandle_new(object,false);}
    ~Root(){if(handle&&api().gchandle_free)api().gchandle_free(handle);}
    Il2CppObject* get() const {return handle&&api().gchandle_get_target?api().gchandle_get_target(handle):nullptr;}
};
struct Field { int offset=-1;size_t bytes=0;std::vector<size_t> refs; };
struct Entry {
    BasicItemEntry source;BasicItemPolicy policy;NativeItemPolicyStatus status;
    std::shared_ptr<Root> clone,animation;std::vector<Field> cloned;
    int color=-1,hair=-1;uint32_t nativeAlpha=0;
    unsigned nextRetryTick=0,failedAttempts=0;
    uint64_t generation=0;
    const MethodInfo* localPlayer=nullptr;
    std::map<std::string,int> slots;
    std::array<int,3> costumeAltSlots{-1,-1,-1};
    int costumeBuffType=-1;
};
struct Snapshot {
    std::map<int,std::shared_ptr<const Entry>> items;
    std::map<int,std::array<float,8>> wings;
    std::map<int,int> robes;
};
std::shared_ptr<const Snapshot> snapshot;
std::mutex queueMutex;
std::map<int,std::shared_ptr<Entry>> incoming;
std::map<int,uint64_t> desiredGeneration;
std::map<int,std::string> desiredOwner;
std::map<int,uint64_t> staticRetirements;
std::map<int,std::string> staticRetirementOwners;
uint64_t nextGeneration=0;
// Old snapshots may own GC roots. The game thread, never ingress, releases them.
std::vector<std::shared_ptr<const Snapshot>> retiringSnapshots;
std::map<int,NativeItemPolicyStatus> statuses;
// Only the game thread mutates installation state. Ingress/diagnostics use the
// separate mutex, never held while calling NativeBasicItems or the game.
std::map<int,std::shared_ptr<Entry>> entries;
std::atomic<size_t> pendingCount{0};
int itemType=-1;
template<class T>T read(const void* p,size_t offset){T v;std::memcpy(&v,static_cast<const char*>(p)+offset,sizeof(v));return v;}
template<class T>void write(void* p,size_t offset,T v){std::memcpy(static_cast<char*>(p)+offset,&v,sizeof(v));}
bool primitive(Il2CppClass* type,const char* name,size_t bytes) {
    auto& a=api();uint32_t align=0;const char* ns=type&&a.class_get_namespace?a.class_get_namespace(type):nullptr;
    const char* actual=type&&a.class_get_name?a.class_get_name(type):nullptr;
    return ns&&actual&&!std::strcmp(ns,"System")&&!std::strcmp(actual,name)&&a.class_is_valuetype(type)&&a.class_value_size(type,&align)==static_cast<int>(bytes);
}
bool bindField(Il2CppClass* cls,const char* name,Field* out,std::string* error,bool isStatic=false,const char* primitiveName=nullptr,size_t expected=0) {
    auto& a=api();auto* f=cls?findField(cls,name):nullptr;auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;
    auto fail=[&]{*error=std::string("Field=")+name+" Reason=NativeFieldContract";return false;};
    if(!f||!type||bool(a.field_get_flags(f)&0x10u)!=isStatic)return fail();
    uint32_t align=0;const bool value=a.class_is_valuetype(type);
    const int bytes=value?a.class_value_size(type,&align):static_cast<int>(sizeof(void*));
    if(bytes<1||bytes>65536||(expected&&static_cast<size_t>(bytes)!=expected)||
       (primitiveName&&!primitive(type,primitiveName,expected)))return fail();
    const size_t offset=a.field_get_offset(f),size=isStatic?65536:a.class_instance_size(cls);
    if(offset>INT32_MAX||(!isStatic&&(offset<sizeof(Il2CppObject)||static_cast<size_t>(bytes)>size||offset>size-static_cast<size_t>(bytes))))return fail();
    out->offset=static_cast<int>(offset);out->bytes=static_cast<size_t>(bytes);
    if(!value)out->refs.push_back(0);
    else if(!valueReferenceOffsets(type,0,&out->refs,error))return false;
    return true;
}
bool scalar(Il2CppClass* cls,const char* name,const char* type,size_t bytes,int* offset,std::string* error) {
    Field f;if(!bindField(cls,name,&f,error,false,type,bytes))return false;*offset=f.offset;return true;
}
bool invoke(const MethodInfo* method,void* self,void** args,Il2CppObject** result=nullptr) {
    if(!method||!api().runtime_invoke)return false;
    Il2CppObject* error=nullptr;
    auto* value=api().runtime_invoke(method,self,args,&error);if(result)*result=value;return !error;
}
const MethodInfo* method(Il2CppClass* cls,const char* signature,bool instance) {
    const auto* m=cls?findMethodBySignature(cls,parseSignature(signature)):nullptr;
    return m&&api().method_is_instance&&api().method_is_instance(m)==instance?m:nullptr;
}
Il2CppClass* nested(const char* kind) {auto* armor=findClass({"Terraria.ID","ArmorIDs",{}});return armor?findNested(armor,kind):nullptr;}
using StaticValue=native_static::Value;
StaticValue staticArray(Il2CppClass* cls,const char* name,Il2CppClass* element=nullptr) {
    auto& a=api();auto value=native_static::resolve(cls,name);auto* type=value.type;
    if(!value||!type||a.class_get_rank(type)!=1||a.class_is_valuetype(type)||
       (element&&a.class_get_element_class(type)!=element))return {};
    return value;
}
Il2CppArray* table(const StaticValue& f) {
    Il2CppArray* array=nullptr;
    if(!native_static::read(f,&array,sizeof(array))||!array||api().object_get_class(reinterpret_cast<Il2CppObject*>(array))!=f.type)return nullptr;
    return array;
}
Il2CppArray* table(FieldInfo* f) {Il2CppArray* array=nullptr;if(f)api().field_static_get_value(f,&array);return array;}
bool grow(const StaticValue& f,int total) {
    auto* array=table(f);if(!array||total<1||array->length>65536)return false;
    if(array->length>=static_cast<uintptr_t>(total))return true;
    if(!f.writable())return false;
    auto* next=runtime::TypeTables::resizedCopy(array,static_cast<uintptr_t>(total));if(!next)return false;
    return native_static::write(f,&next,sizeof(next));
}
bool boolTable(const StaticValue& f,int slot,bool value) {
    auto* array=table(f);auto& a=api();
    if(!array||slot<0||!primitive(a.class_get_element_class(a.object_get_class(reinterpret_cast<Il2CppObject*>(array))),"Boolean",1)||!grow(f,slot+1))return false;
    write<uint8_t>(arrayData(table(f)),static_cast<size_t>(slot),value?1:0);return true;
}
// Primitive cells contain no object references. Keep bytes, never an array/data
// pointer: Terraria can replace its static arrays during initialization.
struct OwnedStaticCell {
    StaticValue field;Il2CppClass* element=nullptr;int slot=-1;
    std::vector<unsigned char> original,last;
};
std::map<std::pair<int,std::string>,OwnedStaticCell> ownedStaticCells;
// Missing tables/invalid ABI are retryable. A shorter, valid replacement array
// has already discarded this cell, so there is nothing to restore or regrow.
bool staticCell(const OwnedStaticCell& cell,unsigned char** out) {
    *out=nullptr;auto* array=table(cell.field);auto& a=api();uint32_t align=0;
    if(!array||array->length>65536||cell.slot<0||
       a.class_get_element_class(cell.field.type)!=cell.element||
       !a.class_is_valuetype(cell.element)||
       a.class_value_size(cell.element,&align)!=static_cast<int>(cell.last.size()))return false;
    if(array->length<=static_cast<uintptr_t>(cell.slot))return true;
    *out=static_cast<unsigned char*>(arrayData(array))+cell.last.size()*static_cast<size_t>(cell.slot);
    return true;
}
bool writeOwnedStaticCell(int type,const std::string& name,const StaticValue& field,int slot,
                          const void* data,size_t bytes) {
    if(!field||!data||bytes==0||bytes>256)return false;
    auto key=std::make_pair(type,name);auto found=ownedStaticCells.find(key);
    OwnedStaticCell next;next.field=field;next.slot=slot;
    next.element=api().class_get_element_class(field.type);next.last.resize(bytes);
    unsigned char* destination=nullptr;
    if(!staticCell(next,&destination)||!destination)return false;
    if(found!=ownedStaticCells.end()) {
        const auto& old=found->second;
        if(old.slot!=slot||old.field.field!=field.field||old.field.getter!=field.getter||
           old.element!=next.element||old.last.size()!=bytes)return false;
        // A foreign write or a reinitialized table becomes the baseline of the
        // next explicit publication. Repeated writes keep the original baseline.
        next.original=std::memcmp(destination,old.last.data(),bytes)==0?old.original:
            std::vector<unsigned char>(destination,destination+bytes);
    } else next.original.assign(destination,destination+bytes);
    std::memcpy(next.last.data(),data,bytes);
    std::memcpy(destination,data,bytes);
    ownedStaticCells[key]=std::move(next);return true;
}
bool ownedBoolTable(int type,const char* name,bool value) {
    auto* ids=findClass({"Terraria.ID","ItemID","Sets"});
    const auto field=staticArray(ids,name);auto* array=table(field);
    if(!array||!primitive(api().class_get_element_class(field.type),"Boolean",1)||!grow(field,type+1))return false;
    const uint8_t flag=value?1:0;return writeOwnedStaticCell(type,name,field,type,&flag,1);
}
bool restoreOwnedStaticCells(int type) {
    bool complete=true;
    for(auto it=ownedStaticCells.lower_bound({type,{}});
        it!=ownedStaticCells.end()&&it->first.first==type;) {
        const auto& cell=it->second;unsigned char* destination=nullptr;
        if(!staticCell(cell,&destination)){complete=false;++it;continue;}
        // Compare before restoring: preserve a value another mod changed since
        // our last publication. Do not grow an array just to revive a removed cell.
        if(destination&&std::memcmp(destination,cell.last.data(),cell.last.size())==0)
            std::memcpy(destination,cell.original.data(),cell.original.size());
        it=ownedStaticCells.erase(it);
    }
    return complete;
}
bool pendingStaticRetirement(int type) {
    std::lock_guard lock(queueMutex);return staticRetirements.contains(type);
}
void pumpStaticRetirements() {
    // Bound native work and rotate past unavailable tables; an unload with many
    // items must not restore every struct in a single frame or starve other UIDs.
    static int cursor=-1;
    std::map<int,uint64_t> requests;
    {std::lock_guard lock(queueMutex);requests=staticRetirements;}
    auto request=requests.upper_bound(cursor);size_t attempted=0;
    for(size_t scanned=0;scanned<requests.size();++scanned) {
        if(request==requests.end())request=requests.begin();
        const auto [type,revision]=*request++;
        const auto cell=ownedStaticCells.lower_bound({type,{}});
        if(cell!=ownedStaticCells.end()&&cell->first.first==type) {
            if(attempted==8)break;
            ++attempted;cursor=type;
        }
        if(!restoreOwnedStaticCells(type))continue;
        std::lock_guard lock(queueMutex);const auto latest=staticRetirements.find(type);
        if(latest!=staticRetirements.end()&&latest->second==revision) {
            staticRetirements.erase(latest);staticRetirementOwners.erase(type);
        }
    }
}
bool setTexture(const StaticValue& field,int slot,Il2CppObject* value) {
    if(field.field)return runtime::content::setTableElement(field.field,slot,value);
    auto* array=table(field);if(!array||slot<0||static_cast<uintptr_t>(slot)>=array->length)return false;
    auto* cls=findClass({"System","Array",{}});const auto* set=method(cls,"void SetValue(object value, int index)",true);
    void* args[]={value,&slot};return invoke(set,array,args);
}
struct EquipKind {
    std::string ids;int vanilla=0,total=0;
    std::unique_ptr<runtime::TypeTables> sets;
};
struct Equipment {
    int type=-1,slot=-1;bool published=false;std::string kind,path;int flags=0;
    std::shared_ptr<Root> asset;
};
std::map<std::string,EquipKind> kinds;
std::map<std::pair<int,std::string>,Equipment> equipment;
struct KindInfo { const char* ids;const char* itemTypes;std::vector<const char*> textures;bool composite=false; };
KindInfo kindInfo(const std::string& kind) {
    if(kind=="Head")return {"Head","headType",{"ArmorHead"}};
    if(kind=="Body")return {"Body","bodyType",{"ArmorBodyComposite","ArmorBody","FemaleBody","ArmorArm"},true};
    if(kind=="Legs")return {"Legs","legType",{"ArmorLeg"}};
    if(kind=="Beard")return {"Beard","",{"AccBeard"}};
    if(kind=="Back")return {"Back","",{"AccBack"}};
    if(kind=="Wings")return {"Wing","",{"Wings"}};
    if(kind=="Shield")return {"Shield","",{"AccShield"}};
    return {"Shoe","",{"AccShoes"}};
}
bool installEquipment(Equipment& equip,std::string* error) {
    auto fail=[&](const char* why){*error="Kind="+equip.kind+" Reason="+why;return false;};
    auto& a=api();const auto info=kindInfo(equip.kind);auto* ids=nested(info.ids);
    const auto countField=native_static::resolve(ids,"Count");int64_t nativeCount=0;
    if(!native_static::readInteger(countField,&nativeCount)||!countField.writable())return fail("EquipmentCountUnavailable");
    if(nativeCount<=0||nativeCount>=32767)return fail("EquipmentCountInvalid");
    const int count=static_cast<int>(nativeCount);
    if(!equip.published)equip.slot=count;
    int total=std::max(count,equip.slot+1);
    if(!*info.itemTypes&&total>128)return fail("SignedByteSlotLimit");
    if(!native_static::fitsInteger(countField,total))return fail("EquipmentCountOverflow");
    auto& kind=kinds[equip.kind];
    auto* sets=ids?findNested(ids,"Sets"):nullptr;
    if(!kind.sets&&sets) {
        kind.ids=info.ids;kind.vanilla=count;kind.total=count;
        kind.sets=std::make_unique<runtime::TypeTables>("C# equipment",count,
            std::vector<runtime::TableClass>{{"Terraria.ID","ArmorIDs",kind.ids.c_str(),"Sets"}},runtime::TypeTables::Fill::Mode);
    }
    // Some Android equipment kinds (e.g. Shield) have no Sets class.
    // Require only the tables actually used by this descriptor.
    if(!sets&&(info.composite||equip.flags||equip.kind=="Wings"))return fail("EquipmentSetsUnavailable");
    if(sets&&a.runtime_class_init)a.runtime_class_init(sets);
    if(equip.kind=="Wings"&&!table(staticArray(sets,"Stats"))) {
        auto* initializer=findClass({"Terraria.Initializers","WingStatsInitializer",{}});
        if(!invoke(method(initializer,"void Load()",false),nullptr,nullptr))return fail("WingStatsUnavailable");
    }
    // Validate all participating arrays before growing any of them.
    auto* assets=findClass({"Terraria.GameContent","TextureAssets",{}});
    auto* assetClass=runtime::content::textureAssetClass();
    if(!assetClass)return fail("TextureAssetTypeUnavailable");
    std::vector<StaticValue> textures;
    for(const char* name:info.textures) {
        auto f=staticArray(assets,name,assetClass);if(!f||!table(f))return fail("EquipmentTextureTableUnavailable");textures.push_back(f);
    }
    auto mapping=*info.itemTypes?staticArray(findClass({"Terraria","Item",{}}),info.itemTypes,findClass({"System","Int32",{}})):StaticValue{};
    if(*info.itemTypes&&(!mapping||!table(mapping)))return fail("EquipmentItemMapUnavailable");
    if(!equip.asset) {
        struct stat file{};if(stat(equip.path.c_str(),&file)||!S_ISREG(file.st_mode)||file.st_size<1||file.st_size>1024*1024)return fail("EquipmentTextureMissingOrTooLarge");
        auto* asset=runtime::content::loadTextureAsset(equip.path,nullptr,0,"CSharpEquip:"+std::to_string(equip.type)+":"+equip.kind,nullptr,nullptr);
        if(!asset)return fail("EquipmentTextureLoadFailed");
        auto rooted=std::make_shared<Root>(asset);
        if(!rooted->get()){runtime::content::destroyTextureAsset(asset);return fail("EquipmentTextureRootUnavailable");}
        equip.asset=std::move(rooted);
    }
    if(!equip.asset->get())return fail("EquipmentTextureRootUnavailable");
    // TypeTables preserves existing mod slots and uses the vanilla modal fill.
    if(kind.sets) {
        kind.sets->grow(count,total);kind.sets->checkPending(total);
        for(auto* f:kind.sets->tables())if(!table(f)||table(f)->length<static_cast<uintptr_t>(total))return fail("EquipmentSetGrowthFailed");
    }
    for(const auto& f:textures)if(!grow(f,total))return fail("EquipmentTextureGrowthFailed");
    if(mapping&&!grow(mapping,total))return fail("EquipmentItemMapGrowthFailed");
    for(const auto& f:textures)if(!setTexture(f,equip.slot,equip.asset->get()))return fail("EquipmentTextureWriteFailed");
    if(mapping)write<int32_t>(arrayData(table(mapping)),static_cast<size_t>(equip.slot)*4,equip.type);
    if(info.composite&&!boolTable(staticArray(sets,"UsesNewFramingCode"),equip.slot,true))return fail("EquipmentFramingUnavailable");
    if((equip.flags&1)&&!boolTable(staticArray(sets,"UseHairColor"),equip.slot,true))return fail("EquipmentHairFlagUnavailable");
    if(equip.flags&2) {
        // Android 1.4.5 uses MissingHand (show the skin hands). HidesHands in
        // other contracts expresses the opposite policy; never reuse its value.
        auto hands=staticArray(sets,"MissingHand");
        const bool missingHand=bool(hands);
        if(!hands)hands=staticArray(sets,"HidesHands");
        if(!boolTable(hands,equip.slot,missingHand))return fail("EquipmentHandFlagUnavailable");
    }
    if(!native_static::writeInteger(countField,total))return fail("EquipmentCountWriteFailed");
    kind.total=total;equip.published=true;return true;
}
using VoidFn=void (*)(const MethodInfo*);
VoidFn originalAnimations=nullptr,originalWingStats=nullptr;
void restoreAnimations();void restoreWingStats();
void hkAnimations(const MethodInfo* m){if(originalAnimations)originalAnimations(m);restoreAnimations();}
void hkWingStats(const MethodInfo* m){if(originalWingStats)originalWingStats(m);restoreWingStats();}
bool animationsHooked=false,wingStatsHooked=false;
bool registerAnimation(Entry& entry,std::string* error) {
    auto& a=api();auto* main=findClass({"Terraria","Main",{}});
    const auto* registerMethod=method(main,"void RegisterItemAnimation(int index, DrawAnimation animation)",false);
    const auto* init=method(main,"void InitializeItemAnimations()",false);
    auto* cls=findClass({"Terraria.DataStructures","DrawAnimationVertical",{}});
    const auto* ctor=method(cls,"void .ctor(int ticksperframe, int frameCount, bool pingPong)",true);
    const bool hasPingPong=ctor!=nullptr;
    if(!ctor)ctor=method(cls,"void .ctor(int ticksperframe, int frameCount)",true);
    auto fail=[&](const char* why){*error=std::string("Reason=")+why;return false;};
    if(!registerMethod)return fail("AnimationRegisterMethodUnavailable");
    if(!init)return fail("AnimationInitializerUnavailable");
    if(!ctor)return fail("AnimationConstructorUnavailable");
    if(!animationsHooked){if(!hook::install(init,hkAnimations,&originalAnimations))return fail("AnimationHookFailed");animationsHooked=true;}
    if(!entry.animation) {
        auto* object=a.object_new(cls);if(!object)return fail("AnimationAllocationFailed");
        auto root=std::make_shared<Root>(object);if(!root->get())return fail("AnimationRootUnavailable");
        uint8_t pingPong=0;
        void* args[]={&entry.policy.ticks,&entry.policy.frames,&pingPong};
        if(!invoke(ctor,object,args))return fail(hasPingPong?"AnimationConstructor3Failed":"AnimationConstructor2Failed");
        entry.animation=std::move(root);
    }
    void* args[]={&entry.source.type,entry.animation->get()};
    if(!invoke(registerMethod,nullptr,args))return fail("AnimationRegisterFailed");
    const char* names[]={"AnimatesAsSoul","ItemIconPulse","ItemNoGravity"};
    for(size_t i=0;i<3;++i)if(!ownedBoolTable(entry.source.type,names[i],entry.policy.animationFlags[i])){
        *error=std::string("Reason=AnimationFlagUnavailable Table=")+names[i];return false;
    }
    return true;
}
// Hooks may run between ingress revocation and the next game-thread pump.
// The mutable installation cache still owns roots then, but cannot republish them.
bool isDesired(const Entry& entry) {
    std::lock_guard lock(queueMutex);
    const auto wanted=desiredGeneration.find(entry.source.type);
    return wanted!=desiredGeneration.end()&&wanted->second==entry.generation;
}
void restoreAnimations(){for(auto& [type,entry]:entries){(void)type;if(entry->status.ready&&entry->policy.animated&&isDesired(*entry)){std::string error;registerAnimation(*entry,&error);}}}
bool writeWingStats(const Entry& entry,std::string* error) {
    auto found=entry.slots.find("Wings");if(found==entry.slots.end()){*error="Reason=WingEquipSlotNotRegistered";return false;}
    auto* ids=nested("Wing");auto* sets=ids?findNested(ids,"Sets"):nullptr;
    auto f=staticArray(sets,"Stats");auto* array=table(f);auto& a=api();
    if(!array||!grow(f,found->second+1)){*error="Reason=WingStatsUnavailable";return false;}array=table(f);
    auto* type=a.class_get_element_class(a.object_get_class(reinterpret_cast<Il2CppObject*>(array)));
    ValueLayout layout;if(!resolveValueLayout(type,&layout,error))return false;
    std::vector<size_t> references;if(!valueReferenceOffsets(type,0,&references,error))return false;
    if(!references.empty()){*error="Reason=WingStatsUnexpectedReferences";return false;}
    struct Value { const char* name;const char* type;size_t bytes;float number; };
    const auto& w=entry.policy.wings;
    const Value values[]={{"FlyTime","Int32",4,w[0]},{"AccRunSpeedOverride","Single",4,w[1]},{"AccRunAccelerationMult","Single",4,w[2]},
        {"HasDownHoverStats","Boolean",1,0},{"DownHoverSpeedOverride","Single",4,-1},{"DownHoverAccelerationMult","Single",4,1}};
    std::array<size_t,6> offsets{};
    for(size_t i=0;i<6;++i) {
        auto* field=findField(type,values[i].name);
        if(!field||(a.field_get_flags(field)&0x10u)||!primitive(a.class_from_il2cpp_type(a.field_get_type(field)),values[i].type,values[i].bytes)||
            !layout.offset(a.field_get_offset(field),values[i].bytes,&offsets[i])){
            *error=std::string("Reason=WingStatsFieldMismatch Field=")+values[i].name;
            return false;
        }
    }
    // Initialize the entire new struct from WingStats.Default, preserving all
    // other fields in this Terraria version (not just the fields we override).
    auto* defaultField=findField(type,"Default");
    if(a.runtime_class_init)a.runtime_class_init(type);
    std::vector<unsigned char> initialized(layout.size);
    if(defaultField&&(a.field_get_flags(defaultField)&0x10u)&&a.class_from_il2cpp_type(a.field_get_type(defaultField))==type)
        a.field_static_get_value(defaultField,initialized.data());
    else {
        Il2CppObject* boxed=nullptr;
        if(invoke(method(type,"WingStats get_Default()",false),nullptr,nullptr,&boxed)&&boxed&&a.object_get_class(boxed)==type)
            std::memcpy(initialized.data(),reinterpret_cast<const char*>(boxed)+sizeof(Il2CppObject),layout.size);
        else if(array->length>1)
            std::memcpy(initialized.data(),static_cast<char*>(arrayData(array))+layout.size,layout.size);
        else { *error="Reason=WingStatsDefaultUnavailable";return false; }
    }
    for(size_t i=0;i<6;++i) {
        if(i==0)write<int32_t>(initialized.data(),offsets[i],static_cast<int32_t>(values[i].number));
        else if(i==3)write<uint8_t>(initialized.data(),offsets[i],0);
        else write<float>(initialized.data(),offsets[i],values[i].number);
    }
    if(!writeOwnedStaticCell(entry.source.type,"WingStats",f,found->second,initialized.data(),layout.size)) {
        *error="Reason=WingStatsOwnershipUnavailable";return false;
    }
    return true;
}
void restoreWingStats(){for(const auto& [type,entry]:entries){(void)type;if(entry->status.ready&&entry->policy.wing&&isDesired(*entry)){std::string error;writeWingStats(*entry,&error);}}}
struct WingRefs {int logic=-1,velocity=-1,gravity=-1,time=-1,maxTime=-1,brooch=-1;StaticValue jump;};
WingRefs wingRefs;
using PlayerFn=void (*)(Il2CppObject*,const MethodInfo*);
PlayerFn originalMovement=nullptr;bool movementHooked=false;
void hkMovement(Il2CppObject* player,const MethodInfo* m) {
    const auto view=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);
    if(!player||!view){if(originalMovement)originalMovement(player,m);return;}
    const int logic=read<int32_t>(player,wingRefs.logic);const auto found=view->wings.find(logic);
    if(found==view->wings.end()){if(originalMovement)originalMovement(player,m);return;}
    float jump=0;
    if(wingRefs.jump.field)api().field_static_get_value(wingRefs.jump.field,&jump);
    else {using JumpFn=float (*)(const MethodInfo*);jump=reinterpret_cast<JumpFn>(methodPointer(wingRefs.jump.getter))(wingRefs.jump.getter);}
    const float gravity=read<float>(player,wingRefs.gravity),time=read<float>(player,wingRefs.time);
    const float old=read<float>(player,static_cast<size_t>(wingRefs.velocity)+4),maxTime=read<int32_t>(player,wingRefs.maxTime);
    if(!std::isfinite(jump)||jump<0||!std::isfinite(old)||!std::isfinite(time)||!std::isfinite(maxTime)||(gravity!=1&&gravity!=-1))return;
    const auto& w=found->second;float y=old-w[7]*gravity;
    if(gravity==1){if(y>0)y-=w[3];else if(y>-jump*w[5])y-=w[4];y=std::max(y,-jump*w[6]);}
    else {if(y<0)y+=w[3];else if(y<jump*w[5])y+=w[4];y=std::min(y,jump*w[6]);}
    float remaining=time-1;
    if(wingRefs.brooch>=0&&read<uint8_t>(player,wingRefs.brooch)&&remaining!=0)remaining=maxTime;
    if(!std::isfinite(y)||!std::isfinite(remaining))return;
    write<float>(player,static_cast<size_t>(wingRefs.velocity)+4,y);write<float>(player,wingRefs.time,remaining);
}
bool prepareWings(std::string* error) {
    if(movementHooked&&wingStatsHooked)return true;
    auto& a=api();auto* player=findClass({"Terraria","Player",{}});Field velocity;
    if(!scalar(player,"wingsLogic","Int32",4,&wingRefs.logic,error)||!bindField(player,"velocity",&velocity,error,false,nullptr,8)||!velocity.refs.empty()||
        !scalar(player,"gravDir","Single",4,&wingRefs.gravity,error)||!scalar(player,"wingTime","Single",4,&wingRefs.time,error)||
        !scalar(player,"wingTimeMax","Int32",4,&wingRefs.maxTime,error))return false;
    wingRefs.brooch=-1;
    auto* brooch=findField(player,"empressBrooch");
    if(brooch&&(a.field_get_flags(brooch)&0x10u)==0&&primitive(a.class_from_il2cpp_type(a.field_get_type(brooch)),"Boolean",1))
        wingRefs.brooch=static_cast<int>(a.field_get_offset(brooch));
    auto* vector=a.class_from_il2cpp_type(a.field_get_type(findField(player,"velocity")));ValueLayout layout;
    if(!resolveValueLayout(vector,&layout,error))return false;
    for(const auto& [name,offset]:std::initializer_list<std::pair<const char*,size_t>>{{"X",0},{"Y",4}}) {
        auto* f=findField(vector,name);size_t actual=0;
        if(!f||!primitive(a.class_from_il2cpp_type(a.field_get_type(f)),"Single",4)||!layout.offset(a.field_get_offset(f),4,&actual)||actual!=offset)return false;
    }
    wingRefs.velocity=velocity.offset;wingRefs.jump=native_static::resolve(player,"jumpSpeed");
    if(!wingRefs.jump||!primitive(wingRefs.jump.type,"Single",4)||(!wingRefs.jump.field&&!methodPointer(wingRefs.jump.getter))){
        *error="Reason=WingJumpSpeedContractUnavailable";return false;
    }
    const auto* movement=method(player,"void WingMovement()",true);
    if(!movementHooked){
        if(!movement){*error="Reason=WingMovementMethodUnavailable";return false;}
        if(!hook::install(movement,hkMovement,&originalMovement)){
            *error="Reason=WingMovementHookRejected";return false;
        }
        movementHooked=true;
    }
    const auto* init=method(findClass({"Terraria.Initializers","WingStatsInitializer",{}}),"void Load()",false);
    if(!wingStatsHooked&&init){
        if(hook::install(init,hkWingStats,&originalWingStats))wingStatsHooked=true;
        else BL_WARN("[CSharp][Wing] WingStatsInitializer hook indisponivel; tabela nativa ja registrada e sera preservada sem o hook");
    }
    return true;
}
using AlphaFn=uint32_t (*)(Il2CppObject*,uint32_t,const MethodInfo*);
AlphaFn originalAlpha=nullptr;bool alphaHooked=false;NativeColorLayout alphaLayout;
uint32_t hkAlpha(Il2CppObject* item,uint32_t color,const MethodInfo* m) {
    const auto view=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);
    if(item&&view){const auto found=view->items.find(read<int32_t>(item,itemType));if(found!=view->items.end()&&found->second->policy.colored)color=found->second->nativeAlpha;}
    return originalAlpha?originalAlpha(item,color,m):color;
}
bool prepareAlpha(std::string* error) {
    if(alphaHooked)return true;
    auto& a=api();auto* item=findClass({"Terraria","Item",{}});
    const auto* m=method(item,"Color GetAlpha(Color newColor)",true);
    if(!m){*error="Reason=ColorMethodUnavailable";return false;}
    if(!a.method_get_return_type||!a.method_get_param_count||!a.method_get_param||a.method_get_param_count(m)!=1){
        *error="Reason=ColorMethodMetadataUnavailable";return false;
    }
    auto* color=a.class_from_il2cpp_type(a.method_get_return_type(m));
    auto* parameter=a.class_from_il2cpp_type(a.method_get_param(m,0));
    auto* field=item?findField(item,"color"):nullptr;
    if(!color||parameter!=color||!field||(a.field_get_flags(field)&0x10u)||
       a.class_from_il2cpp_type(a.field_get_type(field))!=color){*error="Reason=ColorAbiMismatch";return false;}
    if(!alphaLayout.bind(color,error))return false;
    if(!hook::install(m,hkAlpha,&originalAlpha)){*error="Reason=ColorHookRejected";return false;}
    alphaHooked=true;return true;
}
struct Vec2 {float x,y;};
struct LightRefs {int inner=-1,active=-1,position=-1,width=-1,height=-1;FieldInfo* essence=nullptr;const MethodInfo* add=nullptr;const MethodInfo* activeGetter=nullptr;};
LightRefs lightRefs;
using WorldFn=void (*)(Il2CppObject*,int32_t,const MethodInfo*);
WorldFn originalWorld=nullptr;bool worldHooked=false;
void hkWorld(Il2CppObject* world,int32_t index,const MethodInfo* m) {
    if(originalWorld)originalWorld(world,index,m);
    if(!world)return;
    auto* item=read<Il2CppObject*>(world,lightRefs.inner);if(!item)return;
    const auto view=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);if(!view)return;
    const auto it=view->items.find(read<int32_t>(item,itemType));if(it==view->items.end()||!it->second->policy.lit)return;
    using ActiveFn=bool (*)(Il2CppObject*,const MethodInfo*);
    const bool active=lightRefs.activeGetter?
        reinterpret_cast<ActiveFn>(methodPointer(lightRefs.activeGetter))(world,lightRefs.activeGetter):read<uint8_t>(world,lightRefs.active)!=0;
    if(!active)return;
    const auto& policy=it->second->policy;float scale=1;if(policy.essence)api().field_static_get_value(lightRefs.essence,&scale);
    auto position=read<Vec2>(world,lightRefs.position);position.x+=read<int32_t>(world,lightRefs.width)*0.5f;position.y+=read<int32_t>(world,lightRefs.height)*0.5f;
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(scale)||scale<0||scale>10)return;
    using AddFn=void (*)(Vec2,float,float,float,const MethodInfo*);
    reinterpret_cast<AddFn>(methodPointer(lightRefs.add))(position,policy.light[0]*scale,policy.light[1]*scale,policy.light[2]*scale,lightRefs.add);
}
bool prepareLight(std::string* error) {
    if(worldHooked)return true;
    auto& a=api();auto* world=findClass({"Terraria","WorldItem",{}});Field inner,position;
    if(!bindField(world,"inner",&inner,error,false,nullptr,sizeof(void*))||inner.refs.size()!=1||
        !bindField(world,"position",&position,error,false,nullptr,8)||!position.refs.empty()||
        !scalar(world,"width","Int32",4,&lightRefs.width,error)||!scalar(world,"height","Int32",4,&lightRefs.height,error))return false;
    if(a.class_from_il2cpp_type(a.field_get_type(findField(world,"inner")))!=findClass({"Terraria","Item",{}})){
        *error="Reason=WorldItemInnerTypeMismatch";return false;
    }
    // WorldItem.active is a bool instance getter on Android 1.4.5. Entity
    // supplies position/width/height; there is no active field to offset-read.
    if(findField(world,"active")) {
        if(!scalar(world,"active","Boolean",1,&lightRefs.active,error))return false;
        lightRefs.activeGetter=nullptr;
    } else {
        const auto* getter=method(world,"bool get_active()",true);
        auto* result=getter&&a.method_get_return_type?a.class_from_il2cpp_type(a.method_get_return_type(getter)):nullptr;
        if(!getter||!a.method_get_param_count||a.method_get_param_count(getter)!=0||
           !primitive(result,"Boolean",1)||!methodPointer(getter)){
            *error="Reason=WorldItemActiveGetterContract";return false;
        }
        lightRefs.activeGetter=getter;lightRefs.active=-1;
    }
    auto* vector=a.class_from_il2cpp_type(a.field_get_type(findField(world,"position")));ValueLayout layout;
    if(!resolveValueLayout(vector,&layout,error)||layout.size!=8)return false;
    for(const auto& [name,offset]:std::initializer_list<std::pair<const char*,size_t>>{{"X",0},{"Y",4}}) {
        auto* f=findField(vector,name);size_t actual=0;
        if(!f||!primitive(a.class_from_il2cpp_type(a.field_get_type(f)),"Single",4)||!layout.offset(a.field_get_offset(f),4,&actual)||actual!=offset){
            *error="Reason=WorldItemPositionLayoutMismatch";return false;
        }
    }
    lightRefs.inner=inner.offset;lightRefs.position=position.offset;
    auto* main=findClass({"Terraria","Main",{}});lightRefs.essence=main?findField(main,"essScale"):nullptr;
    if(!lightRefs.essence||!(a.field_get_flags(lightRefs.essence)&0x10u)||!primitive(a.class_from_il2cpp_type(a.field_get_type(lightRefs.essence)),"Single",4)){
        *error="Reason=ItemLightEssenceScaleContract";return false;
    }
    lightRefs.add=method(findClass({"Terraria","Lighting",{}}),"void AddLight(Vector2 position, float r, float g, float b)",false);
    const auto* update=method(world,"void UpdateItem(int i)",true);
    if(!lightRefs.add||!methodPointer(lightRefs.add)){*error="Reason=ItemLightMethodUnavailable";return false;}
    if(!update){*error="Reason=WorldItemUpdateUnavailable";return false;}
    if(!hook::install(update,hkWorld,&originalWorld)){*error="Reason=WorldItemUpdateHookRejected";return false;}
    worldHooked=true;return true;
}
using MatchFn=int32_t (*)(const void*,uint8_t*,const MethodInfo*);
MatchFn originalMatch=nullptr;bool matchHooked=false;size_t matchPart=0,matchBody=0;
int32_t hkMatch(const void* request,uint8_t* special,const MethodInfo* m) {
    const int32_t result=originalMatch?originalMatch(request,special,m):0;
    if(!request||!special||read<int32_t>(request,matchPart)!=1)return result;
    const auto view=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);if(!view)return result;
    const auto found=view->robes.find(read<int32_t>(request,matchBody));if(found==view->robes.end())return result;
    *special=1;return found->second;
}
bool prepareRobe(std::string* error) {
    if(matchHooked)return true;
    auto& a=api();auto* player=findClass({"Terraria","Player",{}});auto* request=player?findNested(player,"SetMatchRequest"):nullptr;ValueLayout layout;
    if(!resolveValueLayout(request,&layout,error)||layout.size<=16||layout.size>64){*error="Reason=SetMatchRequestAbiMismatch";return false;}
    for(const auto& [name,out]:std::initializer_list<std::pair<const char*,size_t*>>{{"ArmorSlotRequested",&matchPart},{"Body",&matchBody}}) {
        auto* f=findField(request,name);if(!f||!primitive(a.class_from_il2cpp_type(a.field_get_type(f)),"Int32",4)||!layout.offset(a.field_get_offset(f),4,out))return false;
    }
    const auto* m=method(player,"int SetMatch(Player.SetMatchRequest request, ref bool somethingSpecial)",false);
    if(!m||!hook::install(m,hkMatch,&originalMatch))return false;
    matchHooked=true;return true;
}
bool install(Entry& entry,std::string* error) {
    auto& a=api();auto* item=findClass({"Terraria","Item",{}});
    if(!a.object_get_class||!a.class_get_rank||!a.class_get_element_class||!a.gchandle_new||!a.gchandle_get_target||!a.gchandle_free||!a.gc_wbarrier_set_field||
       !scalar(item,"type","Int32",4,&itemType,error))return false;
    if(!entry.policy.clone.empty()&&!entry.clone) {
        auto* ids=findClass({"Terraria.ID","ItemID",{}});
        const auto id=native_static::resolve(ids,entry.policy.clone.c_str());int64_t nativeId=0;
        if(!native_static::readInteger(id,&nativeId)){*error="Reason=VanillaCloneIdUnavailable";return false;}
        if(nativeId<1||nativeId>=6196){*error="Reason=VanillaCloneIdOutOfRange";return false;}
        int value=static_cast<int>(nativeId);
        auto* source=a.object_new(item);if(!source)return false;auto cloneRoot=std::make_shared<Root>(source);if(!cloneRoot->get())return false;
        int variant=0;void* args[]={&value,&variant};
        if(!invoke(method(item,"void .ctor()",true),source,nullptr)||!invoke(method(item,"void SetDefaults(int Type, ItemVariant variant)",true),source,args))return false;
        const char* names[]={"wornArmor","tooltipContext","BestiaryNotes","sentry","DD2Summon","shopSpecialCurrency","expert","expertOnly","questItem","fishingPole","bait","hairDye","makeNPC","dye","paint","paintCoating","tileWand","notAmmo","crit","mech","reuseDelay","melee","magic","ranged","summon","placeStyle","buffTime","buffType","mountType","cartTrack","material","noWet","vanity","mana","channel","manaIncrease","noMelee","noUseGraphic","lifeRegen","shoot","shootSpeed","shootsEveryUse","alpha","ammo","useAmmo","autoReuse","accessory","axe","healMana","potion","color","consumable","createTile","createWall","useSoundPitch","damage","defense","armorPenetration","hammer","healLife","holdStyle","knockBack","maxStack","pick","rare","scale","tileBoost","useStyle","useTime","useAnimation","value","useTurn","buy","uniqueStack","width","height","UseSound"};
        std::vector<Field> cloned;
        for(const char* name:names){if(!findField(item,name))continue;Field f;if(!bindField(item,name,&f,error))return false;cloned.push_back(std::move(f));}
        entry.clone=std::move(cloneRoot);entry.cloned=std::move(cloned);
    }
    if(entry.policy.hair) {
        Field color,hair;auto* player=findClass({"Terraria","Player",{}});
        if(!bindField(item,"color",&color,error,false,nullptr,4)||!color.refs.empty()||!bindField(player,"hairColor",&hair,error,false,nullptr,4)||!hair.refs.empty())return false;
        entry.color=color.offset;entry.hair=hair.offset;entry.localPlayer=method(findClass({"Terraria","Main",{}}),"Player get_LocalPlayer()",false);if(!entry.localPlayer)return false;
    }
    for(const auto& source:entry.policy.equips) {
        const auto key=std::make_pair(entry.source.type,source.kind);auto& equip=equipment[key];
        if(equip.type<0){equip.type=entry.source.type;equip.kind=source.kind;equip.path=entry.source.modDir+"/content/"+source.path+".png";equip.flags=source.flags;}
        if(!installEquipment(equip,error)||!setBasicItemEquipSlot(entry.source.type,source.kind,equip.slot,error))return false;
        entry.slots[source.kind]=equip.slot;
    }
    if(entry.policy.costume.enabled) {
        if(!nativeCostumeRuntimeReady()){*error="Reason=CostumePlayerHooksUnavailable";return false;}
        const auto& c=entry.policy.costume;
        const int buffType=runtime::modBuffTypeByName(entry.source.modId,c.buff);
        if(buffType<0){*error="Reason=CostumeBuffNotRegistered";return false;}
        const char* names[]={"Head","Body","Legs"};
        const auto ownedFlag=[&](const char* group,const char* flag,int slot,bool value,int variant) {
            auto* cls=nested(group);
            auto* sets=cls?findNested(cls,"Sets"):nullptr;
            auto field=staticArray(sets,flag);
            const auto* array=table(field);
            if(!array||!primitive(api().class_get_element_class(field.type),"Boolean",1)||
               !grow(field,slot+1))return false;
            const uint8_t bits=value?1:0;
            return writeOwnedStaticCell(entry.source.type,
                std::string("Costume.")+group+"."+flag+"."+std::to_string(variant),
                field,slot,&bits,1);
        };
        for(int i=0;i<3;++i) {
            const std::string name=names[i];
            const auto key=std::make_pair(entry.source.type,name+"#CostumeAlt");
            auto& equip=equipment[key];
            if(equip.type<0) {
                equip.type=entry.source.type;equip.kind=name;
                equip.path=entry.source.modDir+"/content/"+c.alternate+"_"+name+".png";
            }
            if(!installEquipment(equip,error))return false;
            entry.costumeAltSlots[i]=equip.slot;
            const int normal=entry.slots.at(name);
            for(int variant=0;variant<2;++variant) {
                const int slot=variant?equip.slot:normal;
                const bool ready=i==0
                    ?ownedFlag("Head","DrawHead",slot,false,variant)
                    :i==1
                        ?ownedFlag("Body","HidesTopSkin",slot,true,variant)&&
                         ownedFlag("Body","HidesArms",slot,true,variant)
                        :ownedFlag("Legs","HidesBottomSkin",slot,true,variant);
                if(!ready){*error="Reason=CostumeSkinFlagsUnavailable";return false;}
            }
        }
        entry.costumeBuffType=buffType;
    }
    if(entry.policy.animated&&!registerAnimation(entry,error))return false;
    if(entry.policy.wing){
        if(!writeWingStats(entry,error))return false;
        std::string movementError;
        if(!prepareWings(&movementError)){
            BL_WARN("[CSharp][Wing] Type=%d Class=%s Stage=CustomVerticalMovement Result=VanillaMovementFallback Reason=%s",
                    entry.source.type,entry.source.name.c_str(),movementError.empty()?"NativeHookUnavailable":movementError.c_str());
        }
    }
    if(entry.policy.colored){
        if(!prepareAlpha(error))return false;
        const auto& c=entry.policy.color;entry.nativeAlpha=alphaLayout.pack(c[0],c[1],c[2],c[3]);
    }
    if(entry.policy.lit&&!prepareLight(error))return false;
    if(entry.policy.robe&&!prepareRobe(error))return false;
    return true;
}
// Caller holds queueMutex until all temporary snapshot references are dropped.
void revokeSnapshotTypesLocked(const std::set<int>& types) {
    if(types.empty())return;
    const auto old=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);
    if(old) {
        auto next=std::make_shared<Snapshot>();
        for(const auto& [type,entry]:old->items) {
            if(types.contains(type))continue;
            next->items[type]=entry;
            if(entry->policy.wing)next->wings[entry->slots.at("Wings")]=entry->policy.wings;
            if(entry->policy.robe)next->robes[entry->slots.at("Body")]=entry->slots.at("Legs");
        }
        if(next->items.size()!=old->items.size()) {
            retiringSnapshots.push_back(old);
            std::atomic_store_explicit(&snapshot,std::shared_ptr<const Snapshot>(std::move(next)),std::memory_order_release);
        }
    }
}
void publish() {
    std::lock_guard lock(queueMutex);
    auto next=std::make_shared<Snapshot>();
    for(const auto& [type,entry]:entries) {
        const auto desired=desiredGeneration.find(type);
        if(desired==desiredGeneration.end()||desired->second!=entry->generation)continue;
        if(!entry->status.ready)continue;
        next->items[type]=entry;
        if(entry->policy.wing)next->wings[entry->slots.at("Wings")]=entry->policy.wings;
        if(entry->policy.robe)next->robes[entry->slots.at("Body")]=entry->slots.at("Legs");
    }
    std::atomic_store_explicit(&snapshot,std::shared_ptr<const Snapshot>(std::move(next)),std::memory_order_release);
    pendingCount.store(std::count_if(statuses.begin(),statuses.end(),[](const auto& pair){return !pair.second.ready;}),std::memory_order_release);
}
} // namespace item_native
struct NativeItemPolicyBatch {
    std::vector<std::shared_ptr<item_native::Entry>> entries;
    std::vector<int> replacedTypes;
    std::map<int,std::string> owners;
};
bool prepareNativeItemPolicies(const std::vector<BasicItemEntry>& entries,std::shared_ptr<NativeItemPolicyBatch>* batch,std::string* error) {
    auto prepared=std::make_shared<NativeItemPolicyBatch>();
    for(const auto& source:entries) {
        prepared->replacedTypes.push_back(source.type);
        prepared->owners[source.type]=source.modId;
        if(source.behavior.empty())continue;
        auto entry=std::make_shared<item_native::Entry>();entry->source=source;
        if(!item_policy_wire::read(source,&entry->policy,error))return false;
        entry->status={source.type,source.modId,source.name,"waiting-native-install","",false};prepared->entries.push_back(std::move(entry));
    }
    *batch=std::move(prepared);return true;
}
void commitNativeItemPolicies(const std::shared_ptr<NativeItemPolicyBatch>& batch) {
    if(!batch||batch->replacedTypes.empty())return;
    using namespace item_native;
    std::lock_guard lock(queueMutex);
    const uint64_t generation=++nextGeneration;
    for(int type:batch->replacedTypes) {
        desiredGeneration[type]=generation;
        desiredOwner[type]=batch->owners.at(type);
        staticRetirements[type]=generation;
        staticRetirementOwners[type]=batch->owners.at(type);
        incoming[type]=nullptr; // Empty behavior is an explicit per-type replacement.
        statuses.erase(type);
    }
    for(const auto& prepared:batch->entries) {
        // Prepared batches can be committed repeatedly; installation mutates its own copy.
        auto entry=std::make_shared<Entry>(*prepared);entry->generation=generation;
        incoming[entry->source.type]=entry;statuses[entry->source.type]=entry->status;
    }
    revokeSnapshotTypesLocked({batch->replacedTypes.begin(),batch->replacedTypes.end()});
    pendingCount.store(std::count_if(statuses.begin(),statuses.end(),[](const auto& pair){return !pair.second.ready;}),std::memory_order_release);
}
void retainNativeItemPoliciesForMod(const std::string& mod,const std::vector<int>& activeTypes) {
    using namespace item_native;
    const std::set<int> retained(activeTypes.begin(),activeTypes.end());
    std::lock_guard lock(queueMutex);
    std::set<int> removed;
    for(auto it=desiredOwner.begin();it!=desiredOwner.end();) {
        if(it->second!=mod||retained.contains(it->first)){++it;continue;}
        const int type=it->first;
        removed.insert(type);
        // entries and their roots belong to the game thread. Ingress only
        // invalidates the generation and queues removal, including pending work.
        desiredGeneration.erase(type);
        staticRetirements[type]=++nextGeneration;
        staticRetirementOwners[type]=mod;
        incoming[type]=nullptr;
        statuses.erase(type);
        it=desiredOwner.erase(it);
    }
    revokeSnapshotTypesLocked(removed);
    pendingCount.store(std::count_if(statuses.begin(),statuses.end(),[](const auto& pair){return !pair.second.ready;}),std::memory_order_release);
}
void retireNativeItemPoliciesForMod(const std::string& mod) {
    retainNativeItemPoliciesForMod(mod,{});
}
bool applyNativeItemPolicyDefaults(Il2CppObject* item,int type) {
    using namespace item_native;const auto view=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);if(!item||!view)return false;
    const auto found=view->items.find(type);if(found==view->items.end())return false;const auto& entry=*found->second;
    if(entry.clone)if(auto* source=entry.clone->get())for(const auto& f:entry.cloned) {
        auto* target=reinterpret_cast<char*>(item)+f.offset;const auto* bytes=reinterpret_cast<const char*>(source)+f.offset;
        std::memcpy(target,bytes,f.bytes);
        for(auto offset:f.refs){auto* value=read<Il2CppObject*>(bytes,offset);api().gc_wbarrier_set_field(item,reinterpret_cast<void**>(target+offset),value);}
    }
    if(entry.policy.hair){Il2CppObject* player=nullptr;if(invoke(entry.localPlayer,nullptr,nullptr,&player)&&player)std::memcpy(reinterpret_cast<char*>(item)+entry.color,reinterpret_cast<char*>(player)+entry.hair,4);}
    return bool(entry.clone);
}
bool nativeCostumeEffect(int itemType,NativeCostumeEffect* effect) {
    if(!effect)return false;
    const auto view=std::atomic_load_explicit(&item_native::snapshot,std::memory_order_acquire);
    if(!view)return false;
    const auto found=view->items.find(itemType);
    if(found==view->items.end())return false;
    const auto& entry=*found->second;
    if(!entry.policy.costume.enabled||entry.costumeBuffType<0)return false;
    effect->buffType=entry.costumeBuffType;
    const char* names[]={"Head","Body","Legs"};
    for(int i=0;i<3;++i) {
        const auto normal=entry.slots.find(names[i]);
        if(normal==entry.slots.end()||entry.costumeAltSlots[i]<0)return false;
        effect->normal[i]=normal->second;
        effect->alternate[i]=entry.costumeAltSlots[i];
    }
    effect->jumpSpeedBoost=entry.policy.costume.jump;
    effect->extraFall=entry.policy.costume.fall;
    effect->requiredTownNpcs=entry.policy.costume.town;
    return true;
}
bool nativeItemPolicyReady(int type) {std::lock_guard lock(item_native::queueMutex);if(item_native::staticRetirements.contains(type))return false;const auto it=item_native::statuses.find(type);return it==item_native::statuses.end()||it->second.ready;}
size_t pendingNativeItemPolicies(){std::lock_guard lock(item_native::queueMutex);return item_native::pendingCount.load(std::memory_order_acquire)+item_native::staticRetirements.size();}
std::vector<NativeItemPolicyStatus> nativeItemPolicyStatus() {
    using namespace item_native;std::lock_guard lock(queueMutex);
    std::vector<NativeItemPolicyStatus> out;
    for(const auto& [type,status]:statuses) {
        out.push_back(status);
        if(staticRetirements.contains(type)) {
            out.back().ready=false;out.back().phase="waiting-static-retirement";
            out.back().error="Reason=NativeStaticCellRestorePending";
        }
    }
    for(const auto& [type,revision]:staticRetirements)if(!statuses.contains(type)) {
        (void)revision;out.push_back({type,staticRetirementOwners.at(type),"",
            "waiting-static-retirement","Reason=NativeStaticCellRestorePending",false});
    }
    return out;
}
void pumpNativeItemPoliciesOnGameThread() {
    using namespace item_native;static unsigned tick=0;static int cursor=-1;++tick;
    pumpStaticRetirements();
    bool changed=false;
    std::vector<std::shared_ptr<const Snapshot>> retired; // destroyed on this game thread
    {
        std::lock_guard lock(queueMutex);
        changed=!incoming.empty();
        for(auto& [type,entry]:incoming) {
            if(entry)entries[type]=std::move(entry);
            else entries.erase(type);
        }
        incoming.clear();retired.swap(retiringSnapshots);
    }
    if(pendingCount.load(std::memory_order_acquire)) {
        auto it=entries.upper_bound(cursor);size_t attempted=0;
        for(size_t scanned=0;scanned<entries.size()&&attempted<8;++scanned) {
            if(it==entries.end())it=entries.begin();
            auto& [type,entry]=*it++;
            if(entry->status.ready||tick<entry->nextRetryTick||pendingStaticRetirement(type))continue;
            ++attempted;cursor=type;std::string error;
            const std::string previousError=entry->status.error;
            entry->status.ready=install(*entry,&error);
            if(!entry->status.ready&&error.empty())error="Reason=NativeMethodOrTableUnavailable";
            entry->status.phase=entry->status.ready?"ready":"waiting-native-install";entry->status.error=error;
            bool current=false;
            {std::lock_guard lock(queueMutex);
                const auto desired=desiredGeneration.find(type);
                current=desired!=desiredGeneration.end()&&desired->second==entry->generation;
                if(current)statuses[type]=entry->status;
            }
            if(!current){changed=true;continue;}
            if(!entry->status.ready&&(entry->failedAttempts==0||previousError!=error))
                BL_WARN("[CSharp][NativePolicy] Type=%d Class=%s Result=Pending Detail=%s",
                        type,entry->source.name.c_str(),error.c_str());
            if(entry->status.ready){
                acknowledgeBasicItemBehavior(type,"",entry->source.defaultsGeneration);
                requestBasicItemInstanceRefresh("NativePolicyReady");
                changed=true;
            } else if(++entry->failedAttempts>3&&tick>120)entry->nextRetryTick=tick+15;
        }
    }
    // Legacy BunnyAccessory Shield/Shoes use exactly the same native installer.
    // This adapter consumes C# descriptors directly, without EquipLoader JS.
    if(pendingManagedNativeEquipTextures()&&(tick<60||tick%15u==0))for(const auto& source:managedNativeEquipTextures()) {
        const auto kind=source.visualMode==1?"Shield":"Shoes";auto& equip=equipment[{source.itemType,kind}];
        if(equip.type<0) {
            const auto sep=source.texture.find(':');if(sep==std::string::npos)continue;
            const auto relative=source.texture.substr(sep+1);
            if(!relative.starts_with("Content/")||relative.find("..")!=std::string::npos||relative.find('\\')!=std::string::npos)continue;
            equip.type=source.itemType;equip.kind=kind;equip.path=mods::rootOf(source.texture.substr(0,sep))+"/content/"+relative;
        }
        std::string error;const bool ready=installEquipment(equip,&error)&&setManagedNativeEquipSlot(source.itemType,source.visualMode,equip.slot);
        NativeItemPolicyStatus status{source.itemType,source.texture.substr(0,source.texture.find(':')),kind,ready?"ready":"waiting-native-install",error,ready};
        {std::lock_guard lock(queueMutex);statuses[source.itemType]=std::move(status);}
    }
    if(changed)publish();
    for(auto& [name,kind]:kinds){(void)name;if(kind.sets)kind.sets->checkPending(kind.total);}
    if(tick%120u==0) {
        for(auto& [name,kind]:kinds){(void)name;if(kind.sets)kind.sets->watch(kind.total,nullptr);}
        static std::pair<int,std::string> last;
        auto it=equipment.upper_bound(last);
        for(size_t n=0;n<std::min(size_t{8},equipment.size());++n) {
            if(it==equipment.end())it=equipment.begin();
            auto& [key,equip]=*it++;last=key;std::string error;installEquipment(equip,&error);
        }
    }
}
} // namespace bl::csharp
