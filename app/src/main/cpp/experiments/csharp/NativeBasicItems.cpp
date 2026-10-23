#include "il2cpp/GuardedCall.h"
#include "experiments/csharp/NativeDefaults.h"
#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeRarityGameplay.h"
#include "experiments/csharp/NativeArmorPolicies.h"
#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "content/items/ModItems.h"
#include "content/tiles/ModTiles.h"
#include "content/walls/ModWalls.h"
#include "content/recipes/NativeRecipes.h"
#include "content/common/GameRefs.h"
#include "experiments/csharp/NativeContent.h"
#include "il2cpp/Signature.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include "core/Log.h"
#include "hook/HookManager.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <memory>

namespace bl::csharp {
namespace {
std::mutex mutex;
std::map<int,BasicItemEntry> definitions;
std::map<std::string,PendingBasicRecipe> recipes;
std::string lastRecipeKey;
int lastBehaviorType=-1;
std::atomic<bool> recipesPending{false};
std::atomic<bool> instancesPending{false};
std::atomic<const char*> refreshReason{"Unspecified"};
std::atomic<bool> behaviorsPending{false};
std::atomic<bool> placementsPending{false};
std::atomic<unsigned> contentGeneration{0};
struct IntegerField { int offset=-1; int bytes=0; int64_t min=0,max=0; };
std::array<IntegerField,BasicFieldCount-1> integerFields{};
struct EquipSlot { IntegerField field; int value=-1; };
std::map<int,std::map<std::string,EquipSlot>> equipSlots;
std::map<int,EquipSlot> defenseDefaults;
std::array<IntegerField,2> placementFields{};
bool placementPrepared=false;
IntegerField wallPlacementField{};
bool wallPlacementPrepared=false;
const MethodInfo* wallDefaultMethod=nullptr;
std::map<int,std::string> placementErrors;
int lastPlacementType=-1;

const char* integerNames[]={"width","height","maxStack","value","rare","damage","crit",
    "useTime","useAnimation","useStyle","pick","axe","hammer"};

bool systemPrimitive(Il2CppClass* cls,const char* name) {
    auto& api=il2cpp::api();
    if(!cls||!api.class_get_namespace)return false;
    const char* ns=api.class_get_namespace(cls),*actual=api.class_get_name(cls);
    return ns&&actual&&std::strcmp(ns,"System")==0&&std::strcmp(actual,name)==0&&api.class_is_valuetype(cls);
}
bool integerContract(Il2CppClass* cls,IntegerField* out) {
    auto& api=il2cpp::api();
    if(cls&&api.class_is_enum&&api.class_is_enum(cls)) {
        if(!api.class_enum_basetype)return false;
        const auto* base=api.class_enum_basetype(cls);
        cls=base?api.class_from_il2cpp_type(base):nullptr;
    }
    if(!cls||!api.class_get_namespace||(!api.class_get_namespace(cls)||std::strcmp(api.class_get_namespace(cls),"System")!=0))return false;
    const char* name=api.class_get_name(cls);
    if(!name)return false;
    struct Kind { const char* name; int bytes; int64_t min,max; };
    const Kind kinds[]={
        {"SByte",1,-128,127},{"Byte",1,0,255},
        {"Int16",2,-32768,32767},{"UInt16",2,0,65535},
        {"Int32",4,INT32_MIN,INT32_MAX},{"UInt32",4,0,UINT32_MAX}};
    for(const auto& kind:kinds)if(std::strcmp(name,kind.name)==0) {
        uint32_t alignment=0;
        if(!api.class_is_valuetype(cls)||api.class_value_size(cls,&alignment)!=kind.bytes)return false;
        out->bytes=kind.bytes;out->min=kind.min;out->max=kind.max;return true;
    }
    return false;
}
bool fitIntegerFields(const BasicItemEntry& entry,std::string* error) {
    for(size_t i=0;i<integerFields.size();++i) {
        const auto& field=integerFields[i];const int64_t value=entry.values[i];
        if(value<field.min||value>field.max) {
            *error=entry.name+": "+integerNames[i]+"="+std::to_string(value)+
                " fora do tipo nativo ("+std::to_string(field.bytes*8)+" bits)";return false;
        }
    }
    return true;
}
std::array<int,7> flagOffsets{};
int knockBackOffset=-1,soundOffset=-1;
int soundSize=0,soundValueOffset=0,soundHasValueOffset=-1;
Il2CppClass* soundClass=nullptr;
std::vector<size_t> soundReferences;
std::map<std::string,std::vector<unsigned char>> soundCache;
bool prepared=false;

bool instanceBounds(Il2CppClass* item,int offset,size_t width,const char* field,std::string* error) {
    auto& api=il2cpp::api();const auto size=api.class_instance_size?api.class_instance_size(item):0;
    if(offset>=static_cast<int>(sizeof(Il2CppObject))&&width<=size&&static_cast<size_t>(offset)<=size-width)return true;
    *error=std::string("[CSharp][ERROR][NativeItem] Stage=BindFields Field=")+field+
        " Expected=FieldWithinInstance Actual=Offset:"+std::to_string(offset)+",Width:"+std::to_string(width)+
        ",InstanceSize:"+std::to_string(size)+" Result=Rejected Reason=InvalidFieldBounds";return false;
}

using FunctionalEquipFn=void (*)(Il2CppObject*,int32_t,Il2CppObject*,const MethodInfo*);
FunctionalEquipFn originalFunctionalEquip=nullptr;
int equipItemTypeOffset=-1;
bool equipEffectsHooked=false;
void hkBasicFunctionalEquip(Il2CppObject* player,int32_t slot,Il2CppObject* item,const MethodInfo* method) {
    if(originalFunctionalEquip)originalFunctionalEquip(player,slot,item,method);
    if(!player||!item)return;
    const auto* bytes=reinterpret_cast<const unsigned char*>(item);
    if(!bytes[flagOffsets[5]])return;
    int type=0;std::memcpy(&type,bytes+equipItemTypeOffset,sizeof(type));
    applyBasicItemAccessoryEffects(player,type);
}
bool prepareBasicAccessoryHook(std::string* error) {
    if(equipEffectsHooked)return true;
    auto& api=il2cpp::api();auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    auto* field=item?il2cpp::findField(item,"type"):nullptr;
    auto* type=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
    uint32_t align=0;
    if(!field||(api.field_get_flags(field)&0x10u)||!systemPrimitive(type,"Int32")||api.class_value_size(type,&align)!=4) {
        *error="Stage=NativeEquip Field=type Reason=NativeTypeMismatch";return false;
    }
    const auto offset=api.field_get_offset(field);
    equipItemTypeOffset=offset<=INT32_MAX?static_cast<int>(offset):-1;
    if(!instanceBounds(item,equipItemTypeOffset,4,"type",error))return false;
    const auto* method=player?il2cpp::findMethodBySignature(player,
        il2cpp::parseSignature("void ApplyEquipFunctional(int itemSlot, Item currentItem)")):nullptr;
    if(!method||!hook::install(method,hkBasicFunctionalEquip,&originalFunctionalEquip)) {
        *error="Stage=NativeEquip Reason=FunctionalHookUnavailable";return false;
    }
    equipEffectsHooked=true;return true;
}

bool preparePlacement(std::string* error) {
    if(placementPrepared)return true;
    auto& a=il2cpp::api();auto* cls=il2cpp::findClass({"Terraria","Item",{}});
    std::array<IntegerField,2> resolved{};const char* names[]={"createTile","placeStyle"};
    for(size_t i=0;i<resolved.size();++i) {
        auto* f=cls?il2cpp::findField(cls,names[i]):nullptr;
        auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;
        if(!f||(a.field_get_flags(f)&0x10u)||!integerContract(type,&resolved[i])||
           (i==0&&resolved[i].min> -1)) {
            *error=std::string("[CSharp][ERROR][NativeItem] API=DefaultToPlaceableTile Stage=BindPlacement Field=")+names[i]+
                " Expected=InstanceIntegral"+(i==0?std::string("WithMinusOne"):std::string{})+
                " Actual="+(type?a.class_get_name(type):"Missing")+" Result=Rejected Reason=NativeTypeMismatch";return false;
        }
        const auto offset=a.field_get_offset(f);
        resolved[i].offset=offset<=INT32_MAX?static_cast<int>(offset):-1;
        if(!instanceBounds(cls,resolved[i].offset,resolved[i].bytes,names[i],error))return false;
    }
    placementFields=resolved;placementPrepared=true;return true;
}

bool prepareWallPlacement(std::string* error) {
    if(wallPlacementPrepared)return true;
    auto& a=il2cpp::api();auto* cls=il2cpp::findClass({"Terraria","Item",{}});
    auto* field=cls?il2cpp::findField(cls,"createWall"):nullptr;
    auto* type=field?a.class_from_il2cpp_type(a.field_get_type(field)):nullptr;
    IntegerField resolved{};
    if(!field||(a.field_get_flags(field)&0x10u)||!integerContract(type,&resolved)) {
        *error="API=DefaultToPlaceableWall Stage=BindPlacement Field=createWall Expected=InstanceIntegral Actual="+
            std::string(type?a.class_get_name(type):"Missing")+" Reason=NativeTypeOrStorageMismatch";return false;
    }
    const auto offset=a.field_get_offset(field);resolved.offset=offset<=INT32_MAX?static_cast<int>(offset):-1;
    if(!instanceBounds(cls,resolved.offset,resolved.bytes,"createWall",error))return false;
    const auto* helper=il2cpp::findMethodBySignature(cls,il2cpp::parseSignature("void DefaultToPlaceableWall(ushort wallToPlace)"));
    if(!helper||!a.method_is_instance||!a.method_is_instance(helper)||!a.runtime_invoke||!il2cpp::methodPointer(helper)) {
        *error="API=DefaultToPlaceableWall Stage=BindPlacement Expected=Instance:void(ushort) Actual=MissingOrIncompatible Reason=NativeMethodContractMismatch";return false;
    }
    wallDefaultMethod=helper;wallPlacementField=resolved;wallPlacementPrepared=true;return true;
}

struct NativeAccessoryWrite { int offset=-1; float value=0; uint8_t kind=0; };
using NativeAccessoryTable=std::map<int,std::vector<NativeAccessoryWrite>>;
std::shared_ptr<const NativeAccessoryTable> nativeAccessoryTable;
std::atomic<bool> nativeAccessoryReady{false};
bool resolveBasicAccessoryPolicy(const BasicItemEntry& item,std::vector<NativeAccessoryWrite>* writes,std::string* error) {
    if(item.accessoryEffects.empty())return true;
    auto& a=il2cpp::api();auto* player=il2cpp::findClass({"Terraria","Player",{}});
    if(!player||!a.field_get_type||!a.class_from_il2cpp_type||!a.class_value_size||!a.class_instance_size) {
        *error="Terraria.Player ou metadados IL2CPP indisponiveis";return false;
    }
    for(const auto& effect:item.accessoryEffects) {
        auto* field=il2cpp::findField(player,effect.field);
        auto* type=field?a.class_from_il2cpp_type(a.field_get_type(field)):nullptr;
        const bool flag=effect.field=="strongBees";
        const char* expected=flag?"Boolean":"Single";
        uint32_t align=0;
        if(!field||!type||(a.field_get_flags(field)&0x10u)||!systemPrimitive(type,expected)||
            a.class_value_size(type,&align)!=(flag?1:4)) {
            *error="Stage=NativeEquip Field="+effect.field+" Expected="+expected+" Reason=NativeTypeMismatch";return false;
        }
        const size_t offset=a.field_get_offset(field);
        if(offset>INT32_MAX||!instanceBounds(player,static_cast<int>(offset),flag?1u:4u,
                effect.field.c_str(),error))return false;
        writes->push_back({static_cast<int>(offset),effect.value,
            static_cast<uint8_t>(flag?2:effect.operation=="mul"?1:0)});
    }
    return true;
}

void writeIntegral(Il2CppObject* object,const IntegerField& field,int value) {
    auto* destination=reinterpret_cast<unsigned char*>(object)+field.offset;
    if(field.bytes==1){const auto v=static_cast<uint8_t>(value);std::memcpy(destination,&v,1);}
    else if(field.bytes==2){const auto v=static_cast<uint16_t>(value);std::memcpy(destination,&v,2);}
    else {const auto v=static_cast<uint32_t>(value);std::memcpy(destination,&v,4);}
}
// Only on the game thread: IDs must exist in the installed native tile tables.
void resolvePlacements() {
    std::lock_guard lock(mutex);unsigned processed=0;
    auto it=definitions.upper_bound(lastPlacementType);
    if(it==definitions.end())it=definitions.begin();
    for(size_t scanned=0;scanned<definitions.size()&&processed<8;++scanned) {
        if(it==definitions.end())it=definitions.begin();
        auto& entry=it->second;lastPlacementType=it->first;++it;
        if(!entry.placeable||entry.placeTileKey.empty()||entry.placeTile>=0)continue;
        ++processed;const int target=entry.wallPlaceable?runtime::modWallTypeByKey(entry.placeTileKey):runtime::modTileTypeByKey(entry.placeTileKey);
        const auto limit=entry.wallPlaceable?wallPlacementField.max:placementFields[0].max;
        std::string reason=target<0?"MissingDependency":target>=(entry.wallPlaceable?runtime::wallTypeCount():runtime::tileTypeCount())?"DependencyNotInstalled":
            target>std::min<int64_t>(limit,entry.wallPlaceable?65535:INT32_MAX)?"NativeRangeExceeded":
            entry.wallPlaceable&&!runtime::modWallAssetReady(target)?"NativeWallAssetUnavailable":"";
        if(!reason.empty()) {
            auto message="[CSharp]["+std::string(reason=="NativeRangeExceeded"?"ERROR":"WARN")+
                "][Placement] Mod="+entry.modId+" Class="+entry.name+" File="+entry.sourceFile+
                " API=DefaultToPlaceable"+std::string(entry.wallPlaceable?"Wall":"Tile")+" ContentId="+std::to_string(entry.type)+" Key="+entry.placeTileKey+
                " Stage=ResolvePlacement Expected=InstalledContentWithinNativeRange Actual="+std::to_string(target)+
                " Result=PlacementDisabled Reason="+reason;
            if(placementErrors[entry.type]!=message){placementErrors[entry.type]=message;
                if(reason=="NativeRangeExceeded")BL_ERROR("%s",message.c_str());else BL_DEBUG("%s",message.c_str());}
            continue;
        }
        entry.placeTile=target;placementErrors.erase(entry.type);
        refreshReason.store("NativePlacementReady",std::memory_order_release);
        instancesPending.store(true,std::memory_order_release);
        BL_INFO("[CSharp][INFO][Placement Mod=%s Class=%s Tile=%s ContentId=%d TileId=%d Stage=ResolveTile Result=Ready",
            entry.modId.c_str(),entry.name.c_str(),entry.placeTileKey.c_str(),entry.type,target);
    }
    bool pending=false;
    for(const auto& [id,entry]:definitions)
        if(entry.placeable&&!entry.placeTileKey.empty()&&entry.placeTile<0) {
            pending=true;
            break;
        }
    placementsPending.store(pending,std::memory_order_release);
}

bool prepare(std::string* error) {
    if(prepared)return true;
    auto& api=il2cpp::api();
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    if(!item||!api.field_get_type||!api.class_from_il2cpp_type||!api.class_get_name||
       !api.class_get_namespace||!api.class_get_fields||!api.field_get_flags||!api.field_get_offset||
       !api.class_value_size||!api.class_is_valuetype||!api.class_instance_size||
       !api.runtime_class_init||!api.field_static_get_value) {
        std::string missing=item?"":"Terraria.Item ";
        for(const auto& entry:std::initializer_list<std::pair<const char*,bool>>{
            {"field_get_type",!!api.field_get_type},{"class_from_il2cpp_type",!!api.class_from_il2cpp_type},
            {"class_get_name",!!api.class_get_name},{"class_get_namespace",!!api.class_get_namespace},
            {"class_get_fields",!!api.class_get_fields},{"field_get_flags",!!api.field_get_flags},
            {"field_get_offset",!!api.field_get_offset},{"class_value_size",!!api.class_value_size},
            {"class_is_valuetype",!!api.class_is_valuetype},{"class_instance_size",!!api.class_instance_size},
            {"runtime_class_init",!!api.runtime_class_init},{"field_static_get_value",!!api.field_static_get_value}})
            if(!entry.second)missing+=std::string(entry.first)+" ";
        *error="[CSharp][ERROR][NativeBridge] API=BunnyItem Stage=ResolveContract Expected=Terraria.ItemAndRequiredIL2CPPExports Actual=Missing:"+
            missing+" Result=Rejected Reason=NativeContractUnavailable";return false;
    }
    soundSize=0;soundValueOffset=0;soundHasValueOffset=-1;soundClass=nullptr;soundReferences.clear();
    for(size_t i=0;i<integerFields.size();++i) {
        auto* field=il2cpp::findField(item,integerNames[i]);
        auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
        auto& binding=integerFields[i];
        if(!field||(api.field_get_flags(field)&0x10u)||!integerContract(cls,&binding)) {
            const char* actual=cls?api.class_get_name(cls):nullptr;
            *error=std::string("[CSharp][ERROR][NativeItem] Stage=BindFields Field=")+integerNames[i]+
                " Expected=Integral8/16/32 Actual="+(actual?actual:"Missing")+" Result=Rejected Reason=NativeTypeMismatch";return false;
        }
        binding.offset=il2cpp::fieldOffset(item,integerNames[i]);
        if(!instanceBounds(item,binding.offset,static_cast<size_t>(binding.bytes),integerNames[i],error))return false;
        BL_DEBUG("csharp item field: %s tipo=%s bytes=%d offset=%d",integerNames[i],
            api.class_get_name(cls),binding.bytes,binding.offset);
    }
    const char* flags[]={"melee","noMelee","autoReuse","useTurn","consumable","accessory","vanity"};
    for(size_t i=0;i<flagOffsets.size();++i) {
        auto* field=il2cpp::findField(item,flags[i]);
        auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
        if(!field||!systemPrimitive(cls,"Boolean")||(api.field_get_flags(field)&0x10u)) {*error=std::string("[CSharp][ERROR][NativeItem] Stage=BindFields Field=")+flags[i]+" Expected=System.Boolean/1Byte Actual="+(cls?api.class_get_name(cls):"Missing")+" Result=Rejected Reason=NativeTypeMismatch";return false;}
        uint32_t alignment=0;
        flagOffsets[i]=il2cpp::fieldOffset(item,flags[i]);
        if(flagOffsets[i]<static_cast<int>(sizeof(Il2CppObject))||api.class_value_size(cls,&alignment)!=1) {
            *error=std::string("[CSharp][ERROR][NativeItem] Stage=BindFields Field=")+flags[i]+" Expected=Boolean/1Byte Result=Rejected Reason=InvalidLayout";return false;
        }
        if(!instanceBounds(item,flagOffsets[i],1,flags[i],error))return false;
    }
    auto* knockField=il2cpp::findField(item,"knockBack");
    auto* knockClass=knockField?api.class_from_il2cpp_type(api.field_get_type(knockField)):nullptr;
    uint32_t knockAlign=0;
    if(!systemPrimitive(knockClass,"Single")||(api.field_get_flags(knockField)&0x10u)||api.class_value_size(knockClass,&knockAlign)!=4) {
        *error="[CSharp][ERROR][NativeItem] Stage=BindFields Field=knockBack Expected=Single/4Bytes Result=Rejected Reason=NativeTypeMismatch";return false;
    }
    knockBackOffset=il2cpp::fieldOffset(item,"knockBack");
    if(!instanceBounds(item,knockBackOffset,4,"knockBack",error))return false;
    auto* field=il2cpp::findField(item,"UseSound");
    auto* nullable=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
    if(!nullable||!field||(api.field_get_flags(field)&0x10u)) {
        *error="[CSharp][ERROR][NativeItem] Stage=BindFields Field=UseSound Expected=InstanceSoundField Actual="+
            std::string(!field?"Missing":api.field_get_flags(field)&0x10u?"Static":"UnknownType")+
            " Result=Rejected Reason=NativeTypeMismatch";return false;
    }
    soundOffset=il2cpp::fieldOffset(item,"UseSound");
    if(soundOffset<0) {*error="BunnyItem: offset UseSound indisponivel";return false;}
    if(!api.class_is_valuetype(nullable)) {
        // Terraria Mobile usa LegacySoundStyle como referencia. O caminho
        // de Nullable abaixo cobre hosts que usam SoundStyle como struct.
        soundClass=nullable;soundSize=sizeof(void*);soundReferences={0};
        if(!instanceBounds(item,soundOffset,sizeof(void*),"UseSound",error))return false;
        prepared=true;return true;
    }
    uint32_t align=0;soundSize=api.class_value_size(nullable,&align);
    if(soundSize<1||soundSize>512||soundOffset<0) {*error="BunnyItem: layout UseSound invalido";return false;}
    if(!instanceBounds(item,soundOffset,static_cast<size_t>(soundSize),"UseSound",error))return false;
    il2cpp::ValueLayout nullableLayout;if(!il2cpp::resolveValueLayout(nullable,&nullableLayout,error))return false;
    // SoundStyle? tem hasValue e value; resolver pelo TIPO evita offsets fixos
    // dependentes da build Unity/IL2CPP ou do nome privado do campo.
    void* iter=nullptr;
    while(auto* f=api.class_get_fields(nullable,&iter)) {
        if(api.field_get_flags(f)&0x10u)continue;
        auto* cls=api.class_from_il2cpp_type(api.field_get_type(f));
        uint32_t fieldAlign=0;size_t offset=0;
        const auto width=cls&&api.class_is_valuetype(cls)?api.class_value_size(cls,&fieldAlign):static_cast<int>(sizeof(void*));
        if(width<=0||!nullableLayout.offset(api.field_get_offset(f),static_cast<size_t>(width),&offset)) {
            *error="[CSharp][ERROR][NativeItem] Field=UseSound Stage=BindNullable Expected=FieldWithinValue Actual=Offset:"+
                std::to_string(api.field_get_offset(f))+",Width:"+std::to_string(width)+" Result=Rejected Reason=InvalidValueFieldBounds";return false;
        }
        if(cls&&std::strcmp(api.class_get_name(cls),"Boolean")==0) soundHasValueOffset=static_cast<int>(offset);
        else if(cls&&api.class_is_valuetype(cls)) {soundClass=cls;soundValueOffset=static_cast<int>(offset);}
    }
    if(!soundClass||soundHasValueOffset<0||soundHasValueOffset>=soundSize) {*error="BunnyItem: Nullable<SoundStyle> nao reconhecido";return false;}
    const int valueSize=api.class_value_size(soundClass,&align);
    if(valueSize<1||soundValueOffset<0||soundValueOffset+valueSize>soundSize) {*error="BunnyItem: tamanho SoundStyle incompativel";return false;}
    if(!il2cpp::valueReferenceOffsets(soundClass,static_cast<size_t>(soundValueOffset),&soundReferences,error))return false;
    prepared=true;return true;
}

bool cacheSound(const std::string& name,std::string* error) {
    if(name.empty()||soundCache.contains(name))return true;
    auto& api=il2cpp::api();auto* id=il2cpp::findClass({"Terraria.ID","SoundID",{}});
    auto* field=id?il2cpp::findField(id,name):nullptr;
    if(!field||api.class_from_il2cpp_type(api.field_get_type(field))!=soundClass) {*error="BunnyItem: SoundID."+name+" ausente ou tipo incompativel";return false;}
    api.runtime_class_init(id);
    std::vector<unsigned char> data(static_cast<size_t>(soundSize),0);
    if(soundHasValueOffset>=0)data[static_cast<size_t>(soundHasValueOffset)]=1;
    api.field_static_get_value(field,data.data()+soundValueOffset);
    soundCache.emplace(name,std::move(data));return true;
}

void refreshLiveBasicItems() {
    auto& api=il2cpp::api();auto* main=il2cpp::findClass({"Terraria","Main",{}});
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    if(!main||!item||!player)return;
    const int typeOffset=il2cpp::fieldOffset(item,"type");
    const int prefixOffset=il2cpp::fieldOffset(item,"prefix");
    auto* itemActiveField=il2cpp::findField(item,"active");
    const int activeItem=itemActiveField?static_cast<int>(api.field_get_offset(itemActiveField)):-1;
    const int activePlayer=il2cpp::fieldOffset(player,"active");
    const MethodInfo* prefix=il2cpp::findMethodBySignature(item,il2cpp::parseSignature("bool Prefix(int pre, bool topTier)"));
    if(typeOffset<0||prefixOffset<0)return;
    int count=0;
    auto refresh=[&](Il2CppObject* object) {
        if(!object)return;
        const int type=runtime::field<int32_t>(object,typeOffset);
        if(!isBasicItem(type))return;
        const int savedPrefix=runtime::field<uint8_t>(object,prefixOffset);
        if(!applyBasicItemToObject(object,type))return;
        runtime::finishModItem(object,type); // inferir Width/Height=0 pela textura
        if(savedPrefix>0) {
            runtime::field<uint8_t>(object,prefixOffset)=0;
            Il2CppObject* exception=nullptr;bool topTier=false;int pre=savedPrefix;
            void* args[]={&pre,&topTier};
            auto* result=prefix&&api.runtime_invoke?api.runtime_invoke(prefix,object,args,&exception):nullptr;
            bool applied=result&&!exception&&*reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(result)+sizeof(Il2CppObject));
            if(!applied && managedPrefixDescriptorReady(savedPrefix)) {
                runtime::field<uint8_t>(object,prefixOffset)=0;
                if(applyBasicItemToObject(object,type)) {
                    runtime::finishModItem(object,type);
                    std::string why;
                    applied=applyManagedPrefixStatFallback(object,savedPrefix,&why);
                    if(!applied)
                        BL_WARN("csharp item: prefixo=%d tipo=%d nao reaplicado motivo=%s",
                                savedPrefix,type,why.c_str());
                }
            } else if(!applied && !managedPrefixDescriptorPending(savedPrefix))
                BL_WARN("csharp item: reidratacao type=%d prefix=%d nao reaplicado",type,savedPrefix);
            if(!applied)
                runtime::field<uint8_t>(object,prefixOffset)=static_cast<uint8_t>(savedPrefix);
        }
        ++count;
    };
    auto array=[&](Il2CppArray* objects,bool onlyActive) {
        if(!objects||objects->length>10000)return;
        auto** values=static_cast<Il2CppObject**>(arrayData(objects));
        for(uintptr_t i=0;i<objects->length;++i)if(values[i]&&(!onlyActive||activeItem<0||runtime::field<uint8_t>(values[i],activeItem)))refresh(values[i]);
    };
    auto getStatic=[&](const char* name)->void* {
        void* value=nullptr;if(auto* field=il2cpp::findField(main,name))api.field_static_get_value(field,&value);return value;
    };
    auto* players=static_cast<Il2CppArray*>(getStatic("player"));
    if(players&&players->length<=256) {
        auto** objects=static_cast<Il2CppObject**>(arrayData(players));
        for(uintptr_t i=0;i<players->length;++i)if(objects[i]&&(activePlayer<0||runtime::field<uint8_t>(objects[i],activePlayer))) {
            for(const char* fieldName:{"inventory","armor","dye","miscEquips","miscDyes"}) {
                const int offset=il2cpp::fieldOffset(player,fieldName);
                if(offset>=0)array(runtime::field<Il2CppArray*>(objects[i],offset),false);
            }
            const int loadoutsOffset=il2cpp::fieldOffset(player,"Loadouts");
            auto* loadoutClass=il2cpp::findClass({"Terraria","EquipmentLoadout",{}});
            if(loadoutsOffset>=0&&loadoutClass) {
                auto* loadouts=runtime::field<Il2CppArray*>(objects[i],loadoutsOffset);
                const int armorOffset=il2cpp::fieldOffset(loadoutClass,"Armor");
                const int dyeOffset=il2cpp::fieldOffset(loadoutClass,"Dye");
                if(loadouts&&loadouts->length<=8) {
                    auto** sets=static_cast<Il2CppObject**>(arrayData(loadouts));
                    for(uintptr_t set=0;set<loadouts->length;++set)if(sets[set]) {
                        if(armorOffset>=0)
                            array(runtime::field<Il2CppArray*>(sets[set],armorOffset),false);
                        if(dyeOffset>=0)
                            array(runtime::field<Il2CppArray*>(sets[set],dyeOffset),false);
                    }
                }
            }
        }
    }
    array(static_cast<Il2CppArray*>(getStatic("item")),true);
    refresh(static_cast<Il2CppObject*>(getStatic("mouseItem")));
    refresh(static_cast<Il2CppObject*>(getStatic("HoverItem")));
    refresh(static_cast<Il2CppObject*>(getStatic("tooltipPrefixComparisonItem")));
    int samples=0, missing=0;
    const auto types=basicItemTypes();
    auto* content=il2cpp::findClass({"Terraria.ID","ContentSamples",{}});
    auto* table=content?il2cpp::findField(content,"ItemsByType"):nullptr;
    Il2CppObject* dictionary=nullptr;
    if(table)api.field_static_get_value(table,&dictionary);
    const auto* getter=dictionary?
        api.class_get_method_from_name(api.object_get_class(dictionary),"get_Item",1):nullptr;
    if(getter)for(int type:types) {
        void* args[]={&type};
        Il2CppObject* exception=nullptr;
        Il2CppObject* sample=api.runtime_invoke(getter,dictionary,args,&exception);
        if(!exception&&sample&&runtime::field<int32_t>(sample,typeOffset)==type) {
            refresh(sample);
            ++samples;
        } else ++missing;
    }
    else missing=static_cast<int>(types.size());
    static int lastCount=-1,lastSamples=-1,lastMissing=-1;
    static auto intervalStart=std::chrono::steady_clock::now();
    static unsigned intervalCalls=0;
    static bool repeatedWarning=false;
    ++intervalCalls;
    if(missing&&missing!=lastMissing)BL_WARN("csharp tooltips: ContentSamples incompleto (%d/%zu amostras ausentes)",missing,types.size());
    if(count!=lastCount||samples!=lastSamples||missing!=lastMissing)
        BL_INFO("csharp items: %d instancia(s) atualizadas, %d amostra(s) de tooltip nativo sincronizadas",count,samples);
    lastCount=count;lastSamples=samples;lastMissing=missing;
    const auto now=std::chrono::steady_clock::now();
    if(now-intervalStart>=std::chrono::seconds(2)) {
        if(intervalCalls>30&&!repeatedWarning) {
            BL_WARN("[CSharp][Refresh] Stage=WorldItems Calls=%u WindowMs=2000 Source=%s Result=RepeatedNativeRehydration",intervalCalls,refreshReason.load(std::memory_order_acquire));
            repeatedWarning=true;
        }
        intervalCalls=0;intervalStart=now;
    }
}
}

