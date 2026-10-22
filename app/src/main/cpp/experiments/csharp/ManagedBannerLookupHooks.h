#pragma once

#include "experiments/csharp/ManagedBannerRegistry.h"
#include "experiments/csharp/ManagedBannerNearbyPolicy.h"
#include "content/npcs/ModNpcs.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "core/Log.h"

#include <atomic>
#include <mutex>

namespace bl::csharp::banners {

// Estes tres metodos sao STATIC no BannerSystem do Terraria Android.
// A assinatura completa (inclusive o MethodInfo* final) segue os hooks
// nativos usados por ModNpcs e NativeContent. Os originais sao lidos no
// momento da chamada, pois outros mods podem encadear hooks depois.
using BannerLookupMethod = int32_t (*)(int32_t, const MethodInfo*);

inline BannerLookupMethod originalNpcToBanner = nullptr;
inline BannerLookupMethod originalBannerToItem = nullptr;
inline BannerLookupMethod originalBannerToNpc = nullptr;
using BannerClearMethod = void (*)(const MethodInfo*);
inline BannerClearMethod originalBannerClear = nullptr;
inline std::atomic<bool> lookupsActive{false};

inline void hookBannerClear(const MethodInfo* method) {
    // Clear reinicializa o progresso vanilla antes do novo mundo.
    // Fechamos imediatamente o gate C#; arrays de mod so voltam a ser
    // publicados apos crescer e restaurar todos os dados da nova sessao.
    lookupsActive.store(false, std::memory_order_release);
    nearbyBannerCache().reset();
    if (originalBannerClear) originalBannerClear(method);
}
inline std::mutex hookInstallationGate;
inline bool hooksInstalled = false;

inline int32_t lookupNpcToBanner(int32_t npc, const MethodInfo* method) {
    const auto original = originalNpcToBanner;
    if (!original) return 0;
    if (lookupsActive.load(std::memory_order_acquire) &&
        npc >= runtime::kVanillaNpcCount) {
        const int value = registry(runtime::kVanillaNpcCount).npcToBanner(npc);
        if (value >= runtime::kVanillaNpcCount) return value;
    }
    return original(npc, method);
}

inline int32_t lookupBannerToItem(int32_t banner, const MethodInfo* method) {
    const auto original = originalBannerToItem;
    if (!original) return 0;
    if (lookupsActive.load(std::memory_order_acquire) &&
        banner >= runtime::kVanillaNpcCount) {
        const int value = registry(runtime::kVanillaNpcCount).bannerToItem(banner);
        if (value > 0) return value;
    }
    return original(banner, method);
}

inline int32_t lookupBannerToNpc(int32_t banner, const MethodInfo* method) {
    const auto original = originalBannerToNpc;
    if (!original) return 0;
    if (lookupsActive.load(std::memory_order_acquire) &&
        banner >= runtime::kVanillaNpcCount) {
        const int value = registry(runtime::kVanillaNpcCount).bannerToNpc(banner);
        if (value >= runtime::kVanillaNpcCount) return value;
    }
    return original(banner, method);
}

// Preflight sem instalar nada: se qualquer metodo nao casar, nao altera
// o Terraria. A instalacao parcial tambem permanece desativada quando
// ocorrer uma falha posterior, e uma tentativa futura nao duplica os hooks.
inline bool installBannerLookupHooks() {
    std::lock_guard<std::mutex> lock(hookInstallationGate);
    if (hooksInstalled) return true;
    using namespace bl::il2cpp;
    Il2CppClass* cls = findClass({"Terraria.GameContent", "BannerSystem", {}});
    if (!cls) {
        BL_ERROR("[CSharp][BannerLoader] BannerSystem nao foi encontrado");
        return false;
    }
    const MethodInfo* npcToBanner = findMethodBySignature(
        cls, parseSignature("int NPCtoBanner(int i)"));
    const MethodInfo* bannerToItem = findMethodBySignature(
        cls, parseSignature("int BannerToItem(int banner)"));
    const MethodInfo* bannerToNpc = findMethodBySignature(
        cls, parseSignature("int BannerToNPC(int i)"));
    const MethodInfo* clear = findMethodBySignature(
        cls, parseSignature("void Clear()"));
    if (!npcToBanner || !bannerToItem || !bannerToNpc || !clear) {
        BL_ERROR("[CSharp][BannerLoader] Assinaturas IL2CPP nao encontradas, nenhum lookup ativo");
        return false;
    }

    if (!originalNpcToBanner &&
        !bl::hook::install(npcToBanner, lookupNpcToBanner, &originalNpcToBanner))
        return false;
    if (!originalBannerToItem &&
        !bl::hook::install(bannerToItem, lookupBannerToItem, &originalBannerToItem))
        return false;
    if (!originalBannerToNpc &&
        !bl::hook::install(bannerToNpc, lookupBannerToNpc, &originalBannerToNpc))
        return false;
    if (!originalBannerClear &&
        !bl::hook::install(clear, hookBannerClear, &originalBannerClear))
        return false;
    hooksInstalled = true;
    BL_INFO("[CSharp][BannerLoader] 3 consultas e Clear instalados, sem ativacao do gameplay");
    return true;
}

// So ativar depois que *todos* os arrays, sidecar, refresh e rede nativos
// forem publicados juntos na thread do jogo. O lote atual nunca chama true.
// Em unload de todos os mods C#, a volta ao vanilla e imediata.
inline void setBannerLookupsActive(bool enable) {
    lookupsActive.store(enable && hooksInstalled, std::memory_order_release);
}

} // namespace bl::csharp::banners
