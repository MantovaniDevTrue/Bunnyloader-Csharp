#include "content/tiles/ModWater.h"
#include "content/tiles/WaterAlphaTables.h"
#include "core/Log.h"
#include "hook/CodePatchTransaction.h"
#include "hook/HookManager.h"
#include "boot/Boot.h"
#include "content/common/ContentAssets.h"
#include "il2cpp/Api.h"
#include "il2cpp/CheckedProperty.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <algorithm>
#include <array>
#include <unistd.h>

namespace bl::runtime {

namespace {

// O primeiro laco anda pelo endereco do elemento em floats: o indice + 8
// (os 32 bytes do cabecalho do array).
constexpr int kHeaderInFloats = 8;

int g_count = kVanillaWaterStyleCount;
using WaterDraw=void(*)(void*,bool,const MethodInfo*);
WaterDraw originalDraw=nullptr;
const MethodInfo* guardedMethod=nullptr;

bool waterFailure(const char* stage,const std::string& detail) {
    thread_local std::string previous;
    thread_local uint64_t repeats=0,lastLogged=0;
    const std::string current=std::string(stage)+":"+detail;
    if(current!=previous){previous=current;repeats=0;lastLogged=0;}
    ++repeats;
    if(repeats==1 || (repeats&(repeats-1))==0) {
        BL_ERROR("[Native][ERROR][WaterStyle] API=WaterDrawLimits Stage=%s Content=Terraria.Main.DrawWaters Result=Rejected SuppressedSinceLast=%llu Detail=%s",
            stage,static_cast<unsigned long long>(repeats-lastLogged-1),detail.c_str());lastLogged=repeats;
    }
    return false;
}

void guardedDraw(void* self,bool background,const MethodInfo* method) {
    std::string error;
    // These arrays are recreated with the local user state. A previously
    // patched native loop keeps its larger bound across world changes.
    if(!ensureWaterAlphaTables(g_count,&error)){waterFailure("BeforeDrawWaters",error);return;}
    if(originalDraw)originalDraw(self,background,method);
}

bool installDrawGuard(const MethodInfo* method) {
    if(guardedMethod==method)return true;
    if(guardedMethod)return waterFailure("InstallDrawGuard","Expected=OriginalVerifiedMethod Actual=ChangedMethod Reason=ProcessRestartRequired");
    if(!hook::install(method,guardedDraw,&originalDraw))
        return waterFailure("InstallDrawGuard","Expected=NativeDrawGuardInstalled Actual=HookRejected Count="+
            std::to_string(g_count)+" Reason=NativeHookInstallationRejected Consequence=BothLoopsAppliedRetryGuardRequired");
    guardedMethod=method;return true;
}

bool tablesReady(int total,Il2CppClass* main,std::string* error) {
    using P=il2cpp::CheckedProperty;using K=P::Kind;
    auto* liquid=il2cpp::findClassQuiet("Terraria.GameContent.Liquid","LiquidRenderer");
    auto* textures=il2cpp::findClassQuiet("Terraria.GameContent","TextureAssets");
    auto* asset=content::textureAssetClass();
    const std::array<Il2CppClass*,5> owners={liquid,textures,textures,main,main};
    const std::array<const char*,5> names={"_liquidTextures","Liquid","LiquidSlope","liquidAlpha","activeLiquidAlpha"};
    for(size_t i=0;i<owners.size();++i) {
        P property;
        if(!property.bind(owners[i],names[i],i<3?K::AssetArray:K::SingleArray,true,error,i<3?asset:nullptr))return false;
        Il2CppArray* array=nullptr;
        if(!property.read(nullptr,&array,error) || !property.array(array,static_cast<size_t>(total),error))return false;
        if(i<3)for(int slot=kVanillaWaterStyleCount;slot<total;++slot) {
            auto* value=static_cast<Il2CppObject**>(arrayData(array))[slot];
            if(!value || !il2cpp::api().class_is_assignable_from(asset,il2cpp::api().object_get_class(value)))
                return property.reject(error,"ValidateSlot","LoadedCompatibleTextureAsset",value?P::name(il2cpp::api().object_get_class(value)):"Null",
                    "WaterTextureSlotUnavailable");
        }
    }
    return true;
}

} // namespace

bool setWaterStyleCount(int total) {
    if(total<kVanillaWaterStyleCount || total>kMaxWaterStyleCount)
        return waterFailure("ValidateCount","Expected=15.."+std::to_string(kMaxWaterStyleCount)+" Actual="+std::to_string(total)+" Reason=CountOutsideCompiledImmediateRange");
    if(gameThreadId()<=0 || gameThreadId()!=static_cast<int>(gettid()))
        return waterFailure("ThreadCheck","Expected=ObservedUnityThread:"+std::to_string(gameThreadId())+" Actual="+std::to_string(gettid())+" Reason=WrongThread");
#if defined(__ANDROID__) && !defined(__aarch64__)
    return waterFailure("BindCode","Expected=AArch64 Actual=OtherArchitecture Reason=UnsupportedInstructionSet");
#endif
    std::string error;
    if(!repairCodePatchProtection(&error))return waterFailure("RestoreProtection",error);
    auto& a = il2cpp::api();
    Il2CppClass* main = il2cpp::findClassQuiet("Terraria","Main");
    // Resolve the complete overload and instance storage before interpreting
    // its machine code. Equal parameter counts do not prove ABI compatibility.
    if(!a.method_get_flags || !a.method_get_param_count || !a.method_get_param ||
       !a.method_get_return_type || !a.class_value_size || !a.class_is_valuetype ||
       !a.class_from_il2cpp_type || !a.class_get_name || !a.class_get_namespace ||
       !a.type_get_name || !a.il2cpp_free || !a.class_get_methods || !a.method_get_name)
        return waterFailure("BindMethod","Expected=CompleteTypedMethodMetadata Actual=MissingExports Reason=MetadataUnavailable");
    const MethodInfo* m=main?il2cpp::findMethodBySignature(main,il2cpp::parseSignature("void DrawWaters(bool isBackground)")):nullptr;
    if(!m || (a.method_get_flags(m,nullptr)&0x10u) || (a.method_is_generic && a.method_is_generic(m)))
        return waterFailure("BindMethod","Expected=Instance:void DrawWaters(bool isBackground) Actual="+
            (m?il2cpp::describeMethod(m):std::string("MissingOrAmbiguousOverload"))+" Reason=NativeMethodContractMismatch");
    auto* parameter=a.class_from_il2cpp_type(a.method_get_param(m,0));uint32_t alignment=0;
    const int parameterBytes=parameter && a.class_is_valuetype(parameter)?a.class_value_size(parameter,&alignment):-1;
    if(parameterBytes!=1)
        return waterFailure("BindMethod","Field=isBackground Expected=System.Boolean:1 Actual="+il2cpp::CheckedProperty::name(parameter)+":"+std::to_string(parameterBytes)+" Reason=NativeTypeOrSizeMismatch");
    // The count is monotonic: even a smaller later request must validate the
    // full published span. No loop may run beyond freshly recreated tables.
    if(!ensureWaterAlphaTables(std::max(total,g_count),&error))return waterFailure("PrepareAlphaTables",error);
    if(!tablesReady(std::max(total,g_count),main,&error))return waterFailure("ValidateTables",error);
    if(total<=g_count)return installDrawGuard(m);
    const auto from = static_cast<uint32_t>(g_count), to = static_cast<uint32_t>(total);
    const std::array<LoopLimitPatch,2> changes={LoopLimitPatch{from+kHeaderInFloats,to+kHeaderInFloats,false},
                                                LoopLimitPatch{from,to,true}};
    const auto result=patchLoopLimits(m,changes);
    if(!result.applied)return waterFailure("PatchLoops",result.detail);
    // Both instructions are now committed. Even an OS protection-restore
    // failure must retain the real count, so retries cannot patch from stale 15.
    const int previous=g_count;g_count=total;
    if(!result.permissionsRestored)
        return waterFailure("RestoreProtection",result.detail+" Consequence=BothLoopsApplied Count="+std::to_string(g_count));
    BL_INFO("agua de mod: Main.DrawWaters anda por %d estilos (eram %d)", total, previous);
    return installDrawGuard(m);
}

} // namespace bl::runtime
