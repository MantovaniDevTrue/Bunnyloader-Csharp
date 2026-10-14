#include "content/recipes/NativeRecipes.h"
#include "content/items/ModItems.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cstring>
#include <limits>
#include <map>
#include <vector>

namespace bl::runtime {
namespace {
std::string diagnostic(const NativeRecipeRequest& r, const std::string& stage,
                       const std::string& reason, const std::string& expected, const std::string& actual) {
    return "[CSharp][ERROR][Recipe] Mod="+r.mod+" Class="+r.name+" File="+r.source+
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
bool invoke(const MethodInfo* m,void* obj,void** args,std::string* error) {
    auto& a=il2cpp::api();Il2CppObject* ex=nullptr;a.runtime_invoke(m,obj,args,&ex);
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

bool resolveNativeRecipe(const NativeRecipeRequest& r,NativeRecipeRequest* out,std::string* error) {
    *out=r;
    auto fail=[&](const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
        *error=diagnostic(r,stage,reason,expected,actual);return false;
    };
    if(r.key.empty()||r.result<1||r.stack<1||r.stack>9999||r.ingredients.empty()||r.ingredients.size()>15)
        return fail("ValidateDescriptor","InvalidDescriptor","Result>0,Stack=1..9999,Ingredients=1..15",std::to_string(r.ingredients.size()));
    if(!r.stationKey.empty()) {
        if(!keyValid(r.stationKey))return fail("ResolveStation","InvalidContentKey","NameOrUID/Name",r.stationKey);
        const auto key=qualified(r.mod,r.stationKey);out->station=modTileTypeByKey(key);
        if(out->station<0)return fail("ResolveStation","MissingDependency","RegisteredTile",key);
        if(out->station>=tileTypeCount())return fail("ResolveStation","DependencyNotInstalled","InstalledTile",key);
    } else if(r.station< -1||r.station>=kVanillaTileCount)
        return fail("ResolveStation","InvalidVanillaTile","-1.."+std::to_string(kVanillaTileCount-1),std::to_string(r.station));
    for(auto& ingredient:out->ingredients) {
        if(ingredient.count<1||ingredient.count>9999)return fail("ResolveIngredient","InvalidAmount","1..9999",std::to_string(ingredient.count));
        if(!ingredient.key.empty()) {
            if(!keyValid(ingredient.key))return fail("ResolveIngredient","InvalidContentKey","NameOrUID/Name",ingredient.key);
            const auto key=qualified(r.mod,ingredient.key);ingredient.type=modItemTypeByKey(key);
            if(ingredient.type<0)return fail("ResolveIngredient","MissingDependency","RegisteredItem",key);
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
    auto& a=il2cpp::api();
    if(!staged.contains(r.key)) {
        int index=0,maximum=0;Il2CppArray* table=nullptr;Il2CppObject* previous=nullptr;
        a.field_static_get_value(contract.count.field,&index);a.field_static_get_value(contract.maximum.field,&maximum);
        a.field_static_get_value(contract.table.field,&table);a.field_static_get_value(contract.current.field,&previous);
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
        auto** slots=static_cast<Il2CppObject**>(arrayData(table));
        auto* oldSlot=slots[index];Root slotRoot(oldSlot);
        if(oldSlot&&!slotRoot.handle){detail="Root=Main.recipe[Index]";return fail("Commit","AllocationFailed");}
        a.field_static_set_value(contract.current.field,&recipe);
        const bool invoked=invoke(contract.add,nullptr,nullptr,&detail);
        int after=0;a.field_static_get_value(contract.count.field,&after);
        // Preserve any JS registration transaction that owned currentRecipe.
        a.field_static_set_value(contract.current.field,&previous);
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
    result.index=staged.at(r.key);completed.emplace(r.key,result.index);staged.erase(r.key);return result;
}
}
