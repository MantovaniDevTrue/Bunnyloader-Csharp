#include "content/recipes/NativeRecipes.h"
#include "content/recipes/RecipeCallbackProvider.h"
#include "content/items/ModItems.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <cstring>
#include <charconv>
#include <atomic>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace bl::runtime {
namespace {
std::string diagnostic(const NativeRecipeRequest& r, const std::string& stage,
                       const std::string& reason, const std::string& expected, const std::string& actual) {
    const bool dependency=reason=="MissingDependency"||reason=="DependencyNotInstalled"||reason=="RecipeSetupNotReady";
    return std::string("[CSharp][")+(dependency?"WARN":"ERROR")+
        "][Recipe] Mod="+r.mod+" Class="+r.name+" File="+r.source+
        " API=BunnyRecipe Recipe="+r.key+" ContentId="+std::to_string(r.result)+
        " Station="+(r.stationKey.empty()?std::to_string(r.station):r.stationKey)+
        " Stage="+stage+" Expected="+expected+" Actual="+actual+" Result=Pending Reason="+reason;
}
bool keyValid(const std::string& key) {
    const auto slash=key.find('/');
    if(key.empty()||key.size()>160||key.front()=='/'||key.back()=='/'||
       (slash!=std::string::npos&&key.find('/',slash+1)!=std::string::npos))return false;
    for(unsigned char c:key)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
        (c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'))return false;
    return true;
}
std::string qualified(const std::string& mod,const std::string& key) {
    return key.find('/')==std::string::npos?mod+"/"+key:key;
}
struct Binding { FieldInfo* field=nullptr; int offset=-1; Il2CppClass* type=nullptr; };
struct Contract {
    Il2CppClass* recipe=nullptr; Il2CppClass* item=nullptr;
    Binding current, count, maximum, table, result, ingredients, itemType, stack, maxStack;
    const MethodInfo* ctor=nullptr; const MethodInfo* defaults=nullptr;
    const MethodInfo* station=nullptr; const MethodInfo* add=nullptr;
    Il2CppClass* variant=nullptr; size_t variantSize=0;
    std::vector<const MethodInfo*> lookups;
    bool ready=false;
} contract;
std::atomic<const RecipeCallbackProvider*> callbackProvider{nullptr};
std::map<std::string,int> staged;
std::map<std::string,int> completed;
bool dirty=false;

std::string typeName(Il2CppClass* cls) {
    auto& a=il2cpp::api();
    if(!cls)return "Missing";
    const char* ns=a.class_get_namespace(cls),*name=a.class_get_name(cls);
    return std::string(ns?ns:"")+"."+(name?name:"<unnamed>");
}
bool bindField(Il2CppClass* cls,const char* name,bool isStatic,const std::string& expected,
               Binding* out,std::string* detail) {
    auto& a=il2cpp::api(); auto* f=cls?il2cpp::findField(cls,name):nullptr;
    auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;
    const auto actual=typeName(type);
    if(!f||actual!=expected||bool(a.field_get_flags(f)&0x10u)!=isStatic) {
        *detail="Field="+std::string(name)+" Expected="+(isStatic?"Static:":"Instance:")+expected+" Actual="+actual;return false;
    }
    const size_t width=expected=="System.Int32"?4:sizeof(void*);
    if(width==4) {
        uint32_t align=0;
        if(!a.class_is_valuetype(type)||a.class_value_size(type,&align)!=4) {*detail="Field="+std::string(name)+" Reason=InvalidIntegerSize";return false;}
    } else if(a.class_is_valuetype(type)) {*detail="Field="+std::string(name)+" Reason=ExpectedReference";return false;}
    int offset=-1;
    if(!isStatic) {
        const size_t size=a.class_instance_size(cls), value=a.field_get_offset(f);
        if(value<sizeof(Il2CppObject)||size<width||value>size-width||value>INT32_MAX) {
            *detail="Field="+std::string(name)+" Offset="+std::to_string(value)+" InstanceSize="+std::to_string(size)+" Reason=InvalidFieldBounds";return false;
        }
        offset=static_cast<int>(value);
    }
    *out={f,offset,type};return true;
}
const MethodInfo* method(Il2CppClass* cls,const char* signature,bool instance,std::string* detail) {
    auto& a=il2cpp::api();
    const auto* m=cls?il2cpp::findMethodBySignature(cls,il2cpp::parseSignature(signature)):nullptr;
    if(!m||a.method_is_instance(m)!=instance) {*detail="Method="+std::string(signature)+" Reason=MissingOrIncompatibleSignature";return nullptr;}
    return m;
}
bool prepare(std::string* error) {
    if(contract.ready)return true;
    auto& a=il2cpp::api();
    if(!a.class_instance_size||!a.gc_wbarrier_set_field||!a.gchandle_new||!a.gchandle_free||
       !a.object_new||!a.runtime_invoke||!a.class_get_rank||!a.class_get_element_class||!a.runtime_class_init) {
        *error="Api=IL2CPP Reason=RequiredExportMissing";return false;
    }
    Contract c;
    c.recipe=il2cpp::findClass({"Terraria","Recipe",{}});c.item=il2cpp::findClass({"Terraria","Item",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    if(!c.recipe||!c.item||!main){*error="Class=Terraria.Recipe/Item/Main Reason=MissingClass";return false;}
    a.runtime_class_init(c.recipe);a.runtime_class_init(c.item);a.runtime_class_init(main);
    if(!bindField(c.recipe,"currentRecipe",true,"Terraria.Recipe",&c.current,error)||
       !bindField(c.recipe,"numRecipes",true,"System.Int32",&c.count,error)||
       !bindField(c.recipe,"maxRecipes",true,"System.Int32",&c.maximum,error)||
       !bindField(main,"recipe",true,"Terraria.Recipe[]",&c.table,error)||
       !bindField(c.recipe,"createItem",false,"Terraria.Item",&c.result,error)||
       !bindField(c.recipe,"requiredItem",false,"Terraria.Item[]",&c.ingredients,error)||
       !bindField(c.item,"type",false,"System.Int32",&c.itemType,error)||
       !bindField(c.item,"stack",false,"System.Int32",&c.stack,error)||
       !bindField(c.item,"maxStack",false,"System.Int32",&c.maxStack,error))return false;
    c.ctor=method(c.recipe,"void .ctor()",true,error);
    c.defaults=method(c.item,"void SetDefaults(int Type, ItemVariant variant)",true,error);
    c.station=method(c.recipe,"void SetCraftingStation(int tileType)",true,error);
    c.add=method(c.recipe,"void AddRecipe()",false,error);
    if(!c.ctor||!c.defaults||!c.station||!c.add)return false;
    c.variant=a.class_from_il2cpp_type(a.method_get_param(c.defaults,1));
    if(!c.variant){*error="Method=SetDefaults Parameter=variant Reason=MissingParameterType";return false;}
    if(a.class_is_valuetype(c.variant)) {
        uint32_t align=0;const auto size=a.class_value_size(c.variant,&align);
        if(size<=0||size>256||align>alignof(uint64_t)){*error="Parameter=variant Reason=UnsupportedValueLayout Size="+std::to_string(size);return false;}
        c.variantSize=static_cast<size_t>(size);
    }
    struct Step { const char* ns; const char* cls; const char* nested; const char* signature; };
    const Step steps[]={
        {"Terraria","Recipe","","void CreateRequiredItemQuickLookups()"},
        {"Terraria","Recipe","","void UpdateMaterialFieldForAllRecipes()"},
        {"Terraria","Recipe","","void UpdateWhichItemsAreMaterials()"},
        {"Terraria","Recipe","","void UpdateWhichItemsAreCrafted()"},
        {"Terraria.GameContent","ShimmerTransforms","","void UpdateRecipeSets()"},
        {"Terraria.ID","ContentSamples","","void FixItemsAfterRecipesAreAdded()"}};
    for(const auto& step:steps) {
        auto* cls=il2cpp::findClass({step.ns,step.cls,step.nested});
        const auto* m=method(cls,step.signature,false,error);if(!m)return false;c.lookups.push_back(m);
    }
    c.ready=true;contract=std::move(c);return true;
}
template<class T>T read(Il2CppObject* obj,int offset) {T value;std::memcpy(&value,reinterpret_cast<char*>(obj)+offset,sizeof(T));return value;}
template<class T>void write(Il2CppObject* obj,int offset,T value) {std::memcpy(reinterpret_cast<char*>(obj)+offset,&value,sizeof(T));}
struct Root {
    uint32_t handle=0;
    explicit Root(Il2CppObject* object):handle(object?il2cpp::api().gchandle_new(object,false):0){}
    ~Root(){if(handle)il2cpp::api().gchandle_free(handle);}
    Root(const Root&)=delete;
};
bool invoke(const MethodInfo* m,void* obj,void** args,std::string* error,Il2CppObject** result=nullptr) {
    auto& a=il2cpp::api();Il2CppObject* ex=nullptr;auto* value=a.runtime_invoke(m,obj,args,&ex);
    if(result)*result=ex?nullptr:value;
    if(!ex)return true;
    Root root(ex);
    *error="Method="+il2cpp::describeMethod(m)+" Exception="+typeName(a.object_get_class(ex));
    // Exception.ToString includes the original message and managed stack trace.
    const auto* tostring=il2cpp::findMethodBySignature(a.object_get_class(ex),il2cpp::parseSignature("string ToString()"));
    if(tostring) {
        Il2CppObject* formatting=nullptr;auto* s=reinterpret_cast<Il2CppString*>(a.runtime_invoke(tostring,ex,nullptr,&formatting));
        if(s&&!formatting&&s->length>=0&&s->length<=32768) {
            *error+=" Detail=";
            for(int i=0;i<s->length;++i) {const uint32_t ch=s->chars[i];
                if(ch<0x80)error->push_back(static_cast<char>(ch));
                else if(ch<0x800){error->push_back(static_cast<char>(0xc0|(ch>>6)));error->push_back(static_cast<char>(0x80|(ch&63)));}
                else {error->push_back(static_cast<char>(0xe0|(ch>>12)));error->push_back(static_cast<char>(0x80|((ch>>6)&63)));error->push_back(static_cast<char>(0x80|(ch&63)));}
            }
        }
    }
    return false;
}
struct GroupContract {
    Binding field;
    Il2CppClass* groupClass=nullptr;
    const MethodInfo* placeholder=nullptr;
    const MethodInfo* require=nullptr;
};
std::map<std::string,GroupContract> groupContracts;
bool readGroupObject(const std::string& name,Il2CppObject** object,const GroupContract& binding,
                     const MethodInfo** require,int* placeholder,std::string* detail) {
    auto& a=il2cpp::api();
    Root groupRoot(*object);
    if(!groupRoot.handle){*detail="Group="+name+" Root=RecipeGroup Reason=AllocationFailed";return false;}
    Il2CppObject* boxed=nullptr;
    if(!invoke(binding.placeholder,*object,nullptr,detail,&boxed)){*detail="Group="+name+" "+*detail;return false;}
    Root resultRoot(boxed);
    uint32_t alignment=0;auto* actual=boxed?a.object_get_class(boxed):nullptr;
    if(!boxed||!resultRoot.handle||typeName(actual)!="System.Int32"||
       !a.class_is_valuetype(actual)||a.class_value_size(actual,&alignment)!=4) {
        *detail="Group="+name+" Method=GetPlaceholderItemType Expected=System.Int32:4 Actual="+typeName(actual);return false;
    }
    il2cpp::ValueLayout layout;
    if(!il2cpp::resolveValueLayout(actual,&layout,detail))return false;
    auto* value=a.class_get_field_from_name?a.class_get_field_from_name(actual,"m_value"):nullptr;
    size_t offset=0;const size_t boxedSize=a.class_instance_size(actual);
    if(!value||(a.field_get_flags(value)&0x10u)||
       a.class_from_il2cpp_type(a.field_get_type(value))!=actual||
       !layout.offset(a.field_get_offset(value),sizeof(int),&offset)||
       boxedSize<sizeof(Il2CppObject)+sizeof(int)||offset>boxedSize-sizeof(Il2CppObject)-sizeof(int)) {
        *detail="Group="+name+" Method=GetPlaceholderItemType Field=m_value Expected=BoundedBoxedInt32 ActualSize="+std::to_string(boxedSize)+" Reason=InvalidValueLayout";return false;
    }
    std::memcpy(placeholder,reinterpret_cast<const char*>(boxed)+sizeof(Il2CppObject)+offset,sizeof(int));
    *require=binding.require;return true;
}
bool readNativeGroup(const std::string& name,Il2CppObject** object,
                     const MethodInfo** require,int* placeholder,std::string* detail,bool* notReady=nullptr) {
    if(notReady)*notReady=false;
    auto& a=il2cpp::api();
    auto it=groupContracts.find(name);
    if(it==groupContracts.end()) {
        auto* groups=il2cpp::findClass({"Terraria.ID","RecipeGroups",{}});
        auto* group=il2cpp::findClass({"Terraria","RecipeGroup",{}});
        if(!groups||!group){*detail="Group="+name+" Class=Terraria.ID.RecipeGroups/Terraria.RecipeGroup Reason=MissingClass";return false;}
        a.runtime_class_init(groups);a.runtime_class_init(group);
        GroupContract binding;
        if(!bindField(groups,name.c_str(),true,"Terraria.RecipeGroup",&binding.field,detail))return false;
        binding.placeholder=method(group,"int GetPlaceholderItemType()",true,detail);
        binding.require=method(contract.recipe,"void RequireGroup(RecipeGroup group)",true,detail);
        if(!binding.placeholder||!binding.require)return false;
        binding.groupClass=group;
        it=groupContracts.emplace(name,std::move(binding)).first;
    }
    const auto& binding=it->second;
    a.field_static_get_value(binding.field.field,object);
    if(!*object){if(notReady)*notReady=true;*detail="Group="+name+" Expected=InitializedRecipeGroup Actual=Null Reason=RecipeSetupNotReady";return false;}
    if(a.object_get_class(*object)!=binding.groupClass){*detail="Group="+name+" Expected=Terraria.RecipeGroup Actual="+typeName(a.object_get_class(*object));return false;}
    return readGroupObject(name,object,binding,require,placeholder,detail);
}

struct CustomGroup {
    std::string description;
    std::vector<int> members;
    uint32_t root=0;
    GroupContract binding;
    const MethodInfo* sort=nullptr;
    bool sorted=false;
};
std::map<std::string,CustomGroup> customGroups;
bool readCustomGroup(const NativeRecipeRequest& request,const RecipeIngredient& ingredient,
                     Il2CppObject** object,const MethodInfo** require,int* placeholder,
                     std::string* detail,bool* notReady=nullptr) {
    if(notReady)*notReady=false;
    auto& a=il2cpp::api();
    const auto key=qualified(request.mod,ingredient.customGroup);
    if(!keyValid(key)||ingredient.customGroup.find('/')!=std::string::npos||
       ingredient.groupDescription.empty()||ingredient.groupDescription.size()>256||
       ingredient.groupMembers.empty()||ingredient.groupMembers.size()>256) {
        *detail="Group="+key+" Reason=InvalidCustomGroupDefinition";return false;
    }
    std::vector<int> members;std::set<int> unique;
    for(const auto& token:ingredient.groupMembers) {
        int type=-1;
        if(token.size()>1&&token[0]=='v') {
            const auto number=token.substr(1);
            if(number.empty()||number.find_first_not_of("0123456789")!=std::string::npos||number.size()>4){*detail="Group="+key+" Member="+token+" Reason=InvalidVanillaItem";return false;}
            type=std::stoi(number);
            if(type<1||type>=kVanillaItemCount){*detail="Group="+key+" Member="+token+" Reason=InvalidVanillaItem";return false;}
        } else if(token.size()>1&&token[0]=='m'&&keyValid(token.substr(1)))type=modItemTypeByKey(qualified(request.mod,token.substr(1)));
        if(type<1||type>=itemTypeCount()) {
            if(notReady)*notReady=true;
            *detail="Group="+key+" Member="+token+" Reason=MissingDependency";return false;
        }
        if(!unique.insert(type).second){*detail="Group="+key+" Member="+token+" Reason=DuplicateMember";return false;}
        members.push_back(type);
    }
    auto it=customGroups.find(key);
    if(it!=customGroups.end()) {
        if(it->second.members!=members||it->second.description!=ingredient.groupDescription){*detail="Group="+key+" Reason=ConflictingGroupDefinition";return false;}
        *object=a.gchandle_get_target(it->second.root);
        if(!*object){*detail="Group="+key+" Reason=LostGroupRoot";return false;}
        if(!it->second.sorted){
            if(!invoke(it->second.sort,*object,nullptr,detail))return false;
            it->second.sorted=true;
        }
        return readGroupObject(key,object,it->second.binding,require,placeholder,detail);
    }
    int recipeCount=0;Il2CppObject* current=nullptr;
    a.field_static_get_value(contract.count.field,&recipeCount);a.field_static_get_value(contract.current.field,&current);
    if(recipeCount<=0||!current){
        if(notReady)*notReady=true;
        *detail="Group="+key+" Reason=RecipeSetupNotReady NumRecipes="+std::to_string(recipeCount);return false;
    }
    if(!a.array_new||!a.string_new||!a.gchandle_get_target){*detail="Group="+key+" Reason=RequiredExportMissing";return false;}
    auto* group=il2cpp::findClass({"Terraria","RecipeGroup",{}});
    auto* integer=il2cpp::findClass({"System","Int32",{}});
    if(!group||!integer){*detail="Group="+key+" Reason=MissingClass";return false;}
    a.runtime_class_init(group);uint32_t alignment=0;
    if(!a.class_is_valuetype(integer)||a.class_value_size(integer,&alignment)!=4){*detail="Group="+key+" Expected=System.Int32:4 Reason=InvalidIntegerLayout";return false;}
    const auto* ctor=method(group,"void .ctor(string groupDescriptorKey, int[] validItems)",true,detail);
    const auto* reg=method(group,"int Register()",true,detail);
    if(!reg) {
        const auto* alternative=method(group,"void Register()",true,detail);
        if(alternative)reg=alternative;
        else {
            const auto candidates=il2cpp::listOverloads(group,"Register");
            *detail="Group="+key+" Method=Register() Candidates=";
            if(candidates.empty())*detail+="None";
            for(const auto& signature:candidates) {
                if(detail->size()>1200)break;
                *detail+=signature+";";
            }
            return false;
        }
    }
    const auto* sort=method(group,"void SortDecraftingEntries()",true,detail);
    GroupContract binding;binding.groupClass=group;
    binding.placeholder=method(group,"int GetPlaceholderItemType()",true,detail);
    binding.require=method(contract.recipe,"void RequireGroup(RecipeGroup group)",true,detail);
    if(!ctor||!sort||!binding.placeholder||!binding.require)return false;
    auto* created=a.object_new(group);Root groupRoot(created);
    auto* array=a.array_new(integer,members.size());Root membersRoot(array?&array->obj:nullptr);
    auto* descriptor=a.string_new(ingredient.groupDescription.c_str());Root descriptorRoot(reinterpret_cast<Il2CppObject*>(descriptor));
    if(!groupRoot.handle||!membersRoot.handle||!descriptorRoot.handle||!array||
       a.class_get_rank(array->obj.klass)!=1||a.class_get_element_class(array->obj.klass)!=integer||array->length!=members.size()) {
        *detail="Group="+key+" Reason=AllocationOrArrayLayoutFailed";return false;
    }
    std::memcpy(arrayData(array),members.data(),members.size()*sizeof(int));
    void* args[]={descriptor,array};
    if(!invoke(ctor,created,args,detail))return false;
    const auto handle=a.gchandle_new(created,false);
    if(!handle){*detail="Group="+key+" Reason=AllocationFailed";return false;}
    if(!invoke(reg,created,nullptr,detail)){a.gchandle_free(handle);return false;}
    auto entry=customGroups.emplace(key,CustomGroup{ingredient.groupDescription,members,handle,binding,sort,false}).first;
    if(!invoke(sort,created,nullptr,detail))return false;
    entry->second.sorted=true;
    *object=created;return readGroupObject(key,object,binding,require,placeholder,detail);
}

bool arrayOf(Il2CppArray* array,Il2CppClass* element,uintptr_t minimum) {
    auto& a=il2cpp::api();
    return array&&array->length>=minimum&&array->length<=100000&&
        a.class_get_rank(array->obj.klass)==1&&a.class_get_element_class(array->obj.klass)==element;
}
bool setItem(Il2CppObject* item,int type,int count,std::string* error) {
    auto& a=il2cpp::api();if(!item||a.object_get_class(item)!=contract.item){*error="Field=Recipe.Item Reason=UnexpectedObjectType";return false;}
    std::vector<uint64_t> variant((contract.variantSize+7)/8,0);
    void* args[]={&type,contract.variantSize?variant.data():nullptr};
    if(!invoke(contract.defaults,item,args,error))return false;
    const auto actualType=read<int>(item,contract.itemType.offset), maximum=read<int>(item,contract.maxStack.offset);
    if(actualType!=type||count<1||count>maximum) {
        *error="Item="+std::to_string(type)+" ActualType="+std::to_string(actualType)+" Count="+std::to_string(count)+" MaxStack="+std::to_string(maximum)+" Reason=ItemDefaultsRejected";return false;
    }
    write<int>(item,contract.stack.offset,count);return true;
}
}

void setRecipeCallbackProvider(const RecipeCallbackProvider* provider){callbackProvider.store(provider,std::memory_order_release);}

bool resolveNativeRecipe(const NativeRecipeRequest& r,NativeRecipeRequest* out,std::string* error) {
    *out=r;
    auto fail=[&](const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
        *error=diagnostic(r,stage,reason,expected,actual);return false;
    };
    if(r.callbacks) {
        const auto hash=r.key.rfind('#');int local=-1;
        if(hash!=std::string::npos) {
            const auto text=r.key.substr(hash+1);const auto parsed=std::from_chars(text.data(),text.data()+text.size(),local);
            if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())local=-1;
        }
        if(r.callbacks<0||r.callbacks>7||local<0||local>=16)return fail("ResolveCallbacks","InvalidHandlerIdentity","Recipe#Index0..15,Mask1..7",r.key);
    }
    if(r.key.empty()||r.result<1||r.stack<1||r.stack>9999||r.ingredients.empty()||r.ingredients.size()>15)
        return fail("ValidateDescriptor","InvalidDescriptor","Result>0,Stack=1..9999,Ingredients=1..15",std::to_string(r.ingredients.size()));
    if(r.result>=kVanillaItemCount&&(!isModItem(r.result)||r.result>=itemTypeCount()))
        return fail("ResolveResult","DependencyNotInstalled","InstalledItem",std::to_string(r.result));
    if(!r.stationKey.empty()) {
        if(!keyValid(r.stationKey))return fail("ResolveStation","InvalidContentKey","NameOrUID/Name",r.stationKey);
        const auto key=qualified(r.mod,r.stationKey);out->station=modTileTypeByKey(key);
        if(out->station<0)return fail("ResolveStation","MissingDependency","RegisteredTile",key);
        if(out->station>=tileTypeCount())return fail("ResolveStation","DependencyNotInstalled","InstalledTile",key);
    } else if(r.station< -1||r.station>=kVanillaTileCount)
        return fail("ResolveStation","InvalidVanillaTile","-1.."+std::to_string(kVanillaTileCount-1),std::to_string(r.station));
    std::set<std::string> groups;
    for(auto& ingredient:out->ingredients) {
        if(ingredient.count<1||ingredient.count>9999)return fail("ResolveIngredient","InvalidAmount","1..9999",std::to_string(ingredient.count));
        if(!ingredient.customGroup.empty()) {
            const auto key=qualified(r.mod,ingredient.customGroup);
            if(!groups.insert("custom:"+key).second)return fail("ResolveRecipeGroup","DuplicateRecipeGroup","OneIngredientPerGroup",key);
            std::string detail;Il2CppObject* object=nullptr;const MethodInfo* require=nullptr;bool notReady=false;
            if(!prepare(&detail)||!readCustomGroup(r,ingredient,&object,&require,&ingredient.type,&detail,&notReady))
                return fail("ResolveRecipeGroup",notReady?(detail.find("RecipeSetupNotReady")!=std::string::npos?"RecipeSetupNotReady":"MissingDependency"):"CustomGroupContractMismatch","InitializedCustomRecipeGroup",key+":"+detail);
        } else if(!ingredient.nativeGroup.empty()) {
            const auto& group=ingredient.nativeGroup;
            bool valid=group.size()<=72&&((group.front()>='A'&&group.front()<='Z')||(group.front()>='a'&&group.front()<='z'));
            for(unsigned char ch:group)if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'))valid=false;
            if(!valid||!ingredient.key.empty())return fail("ResolveRecipeGroup","InvalidGroupField","RecipeGroupsField",group);
            if(!groups.insert(group).second)return fail("ResolveRecipeGroup","DuplicateRecipeGroup","OneIngredientPerGroup",group);
            std::string detail;Il2CppObject* object=nullptr;const MethodInfo* require=nullptr;bool notReady=false;
            if(!prepare(&detail)||!readNativeGroup(group,&object,&require,&ingredient.type,&detail,&notReady))
                return fail("ResolveRecipeGroup",notReady?"RecipeSetupNotReady":"NativeGroupContractMismatch","InitializedNativeRecipeGroup",group+":"+detail);
            if(ingredient.type<1||ingredient.type>=itemTypeCount())
                return fail("ResolveRecipeGroup","InvalidGroupPlaceholder","InstalledItem",group+":"+std::to_string(ingredient.type));
        } else if(!ingredient.key.empty()) {
            if(!keyValid(ingredient.key))return fail("ResolveIngredient","InvalidContentKey","NameOrUID/Name",ingredient.key);
            const auto key=qualified(r.mod,ingredient.key);ingredient.type=modItemTypeByKey(key);
            if(ingredient.type<0)return fail("ResolveIngredient","MissingDependency","RegisteredItem",key);
            if(ingredient.type>=itemTypeCount())return fail("ResolveIngredient","DependencyNotInstalled","InstalledItem",key);
        } else if(ingredient.type<1||ingredient.type>=kVanillaItemCount)
            return fail("ResolveIngredient","InvalidVanillaItem","1.."+std::to_string(kVanillaItemCount-1),std::to_string(ingredient.type));
    }
    error->clear();return true;
}

NativeRecipeResult installNativeRecipe(const NativeRecipeRequest& request) {
    NativeRecipeResult result;NativeRecipeRequest r;
    if(auto it=completed.find(request.key);it!=completed.end()){result.index=it->second;return result;}
    if(!resolveNativeRecipe(request,&r,&result.error))return result;
    std::string detail;
    auto fail=[&](const char* stage,const char* reason){result.error=diagnostic(request,stage,reason,"NativeRecipeContract",detail);return result;};
    if(!prepare(&detail))return fail("BindContract","NativeContractMismatch");
    const auto* provider=callbackProvider.load(std::memory_order_acquire);
    if(r.callbacks&&(!provider||!provider->prepare||!provider->track)){detail="Provider=Missing";return fail("WaitCallbacks","RecipeSetupNotReady");}
    if(r.callbacks){const auto readiness=provider->prepare(r,&detail);
        if(readiness!=RecipeCallbackReadiness::Ready)return fail("BindCallbacks",readiness==RecipeCallbackReadiness::Pending?"RecipeSetupNotReady":"NativeContractMismatch");}
    auto& a=il2cpp::api();
    if(!staged.contains(r.key)) {
        int index=0,maximum=0;Il2CppArray* table=nullptr;Il2CppObject* previous=nullptr;
        a.field_static_get_value(contract.count.field,&index);a.field_static_get_value(contract.maximum.field,&maximum);
        a.field_static_get_value(contract.table.field,&table);a.field_static_get_value(contract.current.field,&previous);
        // Do not commit into the empty table that SetupRecipes will overwrite.
        if(index==0||!previous) {detail="NumRecipes="+std::to_string(index)+" CurrentRecipe="+(previous?"Present":"Null");return fail("WaitRecipeSetup","RecipeSetupNotReady");}
        if(index<0||maximum<1||index>=maximum) {
            detail="Index="+std::to_string(index)+" NativeMaximum="+std::to_string(maximum);return fail("CheckCapacity","NativeRecipeLimitReached");
        }
        if(!arrayOf(table,contract.recipe,static_cast<uintptr_t>(index+1))) {
            detail="Field=Main.recipe Expected=Recipe[] Length>Index Actual="+(table?std::to_string(table->length):"Null");return fail("CheckCapacity","RecipeTableNotReady");
        }
        auto* recipe=a.object_new(contract.recipe);Root recipeRoot(recipe), previousRoot(previous);
        if(!recipe||!recipeRoot.handle){detail="Allocation=Terraria.Recipe";return fail("CreateRecipe","AllocationFailed");}
        if(previous&&!previousRoot.handle){detail="Root=Recipe.currentRecipe";return fail("CreateRecipe","AllocationFailed");}
        if(!invoke(contract.ctor,recipe,nullptr,&detail))return fail("CreateRecipe","ManagedException");
        auto* output=read<Il2CppObject*>(recipe,contract.result.offset);
        auto* ingredients=read<Il2CppArray*>(recipe,contract.ingredients.offset);
        if(!arrayOf(ingredients,contract.item,r.ingredients.size())) {detail="Field=requiredItem Expected=Item[]";return fail("SetIngredients","InvalidArrayLayout");}
        if(!setItem(output,r.result,r.stack,&detail))return fail("SetResult","ItemRejected");
        auto** items=static_cast<Il2CppObject**>(arrayData(ingredients));
        for(size_t i=0;i<r.ingredients.size();++i)if(!setItem(items[i],r.ingredients[i].type,r.ingredients[i].count,&detail))return fail("SetIngredients","ItemRejected");
        if(r.station>=0){void* args[]={&r.station};if(!invoke(contract.station,recipe,args,&detail))return fail("SetStation","ManagedException");}
        for(const auto& ingredient:r.ingredients)if(!ingredient.nativeGroup.empty()||!ingredient.customGroup.empty()) {
            Il2CppObject* group=nullptr;const MethodInfo* require=nullptr;int placeholder=-1;
            const bool groupReady=ingredient.customGroup.empty()
                ?readNativeGroup(ingredient.nativeGroup,&group,&require,&placeholder,&detail)
                :readCustomGroup(r,ingredient,&group,&require,&placeholder,&detail);
            if(!groupReady)return fail("RequireGroup","RecipeGroupContractMismatch");
            Root groupRoot(group);
            if(!groupRoot.handle){detail="Group="+ingredient.nativeGroup+" Root=RecipeGroup";return fail("RequireGroup","AllocationFailed");}
            if(placeholder!=ingredient.type){detail="Group="+ingredient.nativeGroup+" ExpectedPlaceholder="+std::to_string(ingredient.type)+" Actual="+std::to_string(placeholder);return fail("RequireGroup","GroupChangedDuringRegistration");}
            void* args[]={group};
            if(!invoke(require,recipe,args,&detail)){detail="Group="+ingredient.nativeGroup+" "+detail;return fail("RequireGroup","ManagedException");}
        }
        auto** slots=static_cast<Il2CppObject**>(arrayData(table));
        auto* oldSlot=slots[index];Root slotRoot(oldSlot);
        if(oldSlot&&!slotRoot.handle){detail="Root=Main.recipe[Index]";return fail("Commit","AllocationFailed");}
        // IL2CPP reference setters receive the object itself; only value fields use addresses.
        a.field_static_set_value(contract.current.field,recipe);
        const bool invoked=invoke(contract.add,nullptr,nullptr,&detail);
        int after=0;a.field_static_get_value(contract.count.field,&after);
        // Preserve any JS registration transaction that owned currentRecipe.
        a.field_static_set_value(contract.current.field,previous);
        if(!invoked||after!=index+1||slots[index]!=recipe) {
            a.gc_wbarrier_set_field(&table->obj,reinterpret_cast<void**>(&slots[index]),oldSlot);
            a.field_static_set_value(contract.count.field,&index);
            if(invoked)detail="ExpectedCount="+std::to_string(index+1)+" ActualCount="+std::to_string(after)+" ExpectedSlot=StagedRecipe";
            return fail("Commit",invoked?"CommitNotConfirmed":"ManagedException");
        }
        staged.emplace(r.key,index);dirty=true;
    }
    if(dirty) {
        for(const auto* m:contract.lookups)if(!invoke(m,nullptr,nullptr,&detail))return fail("RebuildLookups","ManagedException");
        dirty=false;
    }
    result.index=staged.at(r.key);
    Il2CppArray* table=nullptr;a.field_static_get_value(contract.table.field,&table);
    if(r.callbacks&&table&&static_cast<uintptr_t>(result.index)<table->length)
        provider->track(r,static_cast<Il2CppObject**>(arrayData(table))[result.index],result.index);
    completed.emplace(r.key,result.index);staged.erase(r.key);return result;
}
}
