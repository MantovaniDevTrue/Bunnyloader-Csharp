#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include "../app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp"

struct Il2CppClass {};
struct FieldInfo { bool menu; };
struct MethodInfo {};

namespace {
Il2CppClass mainClass;
FieldInfo netField{false},menuField{true};
MethodInfo menuGetter;
bool available=false,fieldOnly=false,inMenu=true;
int mode=0,classResolutions=0,fieldResolutions=0,methodResolutions=0,worldEntries=0;
struct Box { Il2CppObject object{}; uint8_t value=0; } boxed;
bl::csharp::BasicItemLoadStatus content;
std::vector<std::string> messages;
std::vector<bl::csharp::SourceModStatus> sources;
}
namespace bl::log {
void write(int,const char* format,...) {
    char text[2048];va_list args;va_start(args,format);
    std::vsnprintf(text,sizeof(text),format,args);va_end(args);messages.emplace_back(text);
}
}
namespace bl::il2cpp {
Api& api(){static Api instance{};return instance;}
Il2CppClass* findClass(const TypeRef& ref) {
    assert(ref.ns=="Terraria"&&ref.name=="Main");++classResolutions;
    return available?&mainClass:nullptr;
}
FieldInfo* findField(Il2CppClass*,std::string_view name) {
    ++fieldResolutions;return name=="netMode"?&netField:&menuField;
}
}
namespace bl::csharp {
bool conversionBindingReady=true;
bool nativeBiomeConversionReady() { return conversionBindingReady; }
void resetBiomeConversionWorld() {}
void beginBiomeConversionWorld() {}
void pumpBiomeConversionsOnGameThread() {}

BasicItemLoadStatus basicItemLoadStatus(){return content;}
std::vector<SourceModStatus> sourceModStatuses(){return sources;}
size_t pendingNativeVisualRenderer(){return 0;}
bool managedContentHostClosing(){return false;}
int pendingNativeArmorPolicies(){return 0;}
size_t pendingManagedNativeEquipTextures(){return 0;}
size_t pendingNativeItemPolicies(){return 0;}
std::vector<NativeItemPolicyStatus> nativeItemPolicyStatus(){return {};}
void requestBasicItemInstanceRefresh(const char*){}
bool managedPrefixGameplayReady(){return false;}
void refreshManagedPrefixEligibilityOnWorldReady(){}
int registeredManagedPrefixCount(){return 2;}
ManagedNpcBuffEffectStatus managedNpcBuffEffectStatus(){return {};}
}

namespace bl::runtime {
bool modItemsSettled(){return true;}
int pendingManagedPrefixCountForActivePlayer(){return 0;}
void retryPendingManagedPrefixesOnWorldReady(){}
}
int main(int argc,char**) {
    using namespace bl::csharp;
    fieldOnly=argc>1;
    auto& api=bl::il2cpp::api();
    api.class_get_method_from_name=[](Il2CppClass*,const char*,int)->const MethodInfo* {
        ++methodResolutions;return fieldOnly?nullptr:&menuGetter;
    };
    api.field_static_get_value=[](FieldInfo* field,void* output) {
        if(field->menu)*static_cast<uint8_t*>(output)=inMenu?1:0;
        else *static_cast<int*>(output)=mode;
    };
    api.runtime_invoke=[](const MethodInfo*,void*,void**,Il2CppObject** error)->Il2CppObject* {
        *error=nullptr;boxed.value=inMenu?1:0;return &boxed.object;
    };
    assert(readActivationContext().netMode==-1); // An early failure must not poison the cache.
    available=true;
    auto context=readActivationContext();assert(context.netMode==0&&context.gameMenu==1);
    const int resolvedClasses=classResolutions,resolvedFields=fieldResolutions,resolvedMethods=methodResolutions;
    mode=1;inMenu=false;g_worldSessionReady=true;
    for(int n=0;n<10000;++n) {
        context=readActivationContext();
        assert(context.netMode==1&&context.gameMenu==0&&context.worldReady);
    }
    assert(classResolutions==resolvedClasses&&fieldResolutions==resolvedFields&&methodResolutions==resolvedMethods);
    g_initialCSharpRegistrationFinished=true;g_preWorldBindingsArmed=true;
    AutoSourceStatus status;status.modId="example";status.applied=true;status.phase="ready";g_statuses={status};
    content.items=14;content.installedRecipes=8;content.pendingRecipes=1;
    g_cachedMods=1;g_sourceStart=std::chrono::steady_clock::now()-std::chrono::milliseconds(500);
    reportCSharpLoadSummary();assert(messages.empty());
    content.pendingRecipes=0;reportCSharpLoadSummary();assert(messages.size()==1);
    assert(messages[0].find("14 item(ns), 2 prefixo(s), 8 receita(s)")!=std::string::npos);
    g_sourceStart-=std::chrono::seconds(96); // Gameplay/menu time must not produce another preparation report.
    inMenu=true;g_worldSessionReady=false;g_preWorldBindingsArmed=false;reportCSharpLoadSummary();
    g_preWorldBindingsArmed=true;reportCSharpLoadSummary();assert(messages.size()==1);
    // Exercise the production world-entry barrier for a source mod with no entity bindings.
    SourceModStatus service;service.id="example";service.loaded=true;service.sourceFiles=1;
    service.managedContentDeclarations=1;sources={service};
    assert(!preWorldCSharpContentReady()); // Applied native defaults alone are not acknowledgement.
    g_statuses[0].managedReady=true;
    conversionBindingReady=false;assert(!preWorldCSharpContentReady());
    conversionBindingReady=true;assert(preWorldCSharpContentReady());
    g_statuses[0].phase="failed";assert(!preWorldCSharpContentReady());
    g_statuses[0].phase="ready";g_statuses[0].error="Load failed";assert(!preWorldCSharpContentReady());
    g_statuses[0].error.clear();g_statuses[0].applied=false;assert(!preWorldCSharpContentReady());
    g_statuses[0].applied=true;assert(preWorldCSharpContentReady());
    g_statuses.clear();assert(!preWorldCSharpContentReady());
    // The six-second deferred-assets route must not bypass missing managed acknowledgement.
    g_statuses={status};g_statuses[0].managedReady=false;
    g_delayedWorldEntry=true;g_delayedWorldSince=std::chrono::steady_clock::now()-std::chrono::seconds(7);
    g_originalPlayWorld=[](const MethodInfo*){++worldEntries;};
    flushDeferredWorldEntry();assert(worldEntries==0&&g_delayedWorldEntry);
    g_statuses[0].managedReady=true;
    flushDeferredWorldEntry();assert(worldEntries==1&&!g_delayedWorldEntry);
    // Preserve the original native-assets deferral after managed initialization really completed.
    content.pendingRecipes=1;g_delayedWorldEntry=true;
    flushDeferredWorldEntry();assert(worldEntries==2&&!g_delayedWorldEntry);
    content.pendingRecipes=0;
    sources.clear();assert(preWorldCSharpContentReady());
    std::cout<<"AUTO_SOURCE_REAL_METADATA_CACHE_DYNAMIC_VALUES_EARLY_RETRY_AND_SINGLE_STARTUP_SUMMARY_PASS\n";
}
