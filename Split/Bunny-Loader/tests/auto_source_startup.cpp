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
int mode=0,classResolutions=0,fieldResolutions=0,methodResolutions=0;
struct Box { Il2CppObject object{}; uint8_t value=0; } boxed;
bl::csharp::BasicItemLoadStatus content;
std::vector<std::string> messages;
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
BasicItemLoadStatus basicItemLoadStatus(){return content;}
int registeredManagedPrefixCount(){return 2;}
ManagedNpcBuffEffectStatus managedNpcBuffEffectStatus(){return {};}
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
    std::cout<<"AUTO_SOURCE_REAL_METADATA_CACHE_DYNAMIC_VALUES_EARLY_RETRY_AND_SINGLE_STARTUP_SUMMARY_PASS\n";
}
