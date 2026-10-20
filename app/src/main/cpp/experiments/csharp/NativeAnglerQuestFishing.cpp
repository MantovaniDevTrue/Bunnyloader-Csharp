#include "experiments/csharp/NativeAnglerQuestFishing.h"
#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include "hook/HookManager.h"
#include "core/Log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::angler {
namespace {

using RollFn = void (*)(Il2CppObject*, void*, const MethodInfo*);
RollFn g_originalRoll = nullptr;
const std::vector<Quest>* g_quests = nullptr;
bool g_installed = false;
const char* g_installDiagnostic = "NotAttempted";
FieldInfo* g_players = nullptr;
int g_owner = -1;
int g_gravity = -1;
size_t g_questFish = 0;
size_t g_uncommon = 0;
size_t g_rolledItem = 0;

bool isType(const Il2CppType* type, const char* wanted) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* name = a.type_get_name(type);
    const bool ok = name && std::strcmp(name, wanted) == 0;
    if (name) a.il2cpp_free(name);
    return ok;
}

bool instanceField(Il2CppClass* cls, const char* name,
                   const char* typeName, size_t width, int* offset) {
    auto& a = il2cpp::api();
    auto* field = cls ? il2cpp::findField(cls, name) : nullptr;
    if (!field || !a.class_instance_size || !a.field_get_type ||
        !a.field_get_offset || !a.field_get_flags ||
        (a.field_get_flags(field) & 0x10u) != 0 ||
        !isType(a.field_get_type(field), typeName))
        return false;
    const size_t at = a.field_get_offset(field), size = a.class_instance_size(cls);
    if (at < sizeof(Il2CppObject) || width > size ||
        at > size - width) return false;
    *offset = static_cast<int>(at);
    return true;
}

bool valueField(Il2CppClass* cls, const bl::il2cpp::ValueLayout& layout,
                const char* name, const char* typeName,
                size_t width, size_t* offset) {
    auto& a = il2cpp::api();
    auto* field = cls ? il2cpp::findField(cls, name) : nullptr;
    return field && a.field_get_type && a.field_get_offset &&
           a.field_get_flags && (a.field_get_flags(field) & 0x10u) == 0 &&
           isType(a.field_get_type(field), typeName) &&
           layout.offset(a.field_get_offset(field), width, offset);
}

void onFishingRoll(Il2CppObject* bobber, void* attempt, const MethodInfo* mi) {
    if (g_originalRoll) g_originalRoll(bobber, attempt, mi);
    if (!bobber || !attempt || !g_quests || g_quests->empty() ||
        activeGeneration().load(std::memory_order_acquire) != registry().generation())
        return;
    int32_t questId = -1;
    uint8_t uncommon = 0;
    std::memcpy(&questId, static_cast<const char*>(attempt) + g_questFish, sizeof(questId));
    std::memcpy(&uncommon, static_cast<const char*>(attempt) + g_uncommon, sizeof(uncommon));
    const auto& quests = *g_quests;
    const auto found = std::find_if(quests.begin(), quests.end(),
        [&](const Quest& quest) { return quest.type == questId; });
    if (found == quests.end() || (!found->catchEnabled && found->catchRules == 0 &&
        (found->callbackMask & 2) == 0)) return;
    int32_t owner = -1;
    std::memcpy(&owner, reinterpret_cast<const char*>(bobber) + g_owner, sizeof(owner));
    auto& a = il2cpp::api();
    Il2CppArray* players = nullptr;
    a.field_static_get_value(g_players, &players);
    if (!players || !a.class_get_rank || !a.class_get_element_class ||
        !a.class_get_name || !a.class_get_namespace ||
        a.class_get_rank(players->obj.klass) != 1 ||
        players->length > 256 || owner < 0 ||
        static_cast<uintptr_t>(owner) >= players->length)
        return;
    Il2CppClass* actual = a.class_get_element_class(players->obj.klass);
    const char* actualName = actual ? a.class_get_name(actual) : nullptr;
    const char* actualNamespace = actual ? a.class_get_namespace(actual) : nullptr;
    if (!actualName || !actualNamespace ||
        std::strcmp(actualName, "Player") != 0 ||
        std::strcmp(actualNamespace, "Terraria") != 0) return;
    auto* player = static_cast<Il2CppObject**>(arrayData(players))[owner];
    if (!player || a.object_get_class(player) != actual) return;
    float gravity = 1.f;
    std::memcpy(&gravity, reinterpret_cast<const char*>(player) + g_gravity, sizeof(gravity));
    if (!Registry::canCatch(*found, gravity < 0.f, uncommon != 0)) return;
    if ((found->callbackMask & 2) != 0) {
        bool allowed = false;
        if (!bl::csharp::invokeManagedAnglerQuestCallback(
            found->mod, found->name, 5, gravity < 0.f ? 1 : 0,
            uncommon != 0 ? 1 : 0, questId, &allowed, owner) || !allowed)
            return;
    }
    if (activeGeneration().load(std::memory_order_acquire) != registry().generation())
        return;
    int32_t currentOwner = -1;
    std::memcpy(&currentOwner, reinterpret_cast<const char*>(bobber) + g_owner,
                sizeof(currentOwner));
    Il2CppArray* currentPlayers = nullptr;
    a.field_static_get_value(g_players, &currentPlayers);
    if (currentOwner != owner || currentPlayers != players ||
        static_cast<Il2CppObject**>(arrayData(players))[owner] != player)
        return;
    const int32_t result = questId;
    std::memcpy(static_cast<char*>(attempt) + g_rolledItem, &result, sizeof(result));
}

}

