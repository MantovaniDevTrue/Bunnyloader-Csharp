// The production loader is executed without an IL2CPP game. Every game API double aborts:
// a service-only package must never call one, even to manufacture a placeholder item.
#include "../../../app/src/main/cpp/experiments/csharp/SourceLoader.cpp"
#include <cassert>
#include <cstdlib>
#include <cstdio>
namespace bl::log { void write(int, const char*, ...) {} }
namespace bl::csharp {
bool prepareNativeContent() { std::abort(); }
int registerNativeContent(void*, const NativeContentSpec*) { std::abort(); }
void reserveManagedPrefixDescriptor(int) { std::abort(); }
bool isBasicItem(int) { std::abort(); }
bool setBasicItemDefense(int, int, std::string*) { std::abort(); }
bool applyNativeBasicItemDefaults(const std::string&, const std::vector<BasicItemEntry>&, std::string*) { std::abort(); }
bool applyManagedPrefixDescriptors(const std::string&, const std::vector<ManagedPrefixEntry>&) { std::abort(); }
bool applyManagedNativeDefaultsBatch(const std::string&, const std::vector<ManagedProjectileEntry>&, const std::vector<ManagedWeaponEntry>&) { std::abort(); }
bool applyManagedNativeNpcDefaultsBatch(const std::string&, const std::vector<ManagedNpcEntry>&) { std::abort(); }
bool applyManagedNativeBuffDefaultsBatch(const std::string&, const std::vector<ManagedBuffEntry>&, const std::vector<ManagedPotionEntry>&) { std::abort(); }
bool applyManagedNativeTileDefaultsBatch(const std::string&, const std::vector<ManagedTileEntry>&) { std::abort(); }
bool applyManagedNativePlayerDefaultsBatch(const std::string&, const std::vector<ManagedAccessoryEntry>&, const std::vector<ManagedPlayerEntry>&) { std::abort(); }
}
namespace bl::runtime {
int modPrefixByKey(const std::string&) { std::abort(); }
int modItemTypeByKey(const std::string&) { std::abort(); }
int modNpcTypeByName(const std::string&, const std::string&) { std::abort(); }
int modBuffTypeByName(const std::string&, const std::string&) { std::abort(); }
int modTileTypeByName(const std::string&, const std::string&) { std::abort(); }
int registerModPrefix(const std::string&, const std::string&) { std::abort(); }
void setModItemTooltip(int, std::vector<std::pair<std::string, std::string>>) { std::abort(); }
}
int main(int argc, char** argv) {
    using namespace bl::csharp;
    assert(argc == 2);
    bl::mods::LoadedMod mod;mod.id="service-only";mod.dir=argv[1];
    assert(loadCSharpSourceMod(mod));
    auto status=sourceModStatuses();
    assert(status.size()==1 && status[0].loaded && status[0].managedContentDeclarations==1);
    assert(status[0].registeredTypes.empty() && status[0].registeredKindTypes.empty());
    assert(status[0].items==0 && status[0].npcs==0 && status[0].projectiles==0 && !status[0].runtimeApplied);
    std::string error;
    assert(!applyManagedSourceDefaults(mod.id,"INVALID_HEADER\n",&error));
    assert(!sourceModStatuses()[0].runtimeApplied);
    assert(applyManagedSourceDefaults(mod.id,"BUNNY_MANAGED_DEFAULTS_V2\n",&error));
    assert(sourceModStatuses()[0].runtimeApplied);
    assert(!applyManagedSourceDefaults(mod.id,"BUNNY_MANAGED_DEFAULTS_V2\n",&error));
    std::puts("SERVICE_ONLY_PRODUCTION_SOURCE_LOADER_NO_GAME_API_NO_PLACEHOLDER_PASS");
}
