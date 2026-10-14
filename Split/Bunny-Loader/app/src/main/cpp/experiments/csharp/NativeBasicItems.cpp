#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/items/ModItems.h"
#include "content/common/GameRefs.h"
#include "experiments/csharp/NativeContent.h"
#include "il2cpp/Signature.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "script/bridge/ScriptEngine.h"
#include "core/Log.h"
#if BL_HAVE_QUICKJS
#include <quickjs.h>
#endif
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>

namespace bl::script { size_t structFieldOffset(Il2CppClass*,size_t); }
namespace bl::csharp {
namespace {
std::mutex mutex;
std::map<int,BasicItemEntry> definitions;
std::map<std::string,PendingBasicRecipe> recipes;
std::atomic<bool> recipesPending{false};
std::atomic<bool> instancesPending{false};
std::atomic<bool> behaviorsPending{false};
struct IntegerField { int offset=-1; int bytes=0; int64_t min=0,max=0; };
std::array<IntegerField,BasicFieldCount-1> integerFields{};
struct EquipSlot { IntegerField field; int value=-1; };
std::map<int,std::map<std::string,EquipSlot>> equipSlots;

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

bool prepare(std::string* error) {
    if(prepared)return true;
    auto& api=il2cpp::api();
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    if(!item||!api.field_get_type||!api.class_from_il2cpp_type||!api.class_get_name||
       !api.class_get_namespace||!api.class_get_fields||!api.field_get_flags||!api.field_get_offset||
       !api.class_value_size||!api.class_is_valuetype||
       !api.runtime_class_init||!api.field_static_get_value) {*error="BunnyItem: contrato de campos IL2CPP indisponivel";return false;}
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
        if(binding.offset<static_cast<int>(sizeof(Il2CppObject))) {*error=std::string("BunnyItem: offset invalido: ")+integerNames[i];return false;}
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
    }
    auto* knockField=il2cpp::findField(item,"knockBack");
    auto* knockClass=knockField?api.class_from_il2cpp_type(api.field_get_type(knockField)):nullptr;
    uint32_t knockAlign=0;
    if(!systemPrimitive(knockClass,"Single")||(api.field_get_flags(knockField)&0x10u)||api.class_value_size(knockClass,&knockAlign)!=4) {
        *error="[CSharp][ERROR][NativeItem] Stage=BindFields Field=knockBack Expected=Single/4Bytes Result=Rejected Reason=NativeTypeMismatch";return false;
    }
    knockBackOffset=il2cpp::fieldOffset(item,"knockBack");
    auto* field=il2cpp::findField(item,"UseSound");
    auto* nullable=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
    if(knockBackOffset<0||!nullable) {*error="BunnyItem: knockBack/UseSound indisponivel";return false;}
    soundOffset=il2cpp::fieldOffset(item,"UseSound");
    if(soundOffset<0) {*error="BunnyItem: offset UseSound indisponivel";return false;}
    if(!api.class_is_valuetype(nullable)) {
        // Terraria Mobile usa LegacySoundStyle como referencia. O caminho
        // de Nullable abaixo cobre hosts que usam SoundStyle como struct.
        soundClass=nullable;soundSize=sizeof(void*);soundReferences={0};
        prepared=true;return true;
    }
    uint32_t align=0;soundSize=api.class_value_size(nullable,&align);
    if(soundSize<1||soundSize>512||soundOffset<0) {*error="BunnyItem: layout UseSound invalido";return false;}
    // SoundStyle? tem hasValue e value; resolver pelo TIPO evita offsets fixos
    // dependentes da build Unity/IL2CPP ou do nome privado do campo.
    void* iter=nullptr;
    while(auto* f=api.class_get_fields(nullable,&iter)) {
        if(api.field_get_flags(f)&0x10u)continue;
        auto* cls=api.class_from_il2cpp_type(api.field_get_type(f));
        const auto offset=bl::script::structFieldOffset(nullable,api.field_get_offset(f));
        if(cls&&std::strcmp(api.class_get_name(cls),"Boolean")==0) soundHasValueOffset=static_cast<int>(offset);
        else if(cls&&api.class_is_valuetype(cls)) {soundClass=cls;soundValueOffset=static_cast<int>(offset);}
    }
    if(!soundClass||soundHasValueOffset<0||soundHasValueOffset>=soundSize) {*error="BunnyItem: Nullable<SoundStyle> nao reconhecido";return false;}
    const int valueSize=api.class_value_size(soundClass,&align);
    if(valueSize<1||soundValueOffset<0||soundValueOffset+valueSize>soundSize) {*error="BunnyItem: tamanho SoundStyle incompativel";return false;}
    iter=nullptr;
    while(auto* f=api.class_get_fields(soundClass,&iter)) {
        if(api.field_get_flags(f)&0x10u)continue;
        auto* cls=api.class_from_il2cpp_type(api.field_get_type(f));
        if(cls&&!api.class_is_valuetype(cls)) {
            auto offset=static_cast<size_t>(soundValueOffset)+bl::script::structFieldOffset(soundClass,api.field_get_offset(f));
            if(offset+sizeof(void*)>static_cast<size_t>(soundSize)) {*error="BunnyItem: referencia SoundStyle fora do layout";return false;}
            soundReferences.push_back(offset);
        }
    }
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

#if BL_HAVE_QUICKJS
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
            const bool applied=result&&!exception&&*reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(result)+sizeof(Il2CppObject));
            if(!applied) {
                runtime::field<uint8_t>(object,prefixOffset)=static_cast<uint8_t>(savedPrefix);
                BL_WARN("csharp item: reidratacao type=%d prefix=%d nao reaplicado",type,savedPrefix);
            }
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
    refreshManagedPrefixEligibilityOnWorldReady();
    auto* players=static_cast<Il2CppArray*>(getStatic("player"));
    if(players&&players->length<=256) {
        auto** objects=static_cast<Il2CppObject**>(arrayData(players));
        for(uintptr_t i=0;i<players->length;++i)if(objects[i]&&(activePlayer<0||runtime::field<uint8_t>(objects[i],activePlayer))) {
            for(const char* fieldName:{"inventory","armor","dye","miscEquips","miscDyes"}) {
                const int offset=il2cpp::fieldOffset(player,fieldName);
                if(offset>=0)array(runtime::field<Il2CppArray*>(objects[i],offset),false);
            }
        }
    }
    array(static_cast<Il2CppArray*>(getStatic("item")),true);
    refresh(static_cast<Il2CppObject*>(getStatic("mouseItem")));
    // ChestItem no mobile e um struct compacto, NAO Item[]. O bau/banco
    // cria Item via SetDefaults ao abrir; nunca reinterpretar esses structs.
    BL_INFO("csharp items: %d instancia(s) existentes reidratadas sem alterar pilha/favorito",count);
}
#endif
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
        *error="[CSharp][ERROR][NativeItem] Mod="+modId+" Stage=ApplyDefaults Expected=256 Actual="+
            std::to_string(entries.size())+" Result=Rejected Reason=ContentCountExceeded";return false;
    }
    if(!prepare(error))return false;
    std::vector<BasicItemEntry> resolved=entries;
    for(auto& entry:resolved) {
        if(!definitions.contains(entry.type)||runtime::modItemKey(entry.type)!=modId+"/"+entry.name||
           !validateBasicItem(entry,error)||!fitIntegerFields(entry,error)||!cacheSound(entry.useSound,error)) {if(error->empty())*error=entry.name+": item nao pertence ao mod";return false;}
#if !BL_HAVE_QUICKJS
        if(!entry.recipes.empty()||!entry.behavior.empty()) {*error=entry.name+": host compilado sem bridge QuickJS de receitas";return false;}
#endif
        for(auto& recipe:entry.recipes)for(auto& ingredient:recipe.ingredients) {
            if(ingredient.key.front()=='m') {
                auto key=ingredient.key.substr(1);
                if(key.find('/')==std::string::npos)key=modId+"/"+key;
                ingredient.type=runtime::modItemTypeByKey(key);
                if(ingredient.type<0){*error=entry.name+": ingrediente nao carregado: "+key;return false;}
            }
        }
    }
    for(auto& entry:resolved) {
        entry.modDir=definitions.at(entry.type).modDir;
        entry.sourceFile=definitions.at(entry.type).sourceFile;entry.modId=modId;
        if(!entry.behavior.empty())behaviorsPending.store(true,std::memory_order_release);
        runtime::setModItemTooltip(entry.type,{{"",entry.tooltip}});
        for(size_t i=0;i<entry.recipes.size();++i) {
            auto id=modId+"/"+entry.name+"#"+std::to_string(i);
            recipes.emplace(id,PendingBasicRecipe{id,modId,entry.type,-1,entry.recipes[i]});
            recipesPending.store(true,std::memory_order_release);
        }
        definitions[entry.type]=std::move(entry);
    }
    BL_INFO("csharp items: %zu defaults nativos aplicados para %s",entries.size(),modId.c_str());
    instancesPending.store(true,std::memory_order_release);
    return true;
}

