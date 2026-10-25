#pragma once
#include "experiments/csharp/BasicItemPolicy.h"
#include "il2cpp/Types.h"
#include <memory>
namespace bl::csharp {
struct NativeItemPolicyBatch;
struct NativeCostumeEffect {
    int buffType=-1;
    int normal[3]={-1,-1,-1};
    int alternate[3]={-1,-1,-1};
    int extraFall=0,requiredTownNpcs=1;
    float jumpSpeedBoost=0;
};
bool nativeCostumeEffect(int itemType,NativeCostumeEffect* effect);
bool prepareNativeItemPolicies(const std::vector<BasicItemEntry>& entries,std::shared_ptr<NativeItemPolicyBatch>* batch,std::string* error);
void commitNativeItemPolicies(const std::shared_ptr<NativeItemPolicyBatch>& batch);
// Revoke callbacks now; game-thread pump restores owned primitive cells and releases roots.
void retainNativeItemPoliciesForMod(const std::string& mod,const std::vector<int>& activeTypes);
void retireNativeItemPoliciesForMod(const std::string& mod);
void pumpNativeItemPoliciesOnGameThread();
bool applyNativeItemPolicyDefaults(Il2CppObject* item,int type);
bool nativeItemPolicyReady(int type);
size_t pendingNativeItemPolicies();
struct NativeItemPolicyStatus { int type=-1;std::string mod,name,phase,error;bool ready=false; };
std::vector<NativeItemPolicyStatus> nativeItemPolicyStatus();
}
