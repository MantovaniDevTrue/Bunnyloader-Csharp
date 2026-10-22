#pragma once

#include "experiments/csharp/ManagedBannerLookupHooks.h"
#include "experiments/csharp/ManagedBannerSaveDispatch.h"
#include "experiments/csharp/ManagedBannerSaveSlice.h"

#include <mutex>

namespace bl::csharp::banners {

using BannerSaveFn = void (*)(Il2CppObject*, const MethodInfo*);
using FullStateFn = Il2CppObject* (*)(const MethodInfo*);

inline BannerSaveFn originalBannerSave = nullptr;
inline FullStateFn originalBannerNetwork = nullptr;
inline std::mutex saveHookLock;
inline bool saveHooksInstalled = false;
inline std::mutex bannerSaveSerializationMutex;
inline BannerSaveFields bannerVanillaSaveFields{};

inline void hookBannerSave(Il2CppObject* writer, const MethodInfo* method) {
    const auto original = originalBannerSave;
    if (!original) return;
    if (SaveDispatch::networkDepth() != 0) {
        original(writer,method);
        return;
    }
    std::lock_guard<std::mutex> saveGuard(bannerSaveSerializationMutex);
    ScopedVanillaBannerSave slice(bannerVanillaSaveFields);
    std::string error;
    if (!slice.begin(&error)) {
        lookupsActive.store(false,std::memory_order_release);
        BL_ERROR("[CSharp][BannerLoader] Save vanilla nao pode serializar mod arrays: %s",
                 error.c_str());
        return;
    }
    original(writer, method);
    saveDispatch().requestFromWorldSave(
        lookupsActive.load(std::memory_order_acquire) && saveHooksInstalled);
}

inline Il2CppObject* hookBannerWriteFullState(const MethodInfo* method) {
    SaveDispatch::NetworkScope networkScope;
    const auto original = originalBannerNetwork;
    return original ? original(method) : nullptr;
}

// Preflight exato dos dois metodos do Terraria Android. Somente
// chamadas reais de BannerSystem.Save fora de WriteFullState agendam a
// gravacao no mundo, e a gravacao em si ocorre na thread principal.
inline bool installBannerSaveHooks() {
    std::lock_guard<std::mutex> lock(saveHookLock);
    if (saveHooksInstalled) return true;
    using namespace bl::il2cpp;
    auto* banner = findClass({"Terraria.GameContent", "BannerSystem", {}});
    auto* network = findNested(banner, "NetBannersModule");
    if (!banner || !network) {
        BL_ERROR("[CSharp][BannerLoader] NetBannersModule nao encontrado");
        return false;
    }
    const MethodInfo* save = findMethodBySignature(
        banner, parseSignature("void Save(BinaryWriter writer)"));
    const MethodInfo* full = findMethodBySignature(
        network, parseSignature("NetPacket WriteFullState()"));
    auto& api = il2cpp::api();
    if (!save || !full || !api.method_get_flags ||
        (api.method_get_flags(save, nullptr) & 0x10u) == 0 ||
        (api.method_get_flags(full, nullptr) & 0x10u) == 0) {
        BL_ERROR("[CSharp][BannerLoader] Save/WriteFullState estaticos nao verificados");
        return false;
    }
    BannerSaveFields fields{};
    std::string error;
    if (!resolveVanillaBannerSaveFields(&fields,&error)) {
        BL_ERROR("[CSharp][BannerLoader] world-save preflight falhou: %s",error.c_str());
        return false;
    }
    bannerVanillaSaveFields=fields;
    if (!originalBannerSave &&
        !bl::hook::install(save, hookBannerSave, &originalBannerSave))
        return false;
    if (!originalBannerNetwork &&
        !bl::hook::install(full, hookBannerWriteFullState, &originalBannerNetwork))
        return false;
    saveHooksInstalled = true;
    return true;
}

} // namespace bl::csharp::banners