bool setBasicItemEquipSlot(int type,const std::string& kind,int slot,std::string* error) {
    std::lock_guard lock(mutex);
    const auto entry=definitions.find(type);
    const std::map<std::string,const char*> fields={
        {"Head","headSlot"},{"Body","bodySlot"},{"Legs","legSlot"},
        {"Beard","beardSlot"},{"Back","backSlot"}};
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
    binding.offset=static_cast<int>(offset);
    if(slot>binding.max)return reject("SlotOutOfRange",actual?actual:"Unknown");
    equipSlots[type][kind]={binding,slot};
    error->clear();
    return true;
}

bool applyBasicItemToObject(Il2CppObject* object,int type) {
    std::lock_guard lock(mutex);const auto found=definitions.find(type);
    if(found==definitions.end()||!object)return false;
    const auto& entry=found->second;
    for(size_t i=0;i<integerFields.size();++i) {
        const auto& field=integerFields[i];auto* destination=reinterpret_cast<unsigned char*>(object)+field.offset;
        // Converter antes de copiar: jamais escrever quatro bytes em Byte/Int16.
        switch(field.bytes) {
            case 1: {const auto value=static_cast<uint8_t>(entry.values[i]);std::memcpy(destination,&value,1);break;}
            case 2: {const auto value=static_cast<uint16_t>(entry.values[i]);std::memcpy(destination,&value,2);break;}
            case 4: {const auto value=static_cast<uint32_t>(entry.values[i]);std::memcpy(destination,&value,4);break;}
        }
    }
    for(size_t i=0;i<flagOffsets.size();++i)runtime::field<uint8_t>(object,flagOffsets[i])=!!(entry.values[BasicFlags]&(1<<i));
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
    std::memset(destination,0,static_cast<size_t>(soundSize));
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
bool basicItemIsAccessory(int type) {std::lock_guard lock(mutex);auto it=definitions.find(type);return it!=definitions.end()&&(it->second.values[BasicFlags]&32);}
std::vector<BasicItemEntry> pendingBasicItemBehaviors() {
    std::lock_guard lock(mutex);std::vector<BasicItemEntry> out;
    for(const auto& [type,entry]:definitions)if(!entry.behavior.empty()&&!entry.behaviorInstalled){out.push_back(entry);if(out.size()==8)break;}
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
bool basicItemHasDamage(int type) {std::lock_guard lock(mutex);auto it=definitions.find(type);return it!=definitions.end()&&it->second.values[BasicDamage]>0;}
bool isBasicItem(int type) {std::lock_guard lock(mutex);return definitions.contains(type);}
std::vector<int> basicItemTypes() {std::lock_guard lock(mutex);std::vector<int> out;for(const auto& p:definitions)out.push_back(p.first);return out;}
std::vector<PendingBasicRecipe> pendingBasicRecipes() {std::lock_guard lock(mutex);std::vector<PendingBasicRecipe> out;for(const auto& p:recipes)if(p.second.index<0){out.push_back(p.second);if(out.size()==8)break;}return out;}
bool acknowledgeBasicRecipe(const std::string& id,int index) {
    std::lock_guard lock(mutex);auto it=recipes.find(id);
    if(it==recipes.end()||index<0||it->second.index>=0)return false;
    it->second.index=index;
    bool pending=false;for(const auto& p:recipes)if(p.second.index<0){pending=true;break;}
    recipesPending.store(pending,std::memory_order_release);return true;
}
void pumpBasicRecipesOnGameThread() {
#if BL_HAVE_QUICKJS
    if(instancesPending.exchange(false,std::memory_order_acq_rel))refreshLiveBasicItems();
    static unsigned behaviorTicks=0;
    if(behaviorsPending.load(std::memory_order_acquire)&&(behaviorTicks++%120u)==0) {
        script::JsLock lock(0);if(!lock.held()||!script::engine().ready())return;
        auto* ctx=static_cast<JSContext*>(script::engine().context());
        auto global=JS_GetGlobalObject(ctx);auto function=JS_GetPropertyStr(ctx,global,"__blSyncCSharpItemBehaviors");
        if(JS_IsFunction(ctx,function)) {
            auto result=JS_Call(ctx,function,global,0,nullptr);
            if(JS_IsException(result)) {
                auto exception=JS_GetException(ctx);const char* text=JS_ToCString(ctx,exception);
                static unsigned failures=0;if(failures++<8)BL_ERROR("csharp item behavior: %s",text?text:"falha sem mensagem");
                if(text)JS_FreeCString(ctx,text);
                JS_FreeValue(ctx,exception);
            }
            JS_FreeValue(ctx,result);
        }
        JS_FreeValue(ctx,function);JS_FreeValue(ctx,global);

    }
    if(!recipesPending.load(std::memory_order_acquire))return;
    static unsigned ticks=0;if((ticks++%120u)!=0)return;
    script::JsLock lock(0);if(!lock.held()||!script::engine().ready())return;
    auto* ctx=static_cast<JSContext*>(script::engine().context());
    auto global=JS_GetGlobalObject(ctx);
    auto function=JS_GetPropertyStr(ctx,global,"__blSyncCSharpRecipes");
    if(JS_IsFunction(ctx,function)) {
        auto result=JS_Call(ctx,function,global,0,nullptr);
        if(JS_IsException(result)) {
            auto exception=JS_GetException(ctx);const char* text=JS_ToCString(ctx,exception);
            static unsigned errors=0;if(errors++<8)BL_ERROR("csharp recipes: %s",text?text:"falha sem mensagem");
            if(text)JS_FreeCString(ctx,text);
            JS_FreeValue(ctx,exception);
        }
        JS_FreeValue(ctx,result);
    }
    JS_FreeValue(ctx,function);JS_FreeValue(ctx,global);
#endif
}
}
