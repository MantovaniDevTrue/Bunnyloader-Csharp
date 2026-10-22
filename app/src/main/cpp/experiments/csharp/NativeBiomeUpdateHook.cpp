#include "experiments/csharp/NativeBiomeUpdateHook.h"

#include "experiments/csharp/NativeBiomeUpdateSignal.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <thread>

namespace bl::csharp {
namespace {

using UpdateBiomesFn = void (*)(Il2CppObject*, const MethodInfo*);
UpdateBiomesFn g_original = nullptr;
FieldInfo* g_players = nullptr;
FieldInfo* g_localIndex = nullptr;
Il2CppClass* g_playerClass = nullptr;
NativeBiomeUpdateSignal g_signal;
std::thread::id g_gameThread{};

bool gameThread() {
    return std::this_thread::get_id() == g_gameThread;
}

void hkUpdateBiomes(Il2CppObject* self, const MethodInfo* method) {
    if (g_original) g_original(self, method);
    if (!g_signal.armed() || !gameThread() || !self ||
        !g_players || !g_localIndex || !g_playerClass)
        return;

    auto& api = il2cpp::api();
    if (!api.field_static_get_value) return;
    int32_t index = -1;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(g_localIndex, &index);
    api.field_static_get_value(g_players, &players);
    if (!players || index < 0 || index >= 255 ||
        static_cast<uintptr_t>(index) >= players->length ||
        players->length > 256 ||
        !api.class_get_rank || !api.class_get_element_class ||
        api.class_get_rank(players->obj.klass) != 1 ||
        api.class_get_element_class(players->obj.klass) != g_playerClass)
        return;

    auto* local = static_cast<Il2CppObject**>(arrayData(players))[index];
    if (self == local) g_signal.noteLocalPlayerUpdate();
}

bool fail(std::string* error, const char* reason) {
    if (error) *error = reason;
    return false;
}

} // namespace

bool installNativeBiomeUpdateHookOnGameThread(
    bool nativeBiomeRegistrationConfirmed, std::string* error) {
    if (error) error->clear();
    if (!nativeBiomeRegistrationConfirmed)
        return fail(error, "BiomeHook: native biome registrations not confirmed");

    auto& api = il2cpp::api();
    if (!api.method_is_instance || !api.field_static_get_value ||
        !api.field_get_type || !api.class_from_il2cpp_type ||
        !api.class_get_rank || !api.class_get_element_class)
        return fail(error, "BiomeHook: required IL2CPP exports unavailable");

    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!player || !main)
        return fail(error, "BiomeHook: Terraria classes unavailable");
    const auto* method = il2cpp::findMethodBySignature(
        player, il2cpp::parseSignature("void UpdateBiomes()"));
    if (!method || !api.method_is_instance(method))
        return fail(error, "BiomeHook: UpdateBiomes signature mismatch");

    FieldInfo* localIndex = nullptr;
    FieldInfo* playersField = nullptr;
    if (!il2cpp::nativeFieldContract(main, "myPlayer", "System.Int32",
            true, sizeof(int32_t), &localIndex, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "player", "Terraria.Player[]",
            true, sizeof(void*), &playersField, nullptr, error))
        return false;

    Il2CppArray* players = nullptr;
    api.field_static_get_value(playersField, &players);
    if (!players || players->length < 200 || players->length > 256 ||
        api.class_get_rank(players->obj.klass) != 1 ||
        api.class_get_element_class(players->obj.klass) != player)
        return fail(error, "BiomeHook: main player array contract mismatch");

    if (!g_original && !hook::install(method, hkUpdateBiomes, &g_original))
        return fail(error, "BiomeHook: native hook installation rejected");

    g_players = playersField;
    g_localIndex = localIndex;
    g_playerClass = player;
    g_gameThread = std::this_thread::get_id();
    g_signal.arm();
    return true;
}

uint32_t takeNativeBiomePlayerUpdatesOnGameThread() {
    if (!gameThread()) return 0;
    return g_signal.takePending();
}

void pauseNativeBiomeUpdateHookOnGameThread() {
    g_signal.pause();
}

bool nativeBiomeUpdateHookEnabled() {
    return g_signal.armed();
}

} // namespace bl::csharp
