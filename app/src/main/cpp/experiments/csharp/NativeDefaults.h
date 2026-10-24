#pragma once
#include "il2cpp/FieldContract.h"
#include "experiments/csharp/NativeUseSounds.h"
#include <atomic>
#include <charconv>
#include <cmath>
#include <map>
#include <memory>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <mutex>

namespace bl::csharp {
struct NativeDefaultWrite {int offset=-1;std::vector<uint8_t> bytes;FieldInfo* sound=nullptr;FieldInfo* itemSet=nullptr;};
struct NativeDefaultsRequest {std::string name,mod,source,descriptor;int type=-1;bool projectile=false;};
using NativeDefaultsBindings=std::vector<NativeDefaultWrite>;
using NativeDefaultsTable=std::map<std::pair<bool,int>,NativeDefaultsBindings>;
inline std::shared_ptr<const NativeDefaultsTable> g_nativeDefaults=std::make_shared<const NativeDefaultsTable>();
inline std::mutex g_nativeDefaultsWriter;

// Defaults must never change identity, ownership, pool membership or native references.
inline bool nativeDefaultAllowed(const std::string& field,bool projectile) {
    const std::string item=" ItemSet_GamepadWholeScreenUseRange ItemSet_LockOnIgnoresCollision ItemSet_StaffMinionSlotsRequired UseSound width height maxStack value rare damage crit knockBack useTime useAnimation useStyle melee ranged magic summon noMelee autoReuse useTurn consumable accessory vanity pick axe hammer mana shoot shootSpeed useAmmo ammo channel noUseGraphic tileBoost reuseDelay useLimitPerAnimation holdStyle useAmmo canBePlacedInVanity hasVanityEffects noWet bait fishingPole healLife healMana potion buffTime " ;
    const std::string proj=" width height damage knockBack aiStyle friendly hostile melee ranged magic minion sentry minionSlots penetrate maxPenetrate timeLeft ignoreWater tileCollide scale light alpha extraUpdates usesLocalNPCImmunity localNPCHitCooldown usesIDStaticNPCImmunity idStaticNPCHitCooldown armorPenetration bonusCritChance coldDamage noEnchantments noEnchantmentVisuals ownerHitCheck netImportant hide arrow npcProj trap noDropItem soundDelay spriteDirection direction rotation " ;
    return (projectile?proj:item).find(" "+field+" ")!=std::string::npos;
}
inline bool bindNativeDefaults(Il2CppClass* cls,const NativeDefaultsRequest& request,
    NativeDefaultsBindings* output,std::string* error,
    int(*projectileId)(const std::string&)=nullptr) {
    auto reject=[&](const std::string& field,const std::string& why,const std::string& expected,const std::string& actual) {
        *error="[CSharp][ERROR][NativeDefaults] Mod="+request.mod+" Class="+request.name+" File="+request.source+
            " Category="+(request.projectile?"Projectile":"Item")+" ContentId="+std::to_string(request.type)+
            " Field="+field+" Stage=BindDefaults Expected="+expected+" Actual="+actual+" Result=BatchRejected Reason="+why;return false;
    };
    auto split=[](const std::string& text,char separator){std::vector<std::string> parts;size_t start=0;for(size_t i=0;i<=text.size();++i)if(i==text.size()||text[i]==separator){parts.push_back(text.substr(start,i-start));start=i+1;}return parts;};
    if(request.descriptor.empty()||request.descriptor.size()>16384)return reject("<descriptor>","InvalidDescriptorSize","1..16384Bytes",std::to_string(request.descriptor.size()));
    const auto fields=split(request.descriptor,';');
    if(fields.size()>64)return reject("<descriptor>","FieldCapacityExceeded","AtMost64",std::to_string(fields.size()));
    if(!cls)return reject("<class>","NativeClassMissing","Terraria.Item/Projectile","Missing");
    auto& a=il2cpp::api();NativeDefaultsBindings staged;std::map<std::string,bool> seen;
    if(!a.class_from_il2cpp_type||!a.field_get_type||!a.class_get_namespace||!a.class_get_name||!a.class_is_valuetype||
       !a.field_get_flags||!a.class_value_size||!a.field_get_offset||!a.class_instance_size)
        return reject("<exports>","RequiredExportMissing","IL2CPP field/type/layout introspection","Unavailable");
    for(const auto& record:fields) {
        auto parts=split(record,':');const std::string name=parts[0];
        if(parts.size()<3||parts.size()>4||!nativeDefaultAllowed(name,request.projectile)||!seen.emplace(name,true).second)
            return reject(name,"InvalidOrUnsupportedField","UniqueMutableContentDefault",record);
        if(name.rfind("ItemSet_",0)==0&&!request.projectile) {
            if(parts.size()!=3)return reject(name,"ItemSetValueInvalid","Three typed fields",record);
            const bool slots=name=="ItemSet_StaffMinionSlotsRequired";const size_t width=slots?4:1;
            auto* sets=il2cpp::findClass({"Terraria.ID","ItemID","Sets"});
            if(sets&&a.runtime_class_init)a.runtime_class_init(sets);
            auto* field=sets?il2cpp::findField(sets,name.substr(8)):nullptr;Il2CppArray* array=nullptr;
            if(!field||!(a.field_get_flags(field)&0x10u)||!a.field_static_get_value)return reject(name,"ItemSetMissing","Static primitive array","Missing");
            a.field_static_get_value(field,&array);
            if(request.type<1||!array||array->length<=static_cast<uintptr_t>(request.type)||
               !il2cpp::nativeArrayContract(array,slots?"Int32":"Boolean",width))return reject(name,"ItemSetArrayMismatch","Expanded typed ItemID.Sets array","WrongTypeOrCapacity");
            NativeDefaultWrite write;write.itemSet=field;write.bytes.resize(width);
            if(slots) {
                int value=0;auto parsed=std::from_chars(parts[2].data(),parts[2].data()+parts[2].size(),value);
                if(parts[1]!="i"||parts[2].empty()||parsed.ec!=std::errc{}||parsed.ptr!=parts[2].data()+parts[2].size()||value<0||value>100)return reject(name,"ItemSetValueInvalid","Slots0..100",record);
                std::memcpy(write.bytes.data(),&value,4);
            } else {
                if(parts[1]!="b"||(parts[2]!="0"&&parts[2]!="1"))return reject(name,"ItemSetValueInvalid","Boolean0or1",record);
                write.bytes[0]=parts[2]=="1";
            }
            staged.push_back(std::move(write));continue;
        }
        if(name=="UseSound"&&!request.projectile) {
            if(parts.size()!=3||parts[1]!="s"||parts[2].empty()||parts[2].size()>72)
                return reject(name,"InvalidVanillaSound","SoundID field name",record);
            for(char c:parts[2])if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')return reject(name,"InvalidVanillaSound","SoundID field name",record);
            int offset=-1;FieldInfo* destination=nullptr;FieldInfo* source=nullptr;
            if(!a.field_static_get_value||(!a.gc_wbarrier_set_field&&(!a.gc_is_incremental||a.gc_is_incremental()))||
               !il2cpp::nativeFieldContract(cls,"UseSound","Terraria.Audio.LegacySoundStyle",false,sizeof(void*),&destination,&offset,error)||
               (a.field_get_flags(destination)&0x20u))return reject(name,"NativeSoundContractUnavailable","WritableLegacySoundStyle",*error);
            auto* soundIds=il2cpp::findClass({"Terraria.ID","SoundID",{}});
            if(soundIds&&a.runtime_class_init)a.runtime_class_init(soundIds);
            if(!il2cpp::nativeFieldContract(soundIds,parts[2].c_str(),"Terraria.Audio.LegacySoundStyle",true,sizeof(void*),&source,nullptr,error))
                return reject(name,"VanillaSoundMissing","StaticLegacySoundStyle",*error);
            Il2CppObject* value=nullptr;a.field_static_get_value(source,&value);
            if(!value)return reject(name,"VanillaSoundNotInitialized","SoundID style","Null");
            NativeDefaultWrite write;write.offset=offset;write.sound=source;staged.push_back(std::move(write));continue;
        }
        auto* field=il2cpp::findField(cls,name);auto* type=field?a.class_from_il2cpp_type(a.field_get_type(field)):nullptr;
        const char* ns=type?a.class_get_namespace(type):nullptr;const char* kind=type?a.class_get_name(type):nullptr;
        const std::string actual=type?std::string(ns?ns:"")+"."+(kind?kind:"<unnamed>"):"Missing";
        if(!field||!type||!ns||!kind||std::strcmp(ns,"System")||!a.class_is_valuetype(type)||(a.field_get_flags(field)&0x30u))
            return reject(name,"NativeTypeOrStorageMismatch","WritableInstancePrimitive",actual);
        uint32_t alignment=0;int width=a.class_value_size(type,&alignment);
        const size_t offset=a.field_get_offset(field),size=a.class_instance_size(cls);
        if(width!=1&&width!=2&&width!=4)return reject(name,"NativePrimitiveSizeUnsupported","Primitive1/2/4Bytes",actual+"/"+std::to_string(width));
        if(offset<sizeof(Il2CppObject)||static_cast<size_t>(width)>size||offset>size-static_cast<size_t>(width)||offset>INT32_MAX)
            return reject(name,"InvalidFieldBounds","FieldWithinInstance",std::to_string(offset)+"/"+std::to_string(width)+"/"+std::to_string(size));
        NativeDefaultWrite write;write.offset=static_cast<int>(offset);write.bytes.resize(static_cast<size_t>(width));
        if(parts[1]=="b") {
            if(parts.size()!=3||std::strcmp(kind,"Boolean")||width!=1||(parts[2]!="0"&&parts[2]!="1"))return reject(name,"NativeTypeOrValueMismatch","System.Boolean/1Byte:0or1",actual+":"+parts[2]);
            write.bytes[0]=parts[2]=="1";
        } else if(parts[1]=="f") {
            char* end=nullptr;float value=std::strtof(parts[2].c_str(),&end);
            if(parts.size()!=3||std::strcmp(kind,"Single")||width!=4||parts[2].empty()||!end||*end||!std::isfinite(value))return reject(name,"NativeTypeOrValueMismatch","FiniteSystem.Single/4Bytes",actual+":"+parts[2]);
            std::memcpy(write.bytes.data(),&value,4);
        } else {
            int value=0;
            if(parts[1]=="i"&&parts.size()==3) {
                auto parsed=std::from_chars(parts[2].data(),parts[2].data()+parts[2].size(),value);
                if(parts[2].empty()||parsed.ec!=std::errc{}||parsed.ptr!=parts[2].data()+parts[2].size())return reject(name,"InvalidInteger","Int32",parts[2]);
            } else if(parts[1]=="p"&&parts.size()==3&&name=="shoot"&&!request.projectile) {
                const auto key=parts[2].find('/')==std::string::npos?request.mod+"/"+parts[2]:parts[2];
                value=projectileId?projectileId(key):-1;
                if(value<0)return reject(name,"ProjectileDependencyMissing","RegisteredProjectileKey",key);
            } else if(parts[1]=="d"&&parts.size()==4) {
                const auto& family=parts[2];
                if(family!="AmmoID"&&family!="ProjectileID"&&family!="ItemID"&&family!="ItemUseStyleID"&&family!="ItemRarityID")return reject(name,"UnsupportedIdFamily","Terraria.ID content constant",family);
                std::string why;
                if(!il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID",family,{}}),parts[3].c_str(),&value,&why))return reject(name,"NativeIdResolutionFailed",family+"."+parts[3],why);
            } else return reject(name,"InvalidTypedDescriptor","i,b,f,d,p",record);
            if(((name=="width"||name=="height")&&(value<1||value>4096))||
               (name=="extraUpdates"&&(value<0||value>64))||(name=="maxStack"&&(value<1||value>9999))||
               (name=="timeLeft"&&(value<1||value>36000)))return reject(name,"UnsafeDefaultRange","BoundedGeometryOrUpdateCount",std::to_string(value));
            if((name=="ammo"||name=="useAmmo")&&(value<0||value>=6196))return reject(name,"UnresolvedAmmoCategory","VanillaAmmoId0..6195",std::to_string(value));
            if(name=="shoot"&&parts[1]!="p"&&(value<0||value>=1136))return reject(name,"UnresolvedProjectileId","VanillaId0..1135OrNamedModProjectile",std::to_string(value));
            struct Integer {const char* name;int width;int64_t min,max;};
            const Integer integers[]={{"Byte",1,0,255},{"SByte",1,-128,127},{"UInt16",2,0,65535},{"Int16",2,-32768,32767},{"Int32",4,INT32_MIN,INT32_MAX},{"UInt32",4,0,UINT32_MAX}};
            bool fits=false;
            for(const auto& integer:integers)if(!std::strcmp(kind,integer.name)&&width==integer.width&&value>=integer.min&&value<=integer.max)fits=true;
            if(!fits)return reject(name,"NativeIntegerTypeOrRangeMismatch","Integral8/16/32WithRepresentableValue",actual+":"+std::to_string(value));
            switch(width){case 1:{uint8_t n=static_cast<uint8_t>(value);std::memcpy(write.bytes.data(),&n,1);break;}case 2:{uint16_t n=static_cast<uint16_t>(value);std::memcpy(write.bytes.data(),&n,2);break;}case 4:{uint32_t n=static_cast<uint32_t>(value);std::memcpy(write.bytes.data(),&n,4);break;}}
        }
        staged.push_back(std::move(write));
    }
    *output=std::move(staged);return true;
}
inline bool publishNativeDefaults(const std::vector<NativeDefaultsRequest>& requests,std::string* error,int(*projectileId)(const std::string&)) {
    std::lock_guard lock(g_nativeDefaultsWriter);
    auto next=std::make_shared<NativeDefaultsTable>(*std::atomic_load_explicit(&g_nativeDefaults,std::memory_order_acquire));
    std::map<std::pair<bool,int>,bool> seen;
    for(const auto& request:requests) {
        NativeDefaultsBindings fields;auto key=std::make_pair(request.projectile,request.type);
        if(!seen.emplace(key,true).second){*error="[CSharp][ERROR][NativeDefaults] Mod="+request.mod+" Class="+request.name+" Stage=Publish Reason=DuplicateContentDefaults Result=BatchRejected";return false;}
        if(!bindNativeDefaults(il2cpp::findClass({"Terraria",request.projectile?"Projectile":"Item",{}}),request,&fields,error,projectileId))return false;
        (*next)[key]=std::move(fields);
    }
    std::atomic_store_explicit(&g_nativeDefaults,std::shared_ptr<const NativeDefaultsTable>(std::move(next)),std::memory_order_release);return true;
}
inline void applyNativeDefaults(Il2CppObject* object,bool projectile,int type) {
    if(!object)return;
    const auto table=std::atomic_load_explicit(&g_nativeDefaults,std::memory_order_acquire);const auto found=table->find({projectile,type});if(found==table->end())return;
    for(const auto& field:found->second) {
        if(field.itemSet) {
            Il2CppArray* array=nullptr;il2cpp::api().field_static_get_value(field.itemSet,&array);
            if(array&&type>=0&&array->length>static_cast<uintptr_t>(type))std::memcpy(static_cast<char*>(arrayData(array))+static_cast<size_t>(type)*field.bytes.size(),field.bytes.data(),field.bytes.size());
            continue;
        }
        auto* destination=reinterpret_cast<char*>(object)+field.offset;
        if(!field.sound){std::memcpy(destination,field.bytes.data(),field.bytes.size());continue;}
        Il2CppObject* style=nullptr;auto& a=il2cpp::api();a.field_static_get_value(field.sound,&style);
        if(a.gc_wbarrier_set_field)a.gc_wbarrier_set_field(object,reinterpret_cast<void**>(destination),style);
        else std::memcpy(destination,&style,sizeof(style));
    }
}
struct NativeDefaultsAfter {Il2CppObject* object;bool projectile;int type;~NativeDefaultsAfter(){applyNativeDefaults(object,projectile,type);if(!projectile)useSounds::apply(object,type);}};
}