bool checkBasicItemContract(std::string* error) {std::lock_guard lock(mutex);return error&&prepare(error);}

bool registerBasicItemPlaceholder(int type,const std::string& name,std::string* error,const std::string& modDir,const std::string& sourceFile) {
    std::lock_guard lock(mutex);
    if(!prepare(error))return false;
    BasicItemEntry item;item.type=type;item.name=name;item.modDir=modDir;item.sourceFile=sourceFile;
    return definitions.emplace(type,std::move(item)).second;
}

bool applyNativeBasicItemDefaults(const std::string& modId,const std::vector<BasicItemEntry>& entries,std::string* error) {
    if(entries.empty())return true; // mods anteriores nao dependem deste contrato novo
    std::lock_guard lock(mutex);
    if(entries.size()>kSourceMaxCategory) {
        *error="[CSharp][ERROR][NativeItem] Mod="+modId+" Stage=ApplyDefaults Expected=4096 Actual="+
            std::to_string(entries.size())+" Result=Rejected Reason=ContentCountExceeded";return false;
    }
    if(!prepare(error))return false;
    std::vector<BasicItemEntry> resolved=entries;
    const auto existing=std::atomic_load_explicit(&nativeAccessoryTable,std::memory_order_acquire);
    auto nextPolicies=std::make_shared<NativeAccessoryTable>(existing?*existing:NativeAccessoryTable{});
    for(auto& entry:resolved) {
        if(!definitions.contains(entry.type)||runtime::modItemKey(entry.type)!=modId+"/"+entry.name||
           !validateBasicItem(entry,error)||!fitIntegerFields(entry,error)||!cacheSound(entry.useSound,error)) {if(error->empty())*error=entry.name+": item nao pertence ao mod";return false;}
        if(entry.placeable) {
            if(!(entry.wallPlaceable?prepareWallPlacement(error):preparePlacement(error))) {
                *error="[CSharp][ERROR][Placement] Mod="+modId+" Class="+entry.name+
                    " File="+definitions.at(entry.type).sourceFile+" ContentId="+std::to_string(entry.type)+" Detail="+*error;return false;
            }
            if(entry.placeTile>std::min<int64_t>(entry.wallPlaceable?wallPlacementField.max:placementFields[0].max,entry.wallPlaceable?65535:INT32_MAX)||(!entry.wallPlaceable&&entry.placeStyle>placementFields[1].max)) {
                *error="[CSharp][ERROR][Placement] Mod="+modId+" Class="+entry.name+
                    " Stage=BindPlacement Expected=ValuesWithinNativeWidths Actual=Tile:"+std::to_string(entry.placeTile)+
                    ",Style:"+std::to_string(entry.placeStyle)+" Result=Rejected Reason=NativeRangeExceeded";return false;
            }
            if(!entry.placeTileKey.empty()&&entry.placeTileKey.find('/')==std::string::npos)
                entry.placeTileKey=modId+"/"+entry.placeTileKey;
        }

        if(!entry.accessoryEffects.empty()) {
            std::vector<NativeAccessoryWrite> writes;
            std::string policyError;
            if(resolveBasicAccessoryPolicy(entry,&writes,&policyError) && prepareBasicAccessoryHook(&policyError)) {
                (*nextPolicies)[entry.type]=std::move(writes);
            } else {
                *error=entry.name+": "+policyError;return false;
            }
        } else nextPolicies->erase(entry.type);
        for(auto& recipe:entry.recipes) {
            if(!recipe.stationKey.empty()&&recipe.stationKey.find('/')==std::string::npos)
                recipe.stationKey=modId+"/"+recipe.stationKey;
        }
        for(auto& recipe:entry.recipes)for(auto& ingredient:recipe.ingredients) {
            if(ingredient.key.front()=='m') {
                auto key=ingredient.key.substr(1);
                if(key.find('/')==std::string::npos)key=modId+"/"+key;
                ingredient.key="m"+key;
                // Dependencies resolve on the game thread, after all mods reserve their IDs.
                ingredient.type=-1;
            }
        }
    }
    for(auto& entry:resolved) {
        entry.modDir=definitions.at(entry.type).modDir;
        entry.sourceFile=definitions.at(entry.type).sourceFile;entry.modId=modId;
    }
    std::shared_ptr<NativeItemPolicyBatch> itemPolicies;
    if(!prepareNativeItemPolicies(resolved,&itemPolicies,error))return false;
    const auto armorBatch=prepareNativeArmorPolicies(modId,resolved,error);
    if(!armorBatch)return false;
    for(auto& entry:resolved) {
        entry.modDir=definitions.at(entry.type).modDir;
        entry.sourceFile=definitions.at(entry.type).sourceFile;entry.modId=modId;
        if(!entry.behavior.empty())behaviorsPending.store(true,std::memory_order_release);
        if(entry.placeable&&!entry.placeTileKey.empty()&&entry.placeTile<0)
            placementsPending.store(true,std::memory_order_release);
        if(!entry.tooltipPlan.empty())
            BL_INFO("[CSharp][Tooltip] Stage=PlanStored Class=%s Type=%d Bytes=%zu Result=Ready",
                entry.name.c_str(),entry.type,entry.tooltipPlan.size());
        if(!entry.tooltip.empty()||!entry.localizedTooltips.empty()) {
            std::vector<std::pair<std::string,std::string>> tooltipLanguages{{"",entry.tooltip}};
            for(const auto& [culture,text]:entry.localizedTooltips)
                tooltipLanguages.emplace_back(culture,text);
            runtime::setModItemTooltip(entry.type,std::move(tooltipLanguages));
        }
        for(size_t i=0;i<entry.recipes.size();++i) {
            auto id=modId+"/"+entry.name+"#"+std::to_string(i);
            recipes.emplace(id,PendingBasicRecipe{id,modId,entry.type,-1,entry.recipes[i],entry.sourceFile,""});
            recipesPending.store(true,std::memory_order_release);
        }
        definitions[entry.type]=std::move(entry);
    }
    std::atomic_store_explicit(&nativeAccessoryTable,
        std::shared_ptr<const NativeAccessoryTable>(std::move(nextPolicies)),std::memory_order_release);
    nativeAccessoryReady.store(true,std::memory_order_release);
    commitNativeArmorPolicies(armorBatch);
    commitNativeItemPolicies(itemPolicies);
    contentGeneration.fetch_add(1,std::memory_order_release);
    BL_INFO("csharp items: %zu defaults nativos aplicados para %s",entries.size(),modId.c_str());
    instancesPending.store(true,std::memory_order_release);
    return true;
}