bool installFishingOnGameThread(const std::vector<Quest>* quests) {
    g_quests = quests;
    if (g_installed) return true;
    g_installDiagnostic = "MissingIL2CPPExports";
    auto& a = il2cpp::api();
    if (!a.class_is_valuetype || !a.class_value_size ||
        !a.class_get_method_from_name || !a.method_get_param_count ||
        !a.method_get_param || !a.method_get_return_type ||
        !a.field_static_get_value || !a.object_get_class ||
        !a.class_get_element_class || !a.class_get_name ||
        !a.class_get_namespace || !a.class_get_rank || !a.field_get_flags ||
        !a.method_is_instance) return false;

    g_installDiagnostic = "MissingTerrariaTypes";
    Il2CppClass* projectile = il2cpp::findClass({"Terraria", "Projectile", {}});
    Il2CppClass* player = il2cpp::findClass({"Terraria", "Player", {}});
    Il2CppClass* fishing = il2cpp::findClass({"Terraria.DataStructures", "FishingAttempt", {}});
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!projectile || !player || !fishing || !main) return false;
    g_installDiagnostic = "UnexpectedFishingAttemptLayout";
    uint32_t alignment = 0;
    if (!a.class_is_valuetype(fishing) || a.class_value_size(fishing, &alignment) != 92)
        return false;
    g_installDiagnostic = "UnexpectedFishingMethodSignature";
    const MethodInfo* method = a.class_get_method_from_name(
        projectile, "FishingCheck_RollItemDrop", 1);
    if (!method || a.method_get_param_count(method) != 1 ||
        !isType(a.method_get_return_type(method), "System.Void") ||
        !isType(a.method_get_param(method, 0), "Terraria.DataStructures.FishingAttempt&") ||
        !a.method_is_instance(method)) return false;

    g_installDiagnostic = "UnexpectedFishingFieldLayout";
    bl::il2cpp::ValueLayout layout;
    std::string error;
    if (!bl::il2cpp::resolveValueLayout(fishing, &layout, &error) ||
        !valueField(fishing, layout, "questFish", "System.Int32", 4, &g_questFish) ||
        !valueField(fishing, layout, "uncommon", "System.Boolean", 1, &g_uncommon) ||
        !valueField(fishing, layout, "rolledItemDrop", "System.Int32", 4, &g_rolledItem) ||
        !instanceField(projectile, "owner", "System.Int32", 4, &g_owner) ||
        !instanceField(player, "gravDir", "System.Single", 4, &g_gravity))
        return false;
    g_installDiagnostic = "UnexpectedPlayerArrayContract";
    g_players = il2cpp::findField(main, "player");
    if (!g_players || (a.field_get_flags(g_players) & 0x10u) == 0 ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.field_static_get_value) return false;
    Il2CppClass* array = a.class_from_il2cpp_type(a.field_get_type(g_players));
    Il2CppClass* elements = array ? a.class_get_element_class(array) : nullptr;
    if (!elements) return false;
    const char* elementName = a.class_get_name(elements);
    const char* elementNs = a.class_get_namespace(elements);
    if (!elementName || !elementNs || std::strcmp(elementName, "Player") != 0 ||
        std::strcmp(elementNs, "Terraria") != 0) return false;
    g_installDiagnostic = "HookInstallFailed";
    g_installed = bl::hook::install(method, onFishingRoll, &g_originalRoll);
    if (g_installed) {
        g_installDiagnostic = "Installed";
        BL_INFO("[CSharp][AnglerQuest] Stage=FishingHook Result=Installed StructBytes=92");
    }
    return g_installed;
}

bool fishingInstalled() { return g_installed; }
const char* fishingInstallDiagnostic() { return g_installDiagnostic; }

}
