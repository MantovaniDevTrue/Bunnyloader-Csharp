#pragma once

#include "experiments/csharp/ManagedBannerRegistry.h"
#include "content/common/TypeTables.h"
#include "content/npcs/ModNpcs.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace bl::csharp::banners {

inline int requiredArraySlots(const Snapshot& snapshot, int npcFrames) {
    int needed = std::max(runtime::kVanillaNpcCount, npcFrames);
    if (!snapshot.bannerToItem.empty())
        needed = std::max(needed, snapshot.bannerToItem.rbegin()->first + 1);
    return needed > 0 && needed <= 262144 ? needed : -1;
}

// Esta ponte deve ser chamada na THREAD DO JOGO, antes do uso de contadores
// novos pelo BannerSystem. Nao instala hooks e nao ativa os lookups sozinha.
inline bool growGameArraysOnMainThread(std::string* error) {
    if (!error) return false;
    error->clear();
    auto state = registry(runtime::kVanillaNpcCount).snapshot();
    if (state->bannerToItem.empty()) return true;

    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.field_static_set_value ||
        !a.gc_wbarrier_set_field || !a.gchandle_new || !a.gchandle_free) {
        *error = "IL2CPP: API de campos, barreiras e GC handles ausente";
        return false;
    }
    auto* banner = il2cpp::findClass({"Terraria.GameContent", "BannerSystem", {}});
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* scene = il2cpp::findClass({"Terraria", "SceneMetrics", {}});
    if (!banner || !main || !scene) {
        *error = "BannerSystem/Main/SceneMetrics nao foi resolvido";
        return false;
    }
    auto* frameField = il2cpp::findField(main, "npcFrameCount");
    auto* sceneField = il2cpp::findField(main, "SceneMetrics");
    auto* playerSceneField = il2cpp::findField(main, "PlayerSceneMetrics");
    auto* sceneArrayField = il2cpp::findField(scene, "NPCBannerBuff");
    if (!frameField || !sceneField || !playerSceneField || !sceneArrayField ||
        (a.field_get_flags(frameField) & 0x10u) == 0 ||
        (a.field_get_flags(sceneField) & 0x10u) == 0 ||
        (a.field_get_flags(playerSceneField) & 0x10u) == 0 ||
        (a.field_get_flags(sceneArrayField) & 0x10u) != 0) {
        *error = "Main/SceneMetrics: campos nao coincidem com o dump";
        return false;
    }
    Il2CppArray* frames = nullptr;
    Il2CppObject* sceneObj = nullptr;
    Il2CppObject* playerSceneObj = nullptr;
    a.field_static_get_value(frameField, &frames);
    a.field_static_get_value(sceneField, &sceneObj);
    a.field_static_get_value(playerSceneField, &playerSceneObj);
    if (!frames || !sceneObj || !playerSceneObj ||
        a.object_get_class(sceneObj) != scene ||
        a.object_get_class(playerSceneObj) != scene ||
        !il2cpp::nativeArrayContract(frames, "Int32", 4, 262144)) {
        *error = "SceneMetrics ou frames ainda nao iniciados/compativeis";
        return false;
    }

    const int desired = requiredArraySlots(*state, static_cast<int>(frames->length));
    if (desired < 0) {
        *error = "Limite das tabelas de bandeira excedido";
        return false;
    }
    const int offset = il2cpp::fieldOffset(scene, "NPCBannerBuff");
    if (offset < static_cast<int>(sizeof(Il2CppObject))) {
        *error = "SceneMetrics.NPCBannerBuff: offset invalido";
        return false;
    }
    struct Field {
        FieldInfo* staticField = nullptr;
        Il2CppObject* owner = nullptr;
        Il2CppArray** instanceSlot = nullptr;
        Il2CppArray* old = nullptr;
        Il2CppArray* replacement = nullptr;
        uint32_t root = 0;
        const char* element = "";
        int width = 0;
    };
    std::array<Field, 5> all{};
    const std::array<const char*, 3> names{
        "killCount", "claimableBanners", "newBanner"
    };
    for (int i = 0; i < 3; ++i) {
        auto* field = il2cpp::findField(banner, names[i]);
        if (!field || (a.field_get_flags(field) & 0x10u) == 0) {
            *error = std::string("BannerSystem: campo nao estatico ") + names[i];
            return false;
        }
        all[i].staticField = field;
        all[i].element = i == 2 ? "Boolean" : "Int32";
        all[i].width = i == 2 ? 1 : 4;
        a.field_static_get_value(field, &all[i].old);
    }
    for (int i = 3; i < 5; ++i) {
        auto& field = all[i];
        field.owner = i == 3 ? sceneObj : playerSceneObj;
        field.instanceSlot = reinterpret_cast<Il2CppArray**>(
            reinterpret_cast<char*>(field.owner) + offset);
        field.old = *field.instanceSlot;
        field.element = "Boolean";
        field.width = 1;
    }
    for (auto& field : all) {
        if (!il2cpp::nativeArrayContract(field.old, field.element,
                                         field.width, 262144)) {
            *error = "BannerSystem/SceneMetrics: elemento ou rank de array invalido";
            return false;
        }
    }

    struct Roots {
        std::array<Field, 5>& fields;
        decltype(a)& api;
        ~Roots() {
            for (auto& field : fields)
                if (field.root) api.gchandle_free(field.root);
        }
    } roots{all, a};

    // Aloca e protege TODAS as novas tabelas antes de alterar uma unica
    // referencia do jogo. GC pode acontecer entre duas alocacoes.
    for (auto& field : all) {
        if (field.old->length >= static_cast<uintptr_t>(desired)) continue;
        field.replacement = runtime::TypeTables::resizedCopy(field.old, desired);
        if (!field.replacement ||
            !il2cpp::nativeArrayContract(field.replacement, field.element,
                                         field.width, 262144)) {
            *error = "BannerSystem: alocacao de array invalida/insuficiente";
            return false;
        }
        field.root = a.gchandle_new(
            reinterpret_cast<Il2CppObject*>(field.replacement), false);
        if (!field.root) {
            *error = "BannerSystem: nao foi possivel proteger novo array no GC";
            return false;
        }
    }

    // Os cinco slots sao publicados na mesma thread do jogo. Nenhum hook de
    // mod deve usar indices acima do vanilla antes de esta rotina concluir.
    for (auto& field : all) {
        if (!field.replacement) continue;
        if (field.staticField)
            a.field_static_set_value(field.staticField, field.replacement);
        else
            a.gc_wbarrier_set_field(field.owner,
                                   reinterpret_cast<void**>(field.instanceSlot),
                                   field.replacement);
    }
    return true;
}

} // namespace bl::csharp::banners