bool enqueueOwnedSourceRecipes(const std::string& modId,const std::vector<SourceRecipeOwner>& entries,std::string* error) {
    if(entries.empty())return true;
    std::lock_guard lock(mutex);
    std::map<std::string,PendingBasicRecipe> staged;
    auto reject=[&](const SourceRecipeOwner& owner,const std::string& reason,const std::string& expected,const std::string& actual) {
        *error="[CSharp][ERROR][RecipeLoader] Mod="+modId+" Class="+owner.name+" File="+owner.source+
            " ContentId="+std::to_string(owner.type)+" Stage=QueueOwnedRecipes Expected="+expected+
            " Actual="+actual+" Result=Rejected Reason="+reason;return false;
    };
    if(entries.size()>kSourceMaxCategory){*error="[CSharp][ERROR][RecipeLoader] Mod="+modId+" Stage=QueueOwnedRecipes Expected=AtMost4096Owners Actual="+std::to_string(entries.size())+" Reason=OwnerCapacityExceeded";return false;}
    for(const auto& owner:entries) {
        if(owner.type<0||recipeOwnerBase(owner.kind).empty()||definitions.contains(owner.type)||
           runtime::modItemKey(owner.type)!=modId+"/"+owner.name)
            return reject(owner,"OwnerRegistryMismatch","RegisteredSpecializedItemOfMod",runtime::modItemKey(owner.type));
        if(owner.recipes.empty()||owner.recipes.size()>16)return reject(owner,"RecipeCapacityExceeded","1..16Recipes",std::to_string(owner.recipes.size()));
        for(size_t i=0;i<owner.recipes.size();++i) {
            const auto id=modId+"/"+owner.name+"#"+std::to_string(i);
            const auto& recipe=owner.recipes[i];
            if(recipe.ingredients.empty()||recipe.ingredients.size()>15||recipe.stationKey.size()>160)
                return reject(owner,"InvalidRecipeDescriptor","1..15IngredientsAndBoundedStation",id);
            std::string wire=std::to_string(recipe.stack)+","+(recipe.stationKey.empty()?std::to_string(recipe.station):"m"+recipe.stationKey);
            for(const auto& ingredient:recipe.ingredients) {
                if(ingredient.key.empty()||ingredient.key.size()>48000)
                    return reject(owner,"InvalidIngredientDescriptor","NonemptyBoundedTypedKey",id);
                wire+=","+ingredient.key+":"+std::to_string(ingredient.count);
            }
            if(recipe.callbacks)wire+="@"+std::to_string(recipe.callbacks);
            BasicItemEntry checked;checked.name=owner.name;checked.values[BasicMaxStack]=9999;std::string detail;
            if(!parseBasicRecipes(wire,&checked,&detail)||checked.recipes.size()!=1||checked.recipes[0]!=recipe)
                return reject(owner,"InvalidRecipeDescriptor","CanonicalUnresolvedRecipe",id+":"+detail);
            PendingBasicRecipe pending{id,modId,owner.type,-1,recipe,owner.source,""};
            const auto existing=recipes.find(id);
            if((existing!=recipes.end()&&(existing->second.result!=pending.result||existing->second.values!=pending.values))||
               !staged.emplace(id,std::move(pending)).second)
                return reject(owner,"RecipeIdentityConflict","UnchangedUniqueOwnerAndLocalIndex",id);
        }
    }
    // All owners are checked before publishing any new entry. The existing game-thread
    // pump handles dependency resolution, native construction, callbacks and acknowledgement.
    bool added=false;
    for(auto& [id,pending]:staged)if(recipes.emplace(id,std::move(pending)).second)added=true;
    if(added){contentGeneration.fetch_add(1,std::memory_order_release);recipesPending.store(true,std::memory_order_release);}
    error->clear();return true;
}

