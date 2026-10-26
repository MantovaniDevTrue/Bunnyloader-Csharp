#include <array>
#include "content/common/ContentAssets.h"
#include "experiments/csharp/NativeContent.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <map>
#include "native_policy_support.h"
namespace bl::testing {
std::string modContentRoot;
std::vector<csharp::ManagedVisualCommand> visualCommands;
std::map<Il2CppObject*,std::pair<int,int>> assetSizes;
Il2CppClass* assetClass=nullptr;
Il2CppObject* (*loadAsset)(const std::string&)=nullptr;
std::vector<csharp::ManagedNativeEquipTexture> legacyEquips;
std::map<int,int> legacySlots;
}
namespace bl::runtime::content {
Il2CppClass* textureAssetClass(){return testing::assetClass;}
Il2CppObject* loadTextureAsset(const std::string& path,const unsigned char*,size_t,const std::string&,int*,int*){return testing::loadAsset?testing::loadAsset(path):nullptr;}
bool textureAssetSize(Il2CppObject* object,int* width,int* height){const auto found=testing::assetSizes.find(object);if(found==testing::assetSizes.end())return false;*width=found->second.first;*height=found->second.second;return true;}
Il2CppObject* textureAssetValue(Il2CppObject* asset){return asset;}
void destroyTextureAsset(Il2CppObject*){}
bool setTableElement(FieldInfo* f,int index,Il2CppObject* value) {
    Il2CppArray* array=nullptr;il2cpp::api().field_static_get_value(f,&array);
    if(!array||index<0||static_cast<uintptr_t>(index)>=array->length)return false;
    auto* cls=il2cpp::findClass({"System","Array",{}});
    const auto* set=il2cpp::findMethodBySignature(cls,il2cpp::parseSignature("void SetValue(object value, int index)"));
    Il2CppObject* error=nullptr;void* args[]={value,&index};
    il2cpp::api().runtime_invoke(set,array,args,&error);return !error;
}
}
namespace bl::runtime {
int modBuffTypeByName(const std::string&,const std::string&){return 400;}
}
namespace bl::csharp {
bool nativeCostumeRuntimeReady(){return true;}
const char* nativeCostumeRuntimeDiagnostic(){return "Ready";}
size_t activeManagedVisualCount(){return testing::visualCommands.size();}
void visitManagedVisualCommands(int player,void (*visitor)(const ManagedVisualCommand&,void*),void* context,size_t limit){size_t count=0;for(const auto& command:testing::visualCommands)if(player<0||player==command.playerIndex){if(count++==limit)break;visitor(command,context);}}
std::vector<ManagedNativeEquipTexture> managedNativeEquipTextures(){std::vector<ManagedNativeEquipTexture> out;for(const auto& e:testing::legacyEquips)if(!testing::legacySlots.contains(e.itemType))out.push_back(e);return out;}
size_t pendingManagedNativeEquipTextures(){size_t n=0;for(const auto& e:testing::legacyEquips)if(!testing::legacySlots.contains(e.itemType))++n;return n;}
bool setManagedNativeEquipSlot(int type,int,int slot){testing::legacySlots[type]=slot;return true;}
}
namespace bl::mods {const std::string& dirOf(const std::string&) {return testing::modContentRoot;}
std::string rootOf(const std::string&){const auto& root=testing::modContentRoot;return root.ends_with("/content")?root.substr(0,root.size()-8):root;}
}
namespace bl::il2cpp {std::string describeException(Il2CppObject*) {return "FixtureNativeException";}}

namespace bl::csharp {
bool nativeMountBalloonState(Il2CppObject*,std::array<float,3>*,Il2CppObject**){return false;}
}
