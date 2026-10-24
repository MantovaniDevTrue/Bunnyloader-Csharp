#include "mods/ModLoader.h"
#include "script/bridge/ScriptEngine.h"
#include "experiments/csharp/SourceLoader.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int initialized=0,jsLoaded=0,csLoaded=0,builtinLoaded=0,sourceResets=0;
std::vector<std::string> activeCSharp;
bool failJs=false;
namespace bl::script {
ScriptEngine& engine(){static ScriptEngine value;return value;}
bool ScriptEngine::init(){++initialized;ready_=!failJs;return ready_;}
bool ScriptEngine::loadMod(const std::string&,const std::vector<std::string>& files){assert(ready_&&!files.empty());++jsLoaded;return true;}
bool ScriptEngine::loadBuiltinMod(const std::string&,const std::string&){assert(ready_);++builtinLoaded;return true;}
}
namespace bl::runtime {
void configureTexturePacks(const std::string&,const std::vector<ModSpec>&){}
void configureFontPacks(const std::string&,const std::vector<ModSpec>&){}
}
namespace bl::csharp {void resetCSharpSourceModSession(){++sourceResets;activeCSharp.clear();} bool loadCSharpSourceMod(const mods::LoadedMod& mod){assert(mod.entry.ends_with(".cs"));++csLoaded;activeCSharp.push_back(mod.id);return true;}}
namespace bl::log {void write(int,const char*,...) {}}
int main(int argc,char** argv) {
    assert(argc==3);const std::string mode=argv[2];failJs=mode=="--js-fails";
    if(mode=="--builtin") {
        bl::mods::loadBuiltins();assert(initialized==1&&builtinLoaded==1);return 0;
    }
    namespace fs=std::filesystem;const fs::path root=argv[1];
    for(const char* name:{"cs1","cs2","js1","js2"}) {
        fs::create_directories(root/name/"content");
        std::ofstream(root/name/"manifest.json")<<"{\"id\":\""<<name<<"\",\"name\":\""<<name<<"\"}";
        std::ofstream(root/name/"content"/(name[0]=='c'?"main.cs":"main.js"))<<"fixture";
    }
    std::vector<bl::ModSpec> specs={{"cs1","main.cs"},{"cs2","main.cs"}};
    if(mode!="--only-csharp")specs={{"cs1","main.cs"},{"js1","main.js"},{"cs2","main.cs"},{"js2","main.js"}};
    bl::mods::loadAll(root.string(),specs);
    assert(csLoaded==2&&sourceResets==1);
    if(mode=="--only-csharp")assert(initialized==0&&jsLoaded==0&&!bl::script::engine().ready());
    else if(failJs)assert(initialized==2&&jsLoaded==0);
    else assert(initialized==1&&jsLoaded==2);
    for(uint16_t i=0;i<bl::mods::loadedCount();++i) {
        const auto* mod=bl::mods::get(i);assert(mod);
        if(mod->entry.ends_with(".cs"))assert(mod->loaded&&mod->enabled);
        else assert(mod->loaded==!failJs&&mod->enabled==!failJs);
    }
    if(mode=="--only-csharp") {
        bl::mods::loadAll(root.string(),{{"cs1","main.cs"}});
        assert(sourceResets==2&&csLoaded==3);
        assert(activeCSharp.size()==1&&activeCSharp.front()=="cs1");
        assert(bl::mods::loadedCount()==1);
        assert(initialized==0&&jsLoaded==0);
    }
    std::cout<<"CSHARP_MOD_LOADING_NEVER_INITIALIZES_JS_AND_MIXED_LOADING_PRESERVED_PASS "<<mode<<"\n";
}