bool setBasicItemDefense(int type,int value,std::string* error) {
    std::lock_guard lock(mutex);
    const auto entry=definitions.find(type);
    auto reject=[&](const std::string& reason,const std::string& actual) {
        *error="[CSharp][ERROR][NativeItem] Mod="+(entry!=definitions.end()?entry->second.modId:"<unavailable>")+
            " Class="+(entry!=definitions.end()?entry->second.name:"<unavailable>")+
            " File="+(entry!=definitions.end()?entry->second.sourceFile:"<unavailable>")+
            " ContentId="+std::to_string(type)+" Field=defense Stage=BindArmorDefaults"+
            " Expected=InstanceIntegral8/16/32 Actual="+actual+" Value="+std::to_string(value)+
            " Result=Rejected Reason="+reason;
        return false;
    };
    if(entry==definitions.end())return reject("ItemNotRegistered","Missing");
    if(value<0||value>1000)return reject("ValueOutOfRange",std::to_string(value));
    auto& api=il2cpp::api();auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* field=item?il2cpp::findField(item,"defense"):nullptr;
    auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
    IntegerField binding;const char* actual=cls?api.class_get_name(cls):nullptr;
    if(!field||(api.field_get_flags(field)&0x10u)||!integerContract(cls,&binding))
        return reject("NativeTypeMismatch",actual?actual:"Missing");
    const auto offset=api.field_get_offset(field);
    if(offset<sizeof(Il2CppObject)||offset>static_cast<size_t>(INT32_MAX))return reject("InvalidFieldOffset",std::to_string(offset));
    if(!instanceBounds(item,static_cast<int>(offset),static_cast<size_t>(binding.bytes),"defense",error)) {
        const auto detail=*error;return reject("InvalidFieldBounds",detail);
    }
    binding.offset=static_cast<int>(offset);
    if(value>binding.max)return reject("ValueOutOfNativeRange",actual?actual:"Unknown");
    const auto existing=defenseDefaults.find(type);
    if(existing!=defenseDefaults.end()&&existing->second.value==value&&
       existing->second.field.offset==binding.offset&&existing->second.field.bytes==binding.bytes) {
        error->clear();return true;
    }
    defenseDefaults[type]={binding,value};
    refreshReason.store("NativeDefenseChanged",std::memory_order_release);
    instancesPending.store(true,std::memory_order_release);
    error->clear();return true;
}

