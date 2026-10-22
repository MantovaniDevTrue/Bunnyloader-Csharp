#pragma once

#include "experiments/csharp/ManagedBannerArrayBridge.h"
#include "experiments/csharp/ManagedBannerWorldState.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace bl::csharp::banners {

struct GameCounterArrays {
    Il2CppArray* kills = nullptr;
    Il2CppArray* claimable = nullptr;
    Il2CppArray* fresh = nullptr;
    Il2CppClass* bannerClass = nullptr;
};

inline bool resolveBannerGameArrays(GameCounterArrays* out, std::string* error) {
    if (!out || !error) return false;
    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.field_get_flags) {
        *error = "BannerSystem: exports de campos indisponiveis";
        return false;
    }
    auto* cls = il2cpp::findClass({"Terraria.GameContent", "BannerSystem", {}});
    if (!cls) {
        *error = "BannerSystem: classe nativa inexistente";
        return false;
    }
    const char* fields[] = {"killCount", "claimableBanners", "newBanner"};
    Il2CppArray* values[3]{};
    for (int i = 0; i < 3; ++i) {
        auto* field = il2cpp::findField(cls, fields[i]);
        if (!field || (api.field_get_flags(field) & 0x10u) == 0) {
            *error = std::string("BannerSystem: campo invalido: ") + fields[i];
            return false;
        }
        api.field_static_get_value(field, &values[i]);
        if (!il2cpp::nativeArrayContract(values[i], i == 2 ? "Boolean" : "Int32",
                                         i == 2 ? 1 : 4, 262144)) {
            *error = std::string("BannerSystem: array invalido: ") + fields[i];
            return false;
        }
    }
    if (values[0]->length < runtime::kVanillaNpcCount ||
        values[1]->length < runtime::kVanillaNpcCount ||
        values[2]->length < runtime::kVanillaNpcCount) {
        *error = "BannerSystem: arrays menores do que os IDs vanilla";
        return false;
    }
    *out = {values[0], values[1], values[2], cls};
    return true;
}

inline void copyBannerArrays(const GameCounterArrays& arrays,
                             std::vector<int32_t>* kills,
                             std::vector<int32_t>* claimable,
                             std::vector<uint8_t>* fresh) {
    kills->resize(arrays.kills->length);
    claimable->resize(arrays.claimable->length);
    fresh->resize(arrays.fresh->length);
    std::memcpy(kills->data(), arrayData(arrays.kills), kills->size() * sizeof(int32_t));
    std::memcpy(claimable->data(), arrayData(arrays.claimable),
                claimable->size() * sizeof(int32_t));
    std::memcpy(fresh->data(), arrayData(arrays.fresh), fresh->size());
}

inline bool captureGameBannerCounters(WorldEntries* retained, std::string* error) {
    if (!retained || !error) return false;
    GameCounterArrays arrays;
    if (!resolveBannerGameArrays(&arrays, error)) return false;
    std::vector<int32_t> kills, claimable;
    std::vector<uint8_t> fresh;
    copyBannerArrays(arrays, &kills, &claimable, &fresh);
    if (!WorldLedger::capture(*registry(runtime::kVanillaNpcCount).snapshot(),
                              kills, claimable, fresh, retained)) {
        *error = "BannerSystem: IDs de NPC fora do limite ou sidecar lotado";
        return false;
    }
    return true;
}

inline bool restoreGameBannerCounters(const WorldEntries& saved, std::string* error) {
    if (!error) return false;
    GameCounterArrays arrays;
    if (!resolveBannerGameArrays(&arrays, error)) return false;

    // Nao tocar nos arrays sem resolver TODOS os refreshes originais.
    auto& api = il2cpp::api();
    if (!api.runtime_invoke || !api.field_static_get_value ||
        !api.field_static_set_value) {
        *error = "BannerSystem: runtime de refresh indisponivel";
        return false;
    }
    auto* has = il2cpp::findMethodBySignature(
        arrays.bannerClass, il2cpp::parseSignature("void RefreshHasBannerAvailable()"));
    auto* freshMethod = il2cpp::findMethodBySignature(
        arrays.bannerClass, il2cpp::parseSignature("void RefreshAnyNewClaimableBanners()"));
    FieldInfo* stateField = il2cpp::findField(arrays.bannerClass, "bannerState");
    if (!has || !freshMethod || !stateField ||
        (api.field_get_flags(stateField) & 0x10u) == 0) {
        *error = "BannerSystem: refresh ou bannerState nao verificado";
        return false;
    }

    std::vector<int32_t> kills, claimable;
    std::vector<uint8_t> fresh;
    copyBannerArrays(arrays, &kills, &claimable, &fresh);
    const auto bindings = registry(runtime::kVanillaNpcCount).snapshot();
    if (!WorldLedger::restore(*bindings, saved, &kills, &claimable, &fresh)) {
        *error = "BannerSystem: world ledger invalido ou sem slots suficientes";
        return false;
    }
    auto* killDst = static_cast<int32_t*>(arrayData(arrays.kills));
    auto* claimDst = static_cast<int32_t*>(arrayData(arrays.claimable));
    auto* freshDst = static_cast<uint8_t*>(arrayData(arrays.fresh));
    struct Old {
        int id;
        int32_t kills;
        int32_t claimable;
        uint8_t fresh;
    };
    std::vector<Old> previous;
    previous.reserve(bindings->bannerToItem.size());
    for (const auto& [id, item] : bindings->bannerToItem) {
        previous.push_back({id, killDst[id], claimDst[id], freshDst[id]});
        killDst[id] = kills[id];
        claimDst[id] = claimable[id];
        freshDst[id] = fresh[id] ? 1 : 0;
    }
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(has, nullptr, nullptr, &exception);
    if (!exception)
        api.runtime_invoke(freshMethod, nullptr, nullptr, &exception);
    if (exception) {
        for (const auto& entry : previous) {
            killDst[entry.id] = entry.kills;
            claimDst[entry.id] = entry.claimable;
            freshDst[entry.id] = entry.fresh;
        }
        *error = "BannerSystem: refresh nativo lancou excecao, arrays restaurados";
        return false;
    }
    int32_t revision = 0;
    api.field_static_get_value(stateField, &revision);
    revision = revision == std::numeric_limits<int32_t>::max() ? 0 : revision + 1;
    api.field_static_set_value(stateField, &revision);
    return true;
}

} // namespace bl::csharp::banners
