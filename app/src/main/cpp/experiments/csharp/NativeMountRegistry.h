#pragma once
#include "experiments/csharp/NativeMountPolicy.h"
#include "il2cpp/Types.h"
#include <array>
#include <string>
#include <vector>

namespace bl::csharp {
struct ManagedMountEntry {
    std::string name;
    int type=-1;
    mount_policy::Definition values{};
};
int reserveNativeMount(const std::string& mod,const std::string& name,std::string* error);
int nativeMountType(const std::string& mod,const std::string& name);
bool publishNativeMounts(const std::string& mod,const std::string& directory,
                         const std::vector<ManagedMountEntry>& entries,std::string* error);
bool nativeMountBalloonState(Il2CppObject* player,std::array<float,3>* rotations,Il2CppObject** texture);
void retireNativeMounts(const std::string& mod);
}