bool setBasicItemEquipSlot(int type,const std::string& kind,int slot,std::string* error) {
    std::lock_guard lock(mutex);
    const auto entry=definitions.find(type);
    const std::map<std::string,const char*> fields={
        {"Head","headSlot"},{"Body","bodySlot"},{"Legs","legSlot"},
        {"Beard","beardSlot"},{"Back","backSlot"},{"Wings","wingSlot"}};
    const auto named=fields.find(kind);
    auto reject=[&](const std::string& reason,const std::string& actual) {
        *error="[CSharp][ERROR][EquipSlot] Mod="+(entry!=definitions.end()?entry->second.modId:"<unavailable>")+
            " Class="+(entry!=definitions.end()?entry->second.name:"<unavailable>")+
            " File="+(entry!=definitions.end()?entry->second.sourceFile:"<unavailable>")+
            " ContentId="+std::to_string(type)+" Kind="+kind+
            " Field="+(named!=fields.end()?named->second:"<unknown>")+
            " Stage=BindNativeSlot Expected=InstanceIntegral8/16/32 Actual="+actual+
            " Slot="+std::to_string(slot)+" Result=Rejected Reason="+reason;
        return false;
    };
    if(entry==definitions.end())return reject("ItemNotRegistered","Missing");
    if(named==fields.end())return reject("EquipKindUnsupported",kind);
    if(slot<0)return reject("SlotOutOfRange",std::to_string(slot));
    auto& api=il2cpp::api();
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* field=item?il2cpp::findField(item,named->second):nullptr;
    auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
    IntegerField binding;
    const char* actual=cls?api.class_get_name(cls):nullptr;
    if(!field||(api.field_get_flags(field)&0x10u)||!integerContract(cls,&binding))
        return reject("NativeTypeMismatch",actual?actual:"Missing");
    const auto offset=api.field_get_offset(field);
    if(offset<sizeof(Il2CppObject)||offset>static_cast<size_t>(INT32_MAX))
        return reject("InvalidFieldOffset",std::to_string(offset));
    if(!instanceBounds(item,static_cast<int>(offset),static_cast<size_t>(binding.bytes),named->second,error)) {
        const auto detail=*error;return reject("InvalidFieldBounds",detail);
    }
    binding.offset=static_cast<int>(offset);
    if(slot>binding.max)return reject("SlotOutOfRange",actual?actual:"Unknown");
    const auto previous=equipSlots.find(type);
    if(previous!=equipSlots.end()){
        const auto existing=previous->second.find(kind);
        if(existing!=previous->second.end()&&existing->second.value==slot&&
           existing->second.field.offset==binding.offset&&existing->second.field.bytes==binding.bytes){
            error->clear();return true;
        }
    }
    equipSlots[type][kind]={binding,slot};
    refreshReason.store("NativeEquipSlotChanged",std::memory_order_release);
    instancesPending.store(true,std::memory_order_release);
    error->clear();
    return true;
}

