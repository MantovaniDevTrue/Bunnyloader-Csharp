#pragma once
#include "experiments/csharp/BasicItemDefaults.h"
#include "il2cpp/Types.h"
#include <memory>

namespace bl::csharp {
struct NativeArmorBatch;
std::shared_ptr<NativeArmorBatch> prepareNativeArmorPolicies(const std::string& mod,
    const std::vector<BasicItemEntry>& items,std::string* error);
void commitNativeArmorPolicies(const std::shared_ptr<NativeArmorBatch>& batch);
void pumpNativeArmorPoliciesOnGameThread();
bool nativeArmorPolicyReady(int type);
int pendingNativeArmorPolicies();
struct NativeArmorPolicyStatus { int type=-1; bool ready=false; std::string mod,name,phase,error; };
std::vector<NativeArmorPolicyStatus> nativeArmorPolicyStatuses();
}
