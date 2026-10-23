#include "experiments/csharp/NativeArmorPolicies.h"
#include "experiments/csharp/NativeStaticValue.h"
#include "content/items/ModItems.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <set>

namespace bl::csharp {
namespace armor_native {
struct Write { int offset=-1,bytes=4; bool integer=false,sign=true,multiply=false; float value=0; int64_t min=INT32_MIN,max=INT32_MAX; };
struct Policy { std::vector<Write> writes; std::vector<int> immune; int immuneOffset=-1; Il2CppClass* immuneClass=nullptr; };
struct Entry { NativeArmorPolicyStatus status; BasicArmorPolicy source; Policy piece,bonus; std::array<int,3> ids{}; };
struct Snapshot { std::map<int,Policy> pieces; std::map<std::array<int,3>,std::vector<Policy>> sets; std::set<std::array<int,3>> suppressPumpkin; };
std::shared_ptr<const Snapshot> snapshot;
std::mutex mutex;
std::map<int,Entry> entries;
std::atomic<bool> pending{false},haveSets{false};
int itemType=-1,playerArmor=-1;Il2CppClass* armorClass=nullptr;
using GrantFn=void (*)(Il2CppObject*,Il2CppObject*,const MethodInfo*);
using FunctionalFn=void (*)(Il2CppObject*,int32_t,Il2CppObject*,const MethodInfo*);
using UpdateFn=void (*)(Il2CppObject*,int32_t,const MethodInfo*);
using PumpkinFn=void (*)(Il2CppObject*,const MethodInfo*);
GrantFn originalGrant=nullptr;FunctionalFn originalFunctional=nullptr;
UpdateFn originalUpdate=nullptr;PumpkinFn originalPumpkin=nullptr;
bool grantHooked=false,functionalHooked=false,updateHooked=false,pumpkinHooked=false;

bool primitive(Il2CppClass* type,const char* name) {
    auto& a=il2cpp::api();const char* ns=type?a.class_get_namespace(type):nullptr,*actual=type?a.class_get_name(type):nullptr;
    return ns&&actual&&std::strcmp(ns,"System")==0&&std::strcmp(actual,name)==0&&a.class_is_valuetype(type);
}
bool bounds(Il2CppClass* type,size_t offset,size_t bytes) {
    const auto size=il2cpp::api().class_instance_size(type);
    return offset>=sizeof(Il2CppObject)&&bytes<=size&&offset<=size-bytes&&offset<=INT32_MAX;
}
bool intContract(Il2CppClass* type,Write* w) {
    struct Kind { const char* name; int bytes; bool sign; int64_t min,max; };
    for(const auto& k:std::initializer_list<Kind>{{"SByte",1,true,-128,127},{"Byte",1,false,0,255},
        {"Int16",2,true,-32768,32767},{"UInt16",2,false,0,65535},{"Int32",4,true,INT32_MIN,INT32_MAX},{"UInt32",4,false,0,UINT32_MAX}}) {
        uint32_t alignment=0;
        if(primitive(type,k.name)&&il2cpp::api().class_value_size(type,&alignment)==k.bytes) {
            w->bytes=k.bytes;w->sign=k.sign;w->min=k.min;w->max=k.max;return true;
        }
    }
    return false;
}
bool fieldBinding(Il2CppClass* cls,const std::string& name,bool integer,Write* out,std::string* error) {
    auto& a=il2cpp::api();auto* f=cls?il2cpp::findField(cls,name):nullptr;
    auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;uint32_t align=0;
    if(!f||(a.field_get_flags(f)&0x10u)||(integer?!intContract(type,out):
        (!primitive(type,"Single")||a.class_value_size(type,&align)!=4))) {
        *error="Field="+name+" Reason=NativeTypeMismatch";return false;
    }
    const auto offset=a.field_get_offset(f);
    if(!bounds(cls,offset,static_cast<size_t>(out->bytes))){*error="Field="+name+" Reason=InvalidFieldBounds";return false;}
    out->offset=static_cast<int>(offset);out->integer=integer;return true;
}
bool referenceBinding(Il2CppClass* cls,const char* name,Il2CppClass* element,int* offset,Il2CppClass** array,std::string* error) {
    auto& a=il2cpp::api();auto* f=cls?il2cpp::findField(cls,name):nullptr;
    auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;
    if(!f||(a.field_get_flags(f)&0x10u)||!type||a.class_is_valuetype(type)||a.class_get_rank(type)!=1||a.class_get_element_class(type)!=element||
       !bounds(cls,a.field_get_offset(f),sizeof(void*))) {
        *error=std::string("Field=")+name+" Reason=InvalidArrayContract";return false;
    }
    *offset=static_cast<int>(a.field_get_offset(f));*array=type;return true;
}
bool constant(const char* cls,const std::string& name,int* value) {
    auto* type=il2cpp::findClass({"Terraria.ID",cls,{}});int64_t resolved=0;
    if(!native_static::readInteger(native_static::resolve(type,name.c_str()),&resolved)||resolved<0||resolved>65535)return false;
    *value=static_cast<int>(resolved);return true;
}
bool resolvePolicy(const BasicArmorEffects& source,Il2CppClass* player,Policy* out,std::string* error) {
    for(const auto& e:source.effects) {
        Write write;write.value=e.value;write.multiply=e.operation=="mul";
        if(!fieldBinding(player,e.field,e.operation=="int",&write,error))return false;
        out->writes.push_back(write);
    }
    if(!source.immune.empty()) {
        auto* boolean=il2cpp::findClass({"System","Boolean",{}});uint32_t align=0;auto& a=il2cpp::api();
        if(!primitive(boolean,"Boolean")||a.class_value_size(boolean,&align)!=1||
           !referenceBinding(player,"buffImmune",boolean,&out->immuneOffset,&out->immuneClass,error))return false;
        for(const auto& name:source.immune) {
            int id=-1;if(!constant("BuffID",name,&id)){*error="BuffID="+name+" Reason=UnknownBuff";return false;}
            out->immune.push_back(id);
        }
    }
    return true;
}
int64_t readInteger(const unsigned char* p,const Write& w) {
    if(w.bytes==1){if(w.sign){int8_t v;std::memcpy(&v,p,1);return v;}uint8_t v;std::memcpy(&v,p,1);return v;}
    if(w.bytes==2){if(w.sign){int16_t v;std::memcpy(&v,p,2);return v;}uint16_t v;std::memcpy(&v,p,2);return v;}
    if(w.sign){int32_t v;std::memcpy(&v,p,4);return v;}uint32_t v;std::memcpy(&v,p,4);return v;
}
bool apply(const Policy& policy,Il2CppObject* player) {
    if(!player)return false;
    Il2CppArray* immune=nullptr;
    if(!policy.immune.empty()) {
        std::memcpy(&immune,reinterpret_cast<unsigned char*>(player)+policy.immuneOffset,sizeof(immune));
        if(!immune||immune->obj.klass!=policy.immuneClass||immune->length>65536)return false;
        for(int id:policy.immune)if(static_cast<uintptr_t>(id)>=immune->length)return false;
    }
    std::array<float,16> floats{};std::array<int64_t,16> integers{};
    if(policy.writes.size()>floats.size())return false;
    for(size_t i=0;i<policy.writes.size();++i) {
        const auto& w=policy.writes[i];const auto* p=reinterpret_cast<unsigned char*>(player)+w.offset;
        if(w.integer) {
            int64_t value=readInteger(p,w);
            for(size_t j=i;j>0;--j)if(policy.writes[j-1].offset==w.offset){value=integers[j-1];break;}
            integers[i]=value+static_cast<int64_t>(w.value);
            if(integers[i]<w.min||integers[i]>w.max)return false;
        } else {
            float value=0;std::memcpy(&value,p,4);
            for(size_t j=i;j>0;--j)if(policy.writes[j-1].offset==w.offset){value=floats[j-1];break;}
            floats[i]=w.multiply?value*w.value:value+w.value;
            if(!std::isfinite(value)||!std::isfinite(floats[i]))return false;
        }
    }
    for(size_t i=0;i<policy.writes.size();++i) {
        auto* p=reinterpret_cast<unsigned char*>(player)+policy.writes[i].offset;
        if(policy.writes[i].integer) {
            const uint32_t value=static_cast<uint32_t>(integers[i]);std::memcpy(p,&value,static_cast<size_t>(policy.writes[i].bytes));
        } else std::memcpy(p,&floats[i],4);
    }
    if(immune)for(int id:policy.immune)static_cast<unsigned char*>(arrayData(immune))[id]=1;
    return true;
}
std::array<int,3> equipped(Il2CppObject* player) {
    std::array<int,3> ids{};if(!player||playerArmor<0)return ids;
    Il2CppArray* array=nullptr;std::memcpy(&array,reinterpret_cast<unsigned char*>(player)+playerArmor,sizeof(array));
    if(!array||array->obj.klass!=armorClass||array->length<3||array->length>64)return ids;
    auto** pieces=static_cast<Il2CppObject**>(arrayData(array));
    for(size_t i=0;i<ids.size();++i)if(pieces[i])std::memcpy(&ids[i],reinterpret_cast<unsigned char*>(pieces[i])+itemType,4);
    return ids;
}
void piece(Il2CppObject* player,Il2CppObject* item) {
    if(!player||!item)return;
    const auto data=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);if(!data)return;
    int type=0;std::memcpy(&type,reinterpret_cast<unsigned char*>(item)+itemType,4);
    const auto found=data->pieces.find(type);if(found!=data->pieces.end())apply(found->second,player);
}
void grant(Il2CppObject* player,Il2CppObject* item,const MethodInfo* method) {
    if(originalGrant)originalGrant(player,item,method);
    piece(player,item);
}
void functional(Il2CppObject* player,int32_t slot,Il2CppObject* item,const MethodInfo* method) {
    if(originalFunctional)originalFunctional(player,slot,item,method);
    piece(player,item);
}
void update(Il2CppObject* player,int32_t index,const MethodInfo* method) {
    if(originalUpdate)originalUpdate(player,index,method);
    const auto data=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);if(!data)return;
    const auto found=data->sets.find(equipped(player));if(found!=data->sets.end())for(const auto& p:found->second)apply(p,player);
}
void pumpkin(Il2CppObject* player,const MethodInfo* method) {
    const auto data=std::atomic_load_explicit(&snapshot,std::memory_order_acquire);
    if(data&&data->suppressPumpkin.contains(equipped(player)))return;
    if(originalPumpkin)originalPumpkin(player,method);
}
bool prepare(bool needPiece,bool needSet,std::string* error) {
    auto& a=il2cpp::api();
    if(!a.class_get_name||!a.class_get_namespace||!a.class_is_valuetype||!a.class_value_size||!a.class_instance_size||!a.field_get_flags||
       !a.field_get_offset||!a.field_get_type||!a.class_from_il2cpp_type||!a.field_static_get_value||!a.class_get_rank||!a.class_get_element_class||!a.method_is_instance) {
        *error="Reason=MetadataUnavailable";return false;
    }
    auto* player=il2cpp::findClass({"Terraria","Player",{}});auto* item=il2cpp::findClass({"Terraria","Item",{}});
    Write binding;if(!fieldBinding(item,"type",true,&binding,error))return false;
    if(binding.bytes!=4||!binding.sign){*error="Field=type Reason=NativeTypeMismatch";return false;}
    if(itemType<0)itemType=binding.offset;
    else if(itemType!=binding.offset){*error="Field=type Reason=NativeLayoutChanged";return false;}
    if(needSet&&playerArmor<0&&!referenceBinding(player,"armor",item,&playerArmor,&armorClass,error))return false;
    if(needPiece&&!grantHooked) {
        const auto* method=il2cpp::findMethodBySignature(player,il2cpp::parseSignature("void GrantArmorBenefits(Item armorPiece)"));
        if(!method||!a.method_is_instance(method)||!hook::install(method,grant,&originalGrant)){*error="Reason=GrantHookUnavailable";return false;}grantHooked=true;
    }
    if(needPiece&&!functionalHooked) {
        const auto* method=il2cpp::findMethodBySignature(player,il2cpp::parseSignature("void ApplyEquipFunctional(int itemSlot, Item currentItem)"));
        if(!method||!a.method_is_instance(method)||!hook::install(method,functional,&originalFunctional)){*error="Reason=FunctionalHookUnavailable";return false;}functionalHooked=true;
    }
    // Set hooks are installed with the UI registry on the game thread.
    return true;
}
struct Root {
    uint32_t handle=0;
    Root()=default;explicit Root(Il2CppObject* value){if(value)handle=il2cpp::api().gchandle_new(value,false);}
    ~Root(){if(handle)il2cpp::api().gchandle_free(handle);}
    Root(const Root&)=delete;Root& operator=(const Root&)=delete;
    Il2CppObject* get() const {return handle?il2cpp::api().gchandle_get_target(handle):nullptr;}
};
struct UiRecord { std::array<int,3> ids{};std::string text;std::unique_ptr<Root> value; bool added=false;uintptr_t index=0; };
std::map<std::array<int,3>,UiRecord> ui;
FieldInfo* lookupField=nullptr;FieldInfo* allField=nullptr;
const MethodInfo* arrayGet=nullptr;const MethodInfo* arraySet=nullptr;const MethodInfo* addSet=nullptr;
Il2CppArray* lastLookup=nullptr;
Il2CppObject* invoke(const MethodInfo* method,void* object,void** args,bool* ok) {
    Il2CppObject* exception=nullptr;auto* result=il2cpp::api().runtime_invoke(method,object,args,&exception);*ok=!exception;return result;
}
Il2CppObject* get(Il2CppArray* array,int index,bool* ok) {
    if(!array||index<0||static_cast<uintptr_t>(index)>=array->length){*ok=false;return nullptr;}
    void* args[]={&index};return invoke(arrayGet,array,args,ok);
}
bool set(Il2CppArray* array,int index,Il2CppObject* value) {
    if(!array||index<0||static_cast<uintptr_t>(index)>=array->length)return false;
    void* args[]={value,&index};bool ok=false;invoke(arraySet,array,args,&ok);return ok;
}
bool boxedField(Il2CppObject* object,const char* name,size_t width,size_t* offset) {
    auto& a=il2cpp::api();auto* cls=a.object_get_class(object);auto* f=cls?il2cpp::findField(cls,name):nullptr;
    if(!f||(a.field_get_flags(f)&0x10u))return false;
    auto* fieldType=a.class_from_il2cpp_type(a.field_get_type(f));
    if(std::strcmp(name,"Effect")==0) {
        if(!fieldType||a.class_is_valuetype(fieldType))return false;
    } else if(!primitive(fieldType,"Int32"))return false;
    if(a.class_is_valuetype(cls)) {
        il2cpp::ValueLayout layout;std::string error;size_t value=0;
        if(!il2cpp::resolveValueLayout(cls,&layout,&error)||!layout.offset(a.field_get_offset(f),width,&value))return false;
        *offset=sizeof(Il2CppObject)+value;return true;
    }
    const auto raw=a.field_get_offset(f);if(!bounds(cls,raw,width))return false;*offset=raw;return true;
}
Il2CppArray* allSets(bool* ok) {
    auto& a=il2cpp::api();Il2CppObject* list=nullptr;a.field_static_get_value(allField,&list);
    if(!list){*ok=false;return nullptr;}
    const auto* method=a.class_get_method_from_name(a.object_get_class(list),"ToArray",0);
    if(!method||!a.method_is_instance(method)){*ok=false;return nullptr;}
    auto* result=invoke(method,list,nullptr,ok);
    auto* array=reinterpret_cast<Il2CppArray*>(result);
    if(!*ok||!array||a.class_get_rank(a.object_get_class(result))!=1||array->length>65536){*ok=false;return nullptr;}
    return array;
}
bool prepareUi(std::string* error) {
    auto& a=il2cpp::api();
    if(!a.runtime_invoke||!a.object_get_class||!a.class_get_method_from_name||!a.array_new||!a.string_new||
       !a.gchandle_new||!a.gchandle_get_target||!a.gchandle_free||!a.field_static_set_value||!a.method_get_param||!a.class_is_enum) {
        *error="Reason=RegistryExportsUnavailable";return false;
    }
    auto* bonuses=il2cpp::findClass({"Terraria.DataStructures","ArmorSetBonuses",{}});
    auto* benefits=il2cpp::findClass({"Terraria.DataStructures","ArmorSetBonuses",{"Benefits"}});
    auto* arrays=il2cpp::findClass({"System","Array",{}});auto* player=il2cpp::findClass({"Terraria","Player",{}});
    if(!bonuses||!benefits||!arrays){*error="Reason=ArmorRegistryNotReady";return false;}
    lookupField=il2cpp::findField(bonuses,"SetsContaining");allField=il2cpp::findField(bonuses,"All");
    if(!lookupField||!allField||!(a.field_get_flags(lookupField)&0x10u)||!(a.field_get_flags(allField)&0x10u)) {
        *error="Reason=ArmorRegistryNotReady";return false;
    }
    arrayGet=il2cpp::findMethodBySignature(arrays,il2cpp::parseSignature("object GetValue(int index)"));
    arraySet=il2cpp::findMethodBySignature(arrays,il2cpp::parseSignature("void SetValue(object value, int index)"));
    addSet=il2cpp::findMethodBySignature(bonuses,il2cpp::parseSignature("void Add(ArmorSetEffect Effect, string TextKey, PartType PrimaryPart, int Head, int Body, int Legs)"));
    if(!arrayGet||!arraySet||!addSet){*error="Reason=ArmorRegistryMethodUnavailable";return false;}
    auto* partType=a.class_from_il2cpp_type(a.method_get_param(addSet,2));uint32_t alignment=0;
    const int partBytes=partType&&a.class_is_enum(partType)?a.class_value_size(partType,&alignment):0;
    if(!a.method_is_instance(arrayGet)||!a.method_is_instance(arraySet)||a.method_is_instance(addSet)||
       (partBytes!=1&&partBytes!=2&&partBytes!=4&&partBytes!=8)) {
        *error="Reason=ArmorRegistryAbiMismatch";return false;
    }
    if(!pumpkinHooked) {
        const auto* method=il2cpp::findMethodBySignature(benefits,il2cpp::parseSignature("void Pumpkin(Player player)"));
        if(!method||a.method_is_instance(method)||!hook::install(method,pumpkin,&originalPumpkin)){*error="Reason=PumpkinHookUnavailable";return false;}pumpkinHooked=true;
    }
    if(!updateHooked) {
        const auto* method=il2cpp::findMethodBySignature(player,il2cpp::parseSignature("void UpdateArmorSets(int i)"));
        if(!method||!a.method_is_instance(method)||!hook::install(method,update,&originalUpdate)){*error="Reason=ArmorSetHookUnavailable";return false;}updateHooked=true;
    }
    return true;
}
bool localizedTextReady(const std::string& key,std::string* error) {
    auto& a=il2cpp::api();
    auto* language=il2cpp::findClass({"Terraria.Localization","Language",{}});
    const auto* exists=language?il2cpp::findMethodBySignature(language,
        il2cpp::parseSignature("bool Exists(string key)")):nullptr;
    auto* resultType=exists&&a.method_get_return_type?
        a.class_from_il2cpp_type(a.method_get_return_type(exists)):nullptr;
    uint32_t alignment=0;
    if(!exists||a.method_is_instance(exists)||!primitive(resultType,"Boolean")) {
        *error="Reason=ArmorLocalizationContractUnavailable";return false;
    }
    if(a.class_value_size(resultType,&alignment)!=1) {
        *error="Reason=ArmorLocalizationContractUnavailable";return false;
    }
    Root argument(reinterpret_cast<Il2CppObject*>(a.string_new(key.c_str())));
    if(!argument.get()){*error="Reason=TextAllocationFailed";return false;}
    void* args[]={argument.get()};bool ok=false;
    Root result(invoke(exists,nullptr,args,&ok));
    if(!ok||!result.get()||a.object_get_class(result.get())!=resultType) {
        *error="Reason=ArmorLocalizationLookupFailed";return false;
    }
    // A missing GetText key can return an unregistered LocalizedText. Waiting
    // for Exists prevents retaining that orphan across future language reloads.
    if(!*reinterpret_cast<const uint8_t*>(result.get()+1)) {
        *error="Reason=ArmorLocalizationKeyPending";return false;
    }
    return true;
}
bool installUi(const Entry& entry,std::string* error) {
    auto& a=il2cpp::api();bool ok=false;
    Il2CppArray* lookup=nullptr;a.field_static_get_value(lookupField,&lookup);
    if(!lookup||lookup->length>65536||lookup->length==0){*error="Reason=ArmorLookupPending";return false;}
    auto& record=ui[entry.ids];
    const std::string textKey=entry.source.textKey.empty()?entry.source.text:
        "Mods."+entry.status.mod+"."+entry.source.textKey;
    if(!record.text.empty()&&record.text!=textKey){*error="Reason=ConflictingSetText";return false;}
    record.ids=entry.ids;record.text=textKey;
    const auto last=*std::max_element(entry.ids.begin(),entry.ids.end());
    auto* outerClass=a.object_get_class(reinterpret_cast<Il2CppObject*>(lookup));
    auto* rowClass=a.class_get_element_class(outerClass);auto* element=rowClass?a.class_get_element_class(rowClass):nullptr;
    if(a.class_get_rank(outerClass)!=1||!rowClass||a.class_get_rank(rowClass)!=1||!element){*error="Reason=InvalidSetLookupType";return false;}
    if(!record.value&&!record.added) {
        if(!entry.source.textKey.empty()&&!localizedTextReady(textKey,error))return false;
        Root all(reinterpret_cast<Il2CppObject*>(allSets(&ok)));if(!ok||!all.get()){*error="Reason=ArmorAllPending";return false;}
        int pumpkinId=-1;if(!constant("ItemID","PumpkinHelmet",&pumpkinId)){*error="Reason=PumpkinIdMissing";return false;}
        Root* delegate=nullptr;std::unique_ptr<Root> found;
        auto* values=reinterpret_cast<Il2CppArray*>(all.get());
        for(uintptr_t i=0;i<values->length;++i) {
            Root value(get(values,static_cast<int>(i),&ok));if(!ok||!value.get())continue;
            size_t headOffset=0,effectOffset=0;int head=0;
            if(!boxedField(value.get(),"Head",4,&headOffset)||!boxedField(value.get(),"Effect",sizeof(void*),&effectOffset))continue;
            std::memcpy(&head,reinterpret_cast<unsigned char*>(value.get())+headOffset,4);
            if(head!=pumpkinId)continue;
            Il2CppObject* effect=nullptr;std::memcpy(&effect,reinterpret_cast<unsigned char*>(value.get())+effectOffset,sizeof(effect));
            if(effect&&a.object_get_class(effect)==a.class_from_il2cpp_type(a.method_get_param(addSet,0))){found=std::make_unique<Root>(effect);delegate=found.get();break;}
        }
        if(!delegate||!delegate->get()){*error="Reason=PumpkinDelegatePending";return false;}
        Root text(reinterpret_cast<Il2CppObject*>(a.string_new(textKey.c_str())));
        if(!text.get()){*error="Reason=TextAllocationFailed";return false;}
        uint64_t primary=0;int head=entry.ids[0],body=entry.ids[1],legs=entry.ids[2];
        void* args[]={delegate->get(),text.get(),&primary,&head,&body,&legs};
        record.index=values->length;
        invoke(addSet,nullptr,args,&ok);record.added=true;
        if(!ok){*error="Reason=NativeSetAddFailed";return false;}
    }
    if(!record.value) {
        Root after(reinterpret_cast<Il2CppObject*>(allSets(&ok)));auto* list=reinterpret_cast<Il2CppArray*>(after.get());
        if(!ok||!list){*error="Reason=NativeSetResultMissing";return false;}
        if(list->length<=record.index){record.added=false;*error="Reason=NativeSetResultMissing";return false;}
        auto* created=get(list,static_cast<int>(record.index),&ok);
        if(!ok||!created){*error="Reason=NativeSetResultMissing";return false;}
        Root candidate(created);std::array<int,3> actual{};
        for(size_t i=0;i<3;++i) {
            size_t offset=0;
            if(!candidate.get()||!boxedField(candidate.get(),std::array<const char*,3>{"Head","Body","Legs"}[i],4,&offset)) {
                *error="Reason=InvalidCreatedSet";return false;
            }
            std::memcpy(&actual[i],reinterpret_cast<unsigned char*>(candidate.get())+offset,4);
        }
        if(actual!=entry.ids){*error="Reason=CreatedSetIdentityMismatch";return false;}
        record.value=std::make_unique<Root>(created);if(!record.value->get()){record.value.reset();*error="Reason=SetRootFailed";return false;}
    }
    // Use managed Array.GetValue/SetValue so reference and value arrays get the
    // correct boxing and GC barriers. No per-frame boxing occurs.
    if(last>=static_cast<int>(lookup->length)) {
        Root grown(reinterpret_cast<Il2CppObject*>(a.array_new(rowClass,static_cast<uintptr_t>(last+1))));
        if(!grown.get()){*error="Reason=LookupAllocationFailed";return false;}
        auto* next=reinterpret_cast<Il2CppArray*>(grown.get());
        for(uintptr_t i=0;i<lookup->length;++i) {
            Root value(get(lookup,static_cast<int>(i),&ok));if(!ok||!set(next,static_cast<int>(i),value.get())){*error="Reason=LookupCopyFailed";return false;}
        }
        Root empty(reinterpret_cast<Il2CppObject*>(a.array_new(element,0)));
        if(!empty.get()){*error="Reason=EmptyRowAllocationFailed";return false;}
        for(uintptr_t i=lookup->length;i<next->length;++i)if(!set(next,static_cast<int>(i),empty.get()))return false;
        a.field_static_set_value(lookupField,grown.get());a.field_static_get_value(lookupField,&lookup);
        if(lookup!=next){*error="Reason=LookupPublishFailed";return false;}
    }
    for(int type:entry.ids) {
        Root old(get(lookup,type,&ok));if(!ok){*error="Reason=LookupReadFailed";return false;}
        auto* row=reinterpret_cast<Il2CppArray*>(old.get());
        if(row&&(a.object_get_class(old.get())!=rowClass||row->length>4096)){*error="Reason=InvalidSetRow";return false;}
        bool contains=false;
        if(row)for(uintptr_t i=0;i<row->length;++i) {
            Root value(get(row,static_cast<int>(i),&ok));if(!ok)return false;
            if(value.get()==record.value->get()){contains=true;break;}
            std::array<int,3> ids{};bool matches=value.get()!=nullptr;
            for(size_t k=0;k<3&&matches;++k) {size_t off=0;matches=boxedField(value.get(),std::array<const char*,3>{"Head","Body","Legs"}[k],4,&off);if(matches)std::memcpy(&ids[k],reinterpret_cast<unsigned char*>(value.get())+off,4);}
            if(matches&&ids==entry.ids){contains=true;break;}
        }
        if(contains)continue;
        const auto size=row?row->length:0;
        Root grown(reinterpret_cast<Il2CppObject*>(a.array_new(element,size+1)));if(!grown.get()){*error="Reason=SetRowAllocationFailed";return false;}
        auto* next=reinterpret_cast<Il2CppArray*>(grown.get());
        for(uintptr_t i=0;i<size;++i){Root value(get(row,static_cast<int>(i),&ok));if(!ok||!set(next,static_cast<int>(i),value.get()))return false;}
        if(!set(next,static_cast<int>(size),record.value->get())||!set(lookup,type,grown.get())){*error="Reason=SetRowPublishFailed";return false;}
    }
    lastLookup=lookup;return true;
}
void publish() {
    auto next=std::make_shared<Snapshot>();bool waiting=false,sets=false;
    for(const auto& [type,entry]:entries) {
        const auto registered=ui.find(entry.ids);
        if(entry.source.hasSet&&registered!=ui.end()&&registered->second.added)next->suppressPumpkin.insert(entry.ids);
        if(!entry.status.ready){waiting=true;continue;}
        next->pieces[type]=entry.piece;
        if(entry.source.hasSet){sets=true;next->sets[entry.ids].push_back(entry.bonus);}
    }
    std::atomic_store_explicit(&snapshot,std::shared_ptr<const Snapshot>(std::move(next)),std::memory_order_release);
    pending.store(waiting,std::memory_order_release);haveSets.store(sets,std::memory_order_release);
}
} // namespace armor_native
struct NativeArmorBatch { std::vector<armor_native::Entry> values;std::vector<int> types; };
std::shared_ptr<NativeArmorBatch> prepareNativeArmorPolicies(const std::string& mod,const std::vector<BasicItemEntry>& items,std::string* error) {
    using namespace armor_native;
    auto batch=std::make_shared<NativeArmorBatch>();
    bool pieces=false,sets=false;
    for(const auto& item:items)if(item.armor.present){pieces=pieces||!item.armor.piece.effects.empty()||!item.armor.piece.immune.empty();sets=sets||item.armor.hasSet;}
    if((pieces||sets)&&!prepare(pieces,sets,error))return nullptr;
    for(const auto& item:items) {
        batch->types.push_back(item.type);if(!item.armor.present)continue;
        if(item.armor.hasSet) {
            const auto owner=mod+"/"+item.name;bool contains=false;
            for(const auto& key:{item.armor.head,item.armor.body,item.armor.legs})
                if((key.find('/')==std::string::npos?mod+"/"+key:key)==owner)contains=true;
            if(!contains){*error=item.name+": Stage=BindArmorSet Reason=OwnerNotInSet";return nullptr;}
        }
        Entry entry;entry.source=item.armor;entry.status={item.type,!item.armor.hasSet,mod,item.name,item.armor.hasSet?"waiting-set":"ready",{}};
        auto* player=il2cpp::findClass({"Terraria","Player",{}});
        if(!resolvePolicy(item.armor.piece,player,&entry.piece,error)||!resolvePolicy(item.armor.bonus,player,&entry.bonus,error)) {
            *error="[CSharp][ERROR][Armor] Mod="+mod+" Class="+item.name+" Stage=BindPolicy "+*error;return nullptr;
        }
        batch->values.push_back(std::move(entry));
    }
    return batch;
}
void commitNativeArmorPolicies(const std::shared_ptr<NativeArmorBatch>& batch) {
    using namespace armor_native;if(!batch)return;
    std::lock_guard lock(mutex);
    for(int type:batch->types)entries.erase(type);
    for(const auto& value:batch->values)entries[value.status.type]=value;
    publish();
}
void pumpNativeArmorPoliciesOnGameThread() {
    using namespace armor_native;
    static unsigned ticks=0;++ticks;
    if(!pending.load(std::memory_order_acquire)&&(!haveSets.load(std::memory_order_acquire)||ticks%120u))return;
    std::lock_guard lock(mutex);std::string error;
    if(!prepareUi(&error)) {
        for(auto& [type,e]:entries)if(!e.status.ready){e.status.phase="waiting-registry";e.status.error=error;}
        return;
    }
    Il2CppArray* lookup=nullptr;il2cpp::api().field_static_get_value(lookupField,&lookup);
    if(lookup!=lastLookup)for(auto& [type,entry]:entries)if(entry.source.hasSet)entry.status.ready=false;
    static int cursor=-1;unsigned processed=0;
    auto it=entries.upper_bound(cursor);
    for(size_t scanned=0;scanned<entries.size()&&processed<8;++scanned) {
        if(it==entries.end())it=entries.begin();
        auto& [type,entry]=*it++;cursor=type;
        if(entry.status.ready||!entry.source.hasSet)continue;
        ++processed;error.clear();
        bool resolved=true;std::array<std::string,3> keys{entry.source.head,entry.source.body,entry.source.legs};
        for(size_t i=0;i<3;++i) {
            const auto qualified=keys[i].find('/')==std::string::npos?entry.status.mod+"/"+keys[i]:keys[i];
            entry.ids[i]=runtime::modItemTypeByKey(qualified);
            if(entry.ids[i]<6196||entry.ids[i]>65535)resolved=false;
        }
        if(resolved&&std::find(entry.ids.begin(),entry.ids.end(),type)==entry.ids.end()) {
            error="Reason=OwnerNotInSet";resolved=false;
        }
        if(!resolved) {entry.status.phase="waiting-parts";entry.status.error=error.empty()?"Reason=MissingSetPart":error;continue;}
        error.clear();entry.status.ready=installUi(entry,&error);entry.status.phase=entry.status.ready?"ready":"waiting-registry";
        entry.status.error=error;
    }
    publish();
}
bool nativeArmorPolicyReady(int type) {
    std::lock_guard lock(armor_native::mutex);const auto found=armor_native::entries.find(type);
    return found==armor_native::entries.end()||found->second.status.ready;
}
int pendingNativeArmorPolicies() {
    std::lock_guard lock(armor_native::mutex);int count=0;
    for(const auto& [type,e]:armor_native::entries)if(!e.status.ready)++count;
    return count;
}
std::vector<NativeArmorPolicyStatus> nativeArmorPolicyStatuses() {
    std::lock_guard lock(armor_native::mutex);std::vector<NativeArmorPolicyStatus> out;
    for(const auto& [type,e]:armor_native::entries)out.push_back(e.status);
    return out;
}
} // namespace bl::csharp