bool applyBasicItemToObject(Il2CppObject* object,int type) {
    NativeDefaultsAfter nativeDefaults{object,false,type};
    std::lock_guard lock(mutex);const auto found=definitions.find(type);
    if(found==definitions.end()||!object)return false;
    const auto& entry=found->second;
    if(entry.wallPlaceable) {
        uint16_t target=static_cast<uint16_t>(std::max(0,entry.placeTile));
        void* args[]={&target};Il2CppObject* exception=nullptr;
        il2cpp::api().runtime_invoke(wallDefaultMethod,object,args,&exception);
        if(exception) {
            BL_ERROR("[CSharp][ERROR][NativeItem] Mod=%s Class=%s File=%s ContentId=%d API=DefaultToPlaceableWall Stage=Invoke Result=Rejected Reason=NativeException Exception=%s",entry.modId.c_str(),entry.name.c_str(),entry.sourceFile.c_str(),type,il2cpp::describeException(exception).c_str());
            return false;
        }
    }
    const bool cloned=applyNativeItemPolicyDefaults(object,type);
    if(entry.placeable) {
        if(entry.wallPlaceable)writeIntegral(object,wallPlacementField,std::max(0,entry.placeTile));
        else {
            writeIntegral(object,placementFields[0],entry.placeTile);
            writeIntegral(object,placementFields[1],entry.placeStyle);
        }
    }
    for(size_t i=0;i<integerFields.size();++i) {
        if(entry.wallPlaceable&&(entry.wallIntegerOverrides&(1<<i))==0)continue;
        if(cloned&&i<2&&entry.values[i]==0)continue;
        const auto& field=integerFields[i];auto* destination=reinterpret_cast<unsigned char*>(object)+field.offset;
        // Converter antes de copiar: jamais escrever quatro bytes em Byte/Int16.
        switch(field.bytes) {
            case 1: {const auto value=static_cast<uint8_t>(entry.values[i]);std::memcpy(destination,&value,1);break;}
            case 2: {const auto value=static_cast<uint16_t>(entry.values[i]);std::memcpy(destination,&value,2);break;}
            case 4: {const auto value=static_cast<uint32_t>(entry.values[i]);std::memcpy(destination,&value,4);break;}
        }
    }
    for(size_t i=0;i<flagOffsets.size();++i) {
        if(entry.wallPlaceable&&(entry.wallFlagOverrides&(1<<i))==0)continue;
        runtime::field<uint8_t>(object,flagOffsets[i])=!!(entry.values[BasicFlags]&(1<<i));
    }
    const auto defense=defenseDefaults.find(type);
    if(defense!=defenseDefaults.end()) {
        const auto& value=defense->second;
        auto* destination=reinterpret_cast<unsigned char*>(object)+value.field.offset;
        switch(value.field.bytes) {
            case 1: {const auto v=static_cast<uint8_t>(value.value);std::memcpy(destination,&v,1);break;}
            case 2: {const auto v=static_cast<uint16_t>(value.value);std::memcpy(destination,&v,2);break;}
            case 4: {const auto v=static_cast<uint32_t>(value.value);std::memcpy(destination,&v,4);break;}
        }
    }
    const auto slots=equipSlots.find(type);
    if(slots!=equipSlots.end())for(const auto& [kind,slot]:slots->second) {
        auto* destination=reinterpret_cast<unsigned char*>(object)+slot.field.offset;
        switch(slot.field.bytes) {
            case 1: {const auto v=static_cast<uint8_t>(slot.value);std::memcpy(destination,&v,1);break;}
            case 2: {const auto v=static_cast<uint16_t>(slot.value);std::memcpy(destination,&v,2);break;}
            case 4: {const auto v=static_cast<uint32_t>(slot.value);std::memcpy(destination,&v,4);break;}
        }
    }
    runtime::field<float>(object,knockBackOffset)=entry.knockBack;
    auto& api=il2cpp::api();auto* destination=reinterpret_cast<unsigned char*>(object)+soundOffset;
    if(!cloned||!entry.useSound.empty())std::memset(destination,0,static_cast<size_t>(soundSize));
    if(!entry.useSound.empty()) {
        const auto& bytes=soundCache.at(entry.useSound);
        std::memcpy(destination,bytes.data(),bytes.size());
        // Referencias sao estaticos SoundID vivos; avisar tambem o GC incremental.
        if(api.gc_wbarrier_set_field)for(auto offset:soundReferences) {
            void* value=nullptr;std::memcpy(&value,destination+offset,sizeof(value));
            api.gc_wbarrier_set_field(object,reinterpret_cast<void**>(destination+offset),value);
        }
    }
    return true;
}
void applyBasicItemAccessoryEffects(Il2CppObject* player,int itemType) {
    if(!player||!nativeAccessoryReady.load(std::memory_order_acquire))return;
    const auto snapshot=std::atomic_load_explicit(&nativeAccessoryTable,std::memory_order_acquire);
    if(!snapshot)return;
    const auto found=snapshot->find(itemType);
    if(found==snapshot->end())return;
    const auto& effects=found->second;
    if(effects.size()>16)return;
    std::array<float,16> results{};
    // Compute the complete ordered policy before publishing any field writes.
    for(size_t i=0;i<effects.size();++i) {
        const auto& effect=effects[i];if(effect.kind==2)continue;
        float value=0;std::memcpy(&value,reinterpret_cast<unsigned char*>(player)+effect.offset,sizeof(value));
        for(size_t j=i;j>0;--j)if(effects[j-1].offset==effect.offset&&effects[j-1].kind!=2){value=results[j-1];break;}
        results[i]=effect.kind==1?value*effect.value:value+effect.value;
        if(!std::isfinite(value)||!std::isfinite(results[i]))return;
    }
    for(size_t i=0;i<effects.size();++i) {
        auto* target=reinterpret_cast<unsigned char*>(player)+effects[i].offset;
        if(effects[i].kind==2)*target=1;
        else std::memcpy(target,&results[i],sizeof(float));
    }
}
std::string basicItemTooltipPlan(int type) {
    std::lock_guard lock(mutex);
    const auto it=definitions.find(type);
    return it==definitions.end()?std::string{}:it->second.tooltipPlan;
}
bool hasNativeTooltipPlans() {
    std::lock_guard lock(mutex);
    for(const auto& [type,entry]:definitions) {
        (void)type;
        if(entry.nativeTooltip)return true;
    }
    return false;
}
std::optional<NativeTooltipPlan> basicItemNativeTooltip(int type) {
    std::lock_guard lock(mutex);
    const auto it=definitions.find(type);
    return it==definitions.end()?std::nullopt:it->second.nativeTooltip;
}
void retireBasicItemTooltipsForMod(const std::string& modId) {
    std::lock_guard lock(mutex);
    for(auto& [type,entry]:definitions) {
        (void)type;
        if(entry.modId!=modId)continue;
        entry.tooltipPlan.clear();
        entry.nativeTooltip.reset();
    }
}

