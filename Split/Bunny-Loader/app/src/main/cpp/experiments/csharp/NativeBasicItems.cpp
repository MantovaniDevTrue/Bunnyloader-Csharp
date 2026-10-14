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
std::array<int,BasicFieldCount-1> offsets{};
std::array<int,5> flagOffsets{};
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
       !api.class_get_fields||!api.field_get_flags||!api.field_get_offset||
       !api.class_value_size||!api.class_is_valuetype||
       !api.runtime_class_init||!api.field_static_get_value) {*error="BunnyItem: contrato de campos IL2CPP indisponivel";return false;}
    const char* names[]={"width","height","maxStack","value","rare","damage","crit",
        "useTime","useAnimation","useStyle","pick","axe","hammer"};
    for(size_t i=0;i<offsets.size();++i) {
        auto* field=il2cpp::findField(item,names[i]);
        auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
        if(!field||!cls||std::strcmp(api.class_get_name(cls),"Int32")!=0) {*error=std::string("BunnyItem: campo int ausente/incompativel: ")+names[i];return false;}
        offsets[i]=il2cpp::fieldOffset(item,names[i]);
    }
    const char* flags[]={"melee","noMelee","autoReuse","useTurn","consumable"};
    for(size_t i=0;i<flagOffsets.size();++i) {
        auto* field=il2cpp::findField(item,flags[i]);
        auto* cls=field?api.class_from_il2cpp_type(api.field_get_type(field)):nullptr;
        if(!field||!cls||std::strcmp(api.class_get_name(cls),"Boolean")!=0) {*error=std::string("BunnyItem: campo bool ausente/incompativel: ")+flags[i];return false;}
        flagOffsets[i]=il2cpp::fieldOffset(item,flags[i]);
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
    const int activeItem=il2cpp::fieldOffset(item,"active");
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

bool registerBasicItemPlaceholder(int type,const std::string& name,std::string* error) {
    std::lock_guard lock(mutex);
    if(!prepare(error))return false;
    BasicItemEntry item;item.type=type;item.name=name;
    return definitions.emplace(type,std::move(item)).second;
}

bool applyNativeBasicItemDefaults(const std::string& modId,const std::vector<BasicItemEntry>& entries,std::string* error) {
    if(entries.empty())return true; // mods anteriores nao dependem deste contrato novo
    std::lock_guard lock(mutex);
    if(entries.size()>16||!prepare(error))return false;
    std::vector<BasicItemEntry> resolved=entries;
    for(auto& entry:resolved) {
        if(!definitions.contains(entry.type)||runtime::modItemKey(entry.type)!=modId+"/"+entry.name||
           !validateBasicItem(entry,error)||!cacheSound(entry.useSound,error)) {if(error->empty())*error=entry.name+": item nao pertence ao mod";return false;}
#if !BL_HAVE_QUICKJS
        if(!entry.recipes.empty()) {*error=entry.name+": host compilado sem bridge QuickJS de receitas";return false;}
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

bool applyBasicItemToObject(Il2CppObject* object,int type) {
    std::lock_guard lock(mutex);const auto found=definitions.find(type);
    if(found==definitions.end()||!object)return false;
    const auto& entry=found->second;
    for(size_t i=0;i<offsets.size();++i)runtime::field<int32_t>(object,offsets[i])=entry.values[i];
    for(size_t i=0;i<flagOffsets.size();++i)runtime::field<uint8_t>(object,flagOffsets[i])=!!(entry.values[BasicFlags]&(1<<i));
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