bool basicItemIsAccessory(int type) {std::lock_guard lock(mutex);auto it=definitions.find(type);return it!=definitions.end()&&(it->second.values[BasicFlags]&32);}
std::vector<BasicItemEntry> pendingBasicItemBehaviors() {
    std::lock_guard lock(mutex);std::vector<BasicItemEntry> out;
    auto cursor=definitions.upper_bound(lastBehaviorType);
    for(size_t scanned=0;scanned<definitions.size();++scanned) {
        if(cursor==definitions.end())cursor=definitions.begin();
        const auto& [type,entry]=*cursor++;
        if(!entry.behavior.empty()&&!entry.behaviorInstalled) {
            out.push_back(entry);lastBehaviorType=type;if(out.size()==8)break;
        }
    }
    return out;
}
bool acknowledgeBasicItemBehavior(int type,const std::string& error) {
    std::lock_guard lock(mutex);auto it=definitions.find(type);
    if(it==definitions.end()||it->second.behavior.empty()||(it->second.behaviorInstalled&&error.empty()))return false;
    it->second.behaviorError=error.substr(0,1000);
    if(error.empty())it->second.behaviorInstalled=true;
    bool pending=false;
    for(const auto& [id,entry]:definitions)if(!entry.behavior.empty()&&!entry.behaviorInstalled){pending=true;break;}
    behaviorsPending.store(pending,std::memory_order_release);return true;
}
bool basicItemHasDamage(int type) {std::lock_guard lock(mutex);auto it=definitions.find(type);return it!=definitions.end()&&it->second.values[BasicDamage]>0&&it->second.values[BasicMaxStack]==1;}
bool isBasicItem(int type) {std::lock_guard lock(mutex);return definitions.contains(type);}
bool basicItemPolicyReady(int type) {
    std::lock_guard lock(mutex);const auto it=definitions.find(type);
    return (it==definitions.end()||it->second.behavior.empty()||it->second.behaviorInstalled)&&nativeArmorPolicyReady(type)&&nativeItemPolicyReady(type);
}
BasicItemLoadStatus basicItemLoadStatus() {
    std::lock_guard lock(mutex);
    BasicItemLoadStatus state;
    state.items=static_cast<int>(definitions.size());
    state.pendingBehaviors+=static_cast<int>(pendingNativeArmorPolicies()+pendingManagedNativeEquipTextures());
    for(const auto& [type,entry]:definitions) {
        if(!entry.behavior.empty()) {
            if(entry.behaviorInstalled)++state.readyBehaviors;
            else ++state.pendingBehaviors;
        }
        if(entry.placeable&&!entry.placeTileKey.empty()&&entry.placeTile<0)
            ++state.pendingPlacements;
    }
    for(const auto& [key,entry]:recipes)
        if(entry.index>=0)++state.installedRecipes;
        else ++state.pendingRecipes;
    return state;
}
std::vector<int> basicItemTypes() {std::lock_guard lock(mutex);std::vector<int> out;for(const auto& p:definitions)out.push_back(p.first);return out;}
int basicItemForTile(const std::string& key) {
    std::lock_guard lock(mutex);
    for (const auto& [id, entry] : definitions)
        if (entry.placeable && !entry.wallPlaceable && entry.placeStyle == 0 && entry.placeTileKey == key)
            return id;
    return -1;
}

int basicItemForWall(const std::string& key) {
    std::lock_guard lock(mutex);
    for(const auto& [type,entry]:definitions)
        if(entry.placeable&&entry.wallPlaceable&&entry.placeTileKey==key)return type;
    return -1;
}

std::vector<PendingBasicRecipe> pendingBasicRecipes() {
    std::lock_guard lock(mutex);std::vector<PendingBasicRecipe> out;
    auto cursor=recipes.upper_bound(lastRecipeKey);
    for(size_t scanned=0;scanned<recipes.size();++scanned) {
        if(cursor==recipes.end())cursor=recipes.begin();
        const auto& [key,entry]=*cursor++;
        if(entry.index<0) {out.push_back(entry);lastRecipeKey=key;if(out.size()==8)break;}
    }
    return out;
}
bool reportBasicRecipeError(const std::string& id,const std::string& error) {
    std::lock_guard lock(mutex);auto it=recipes.find(id);
    if(it==recipes.end()||it->second.index>=0)return false;
    it->second.error=error.substr(0,1000);return true;
}
bool acknowledgeBasicRecipe(const std::string& id,int index) {
    std::lock_guard lock(mutex);auto it=recipes.find(id);
    if(it==recipes.end()||index<0||it->second.index>=0)return false;
    it->second.index=index;it->second.error.clear();
    bool pending=false;for(const auto& p:recipes)if(p.second.index<0){pending=true;break;}
    recipesPending.store(pending,std::memory_order_release);return true;
}
void requestBasicItemInstanceRefresh(const char* reason) {
    refreshReason.store(reason ? reason : "Unspecified",std::memory_order_release);
    instancesPending.store(true, std::memory_order_release);
}

bool hasManagedRecipeCallbacks(const std::string& modId) {
    std::lock_guard lock(mutex);
    for(const auto& pair:recipes)if(pair.second.modId==modId&&pair.second.values.callbacks)return true;
    return false;
}
void pumpBasicRecipesOnGameThread() {
    pumpNativeArmorPoliciesOnGameThread();
    pumpNativeTooltipGameplay();
    static unsigned ticks=0, knownGeneration=0, fastPasses=0;
    const unsigned generation=contentGeneration.load(std::memory_order_acquire);
    if(generation!=knownGeneration) {
        knownGeneration=generation;
        fastPasses=16;
    }
    const bool fast=fastPasses>0;
    if(fast)--fastPasses;
    ++ticks;
    if(placementsPending.load(std::memory_order_acquire)&&(fast||ticks%120u==0))resolvePlacements();
    pumpNativeItemPoliciesOnGameThread();
    if(instancesPending.exchange(false,std::memory_order_acq_rel))refreshLiveBasicItems();

    if(recipesPending.load(std::memory_order_acquire)) {
        if(fast||ticks%120u==0)for(const auto& pending:pendingBasicRecipes()) {
            runtime::NativeRecipeRequest request;
            request.key=pending.id;request.mod=pending.modId;request.source=pending.source;
            const auto slash=pending.id.find('/'), hash=pending.id.rfind('#');
            request.name=pending.id.substr(slash+1,hash-slash-1);
            request.result=pending.result;request.stack=pending.values.stack;request.callbacks=pending.values.callbacks;
            request.station=pending.values.station;request.stationKey=pending.values.stationKey;
            for(const auto& ingredient:pending.values.ingredients)
                request.ingredients.push_back({ingredient.type,ingredient.count,
                    ingredient.key.front()=='m'?ingredient.key.substr(1):std::string{},
                    ingredient.key.front()=='g'?ingredient.key.substr(1):std::string{},
                    ingredient.customGroup,ingredient.groupDescription,ingredient.groupMembers});
            runtime::NativeRecipeRequest resolved;std::string dependencyError;
            runtime::NativeRecipeResult result;
            if(!runtime::resolveNativeRecipe(request,&resolved,&dependencyError))result.error=dependencyError;
            else {
                int waiting=!basicItemPolicyReady(resolved.result)?resolved.result:-1;
                for(const auto& ingredient:resolved.ingredients)if(!basicItemPolicyReady(ingredient.type)){waiting=ingredient.type;break;}
                if(waiting>=0)result.error="[CSharp][WARN][Recipe] Mod="+request.mod+" Class="+request.name+
                    " File="+request.source+" Recipe="+request.key+" Stage=WaitItemPolicy Expected=InstalledItemPolicy Actual=ContentId:"+
                    std::to_string(waiting)+" Result=Pending Reason=ItemPolicyNotReady";
                else result=runtime::installNativeRecipe(request);
            }
            if(result.index>=0) {
                if(acknowledgeBasicRecipe(pending.id,result.index))
                    BL_INFO("csharp recipe: %s indice=%d instalada pelo nucleo nativo",pending.id.c_str(),result.index);
            } else if(result.error.substr(0,1000)!=pending.error) {
                reportBasicRecipeError(pending.id,result.error);
                if(result.error.rfind("[CSharp][WARN]",0)==0)BL_DEBUG("%s",result.error.c_str());
                else BL_ERROR("%s",result.error.c_str());
            }
        }
    }

}
}
