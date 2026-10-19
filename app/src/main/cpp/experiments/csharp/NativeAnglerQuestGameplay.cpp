#include "experiments/csharp/NativeAnglerQuestGameplay.h"
#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include "experiments/csharp/NativeAnglerQuestFishing.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::angler {
namespace {

using SwapFn = void (*)(const MethodInfo*);
using ChatFn = Il2CppString* (*)(uint8_t, const MethodInfo*);

SwapFn g_originalSwap = nullptr;
ChatFn g_originalChat = nullptr;
bool g_swapHooked = false;
bool g_chatHooked = false;
FieldInfo* g_questItems = nullptr;
FieldInfo* g_questIndex = nullptr;
FieldInfo* g_questFinished = nullptr;
FieldInfo* g_hardMode = nullptr;
FieldInfo* g_chatCorner = nullptr;
FieldInfo* g_questFlag = nullptr;
FieldInfo* g_weaponRackFlag = nullptr;
const MethodInfo* g_nameMethod = nullptr;
const MethodInfo* g_activeCultureMethod = nullptr;
const MethodInfo* g_cultureNameMethod = nullptr;
uint64_t g_appliedGeneration = std::numeric_limits<uint64_t>::max();
unsigned g_poll = 0;
unsigned g_resolveFailures = 0;
std::vector<Quest> g_active;
std::set<int> g_ownedTypes;
std::map<int, bool> g_previousFlags;
std::map<int, bool> g_previousRackFlags;
bool g_fishingWarning = false;

std::string utf8(Il2CppString* input) {
    if (!input || input->length < 0 || input->length > 256) return {};
    std::string out;
    out.reserve(static_cast<size_t>(input->length));
    for (int i = 0; i < input->length; ++i) {
        char32_t c = input->chars[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < input->length) {
            const char32_t low = input->chars[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + low - 0xDC00;
                ++i;
            }
        }
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::string currentCulture() {
    auto& a = il2cpp::api();
    if (!g_activeCultureMethod || !g_cultureNameMethod || !a.runtime_invoke)
        return "en-US";
    Il2CppObject* exception = nullptr;
    Il2CppObject* culture = a.runtime_invoke(g_activeCultureMethod, nullptr, nullptr, &exception);
    if (exception || !culture) return "en-US";
    exception = nullptr;
    auto* result = reinterpret_cast<Il2CppString*>(
        a.runtime_invoke(g_cultureNameMethod, culture, nullptr, &exception));
    if (exception || !result) return "en-US";
    std::string text = utf8(result);
    return validCulture(text) ? text : "en-US";
}

bool typeName(const Il2CppType* type, const char* name) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* actual = a.type_get_name(type);
    bool ok = actual && std::strcmp(actual, name) == 0;
    if (actual) a.il2cpp_free(actual);
    return ok;
}

bool methodSignature(const MethodInfo* method, const char* result,
                     const char* argument = nullptr) {
    auto& a = il2cpp::api();
    const uint32_t count = argument ? 1u : 0u;
    if (!method || !a.method_get_param_count || !a.method_get_return_type ||
        !a.method_get_param || a.method_get_param_count(method) != count ||
        !typeName(a.method_get_return_type(method), result) ||
        (argument && !typeName(a.method_get_param(method, 0), argument)))
        return false;
    return !a.method_is_instance || !a.method_is_instance(method);
}

bool staticField(FieldInfo* field) {
    auto& a = il2cpp::api();
    return field && a.field_get_flags && (a.field_get_flags(field) & 0x10u) != 0;
}

bool arrayType(FieldInfo* field, const char* element) {
    auto& a = il2cpp::api();
    if (!staticField(field) || !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_element_class || !a.class_get_name || !a.class_get_namespace)
        return false;
    Il2CppClass* array = a.class_from_il2cpp_type(a.field_get_type(field));
    Il2CppClass* cls = array ? a.class_get_element_class(array) : nullptr;
    if (!cls) return false;
    const char* name = a.class_get_name(cls);
    const char* ns = a.class_get_namespace(cls);
    return name && ns && std::strcmp(name, element) == 0 &&
           std::strcmp(ns, "System") == 0;
}

bool findContracts() {
    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.field_static_set_value ||
        !a.array_new || !a.object_get_class || !a.string_new ||
        !a.class_get_method_from_name || !a.method_get_param_count ||
        !a.runtime_invoke || !a.method_is_instance || !a.field_get_flags)
        return false;
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* lang = il2cpp::findClass({"Terraria", "Lang", {}});
    Il2CppClass* sets = il2cpp::findClass({"Terraria.ID", "ItemID", "Sets"});
    if (!main || !lang || !sets) return false;
    if (a.runtime_class_init) {
        a.runtime_class_init(main);
        a.runtime_class_init(sets);
        a.runtime_class_init(lang);
    }
    const MethodInfo* swap = a.class_get_method_from_name(main, "AnglerQuestSwap", 0);
    const MethodInfo* chat = a.class_get_method_from_name(lang, "AnglerQuestChat", 1);
    if (!methodSignature(swap, "System.Void") ||
        !methodSignature(chat, "System.String", "System.Boolean"))
        return false;
    g_questItems = il2cpp::findField(main, "anglerQuestItemNetIDs");
    g_questIndex = il2cpp::findField(main, "anglerQuest");
    g_questFinished = il2cpp::findField(main, "anglerQuestFinished");
    g_hardMode = il2cpp::findField(main, "hardMode");
    g_chatCorner = il2cpp::findField(main, "npcChatCornerItem");
    g_questFlag = il2cpp::findField(sets, "IsQuestFish");
    g_weaponRackFlag = il2cpp::findField(sets, "CanBePlacedOnWeaponRacks");
    if (!arrayType(g_weaponRackFlag, "Boolean"))
        g_weaponRackFlag = nullptr;
    if (!arrayType(g_questItems, "Int32") || !arrayType(g_questFlag, "Boolean") ||
        !staticField(g_questIndex) || !staticField(g_questFinished) ||
        !staticField(g_hardMode) || !staticField(g_chatCorner) ||
        !typeName(a.field_get_type(g_questIndex), "System.Int32") ||
        !typeName(a.field_get_type(g_chatCorner), "System.Int32") ||
        !typeName(a.field_get_type(g_questFinished), "System.Boolean") ||
        !typeName(a.field_get_type(g_hardMode), "System.Boolean"))
        return false;
    Il2CppClass* language = il2cpp::findClassQuiet("Terraria.Localization", "Language");
    Il2CppClass* culture = il2cpp::findClassQuiet("Terraria.Localization", "GameCulture");
    g_activeCultureMethod = language ?
        a.class_get_method_from_name(language, "get_ActiveCulture", 0) : nullptr;
    g_cultureNameMethod = culture ?
        a.class_get_method_from_name(culture, "get_Name", 0) : nullptr;
    if (!g_activeCultureMethod || !g_cultureNameMethod ||
        !a.method_get_param_count ||
        a.method_get_param_count(g_activeCultureMethod) != 0 ||
        a.method_get_param_count(g_cultureNameMethod) != 0 ||
        !typeName(a.method_get_return_type(g_cultureNameMethod), "System.String")) {
        g_activeCultureMethod = nullptr;
        g_cultureNameMethod = nullptr;
    }
    const MethodInfo* name = a.class_get_method_from_name(lang, "GetItemNameValue", 1);
    if (methodSignature(name, "System.String", "System.Int32"))
        g_nameMethod = name;
    if (!g_swapHooked) g_swapHooked = hook::install(swap, +[](const MethodInfo* mi) {
        if (!g_originalSwap) return;
        g_originalSwap(mi);
        if (g_active.empty()) return;
        auto& api = il2cpp::api();
        uint8_t finished = 0, hardmode = 0;
        if (!g_questFinished || !g_hardMode) return;
        api.field_static_get_value(g_questFinished, &finished);
        api.field_static_get_value(g_hardMode, &hardmode);
        if (finished) return;
        Registry::reroll([&] { g_originalSwap(mi); }, [&]() -> int {
            Il2CppArray* arr = nullptr;
            int32_t selected = -1;
            api.field_static_get_value(g_questItems, &arr);
            api.field_static_get_value(g_questIndex, &selected);
            if (!arr || selected < 0 || static_cast<uintptr_t>(selected) >= arr->length)
                return -1;
            return static_cast<const int32_t*>(arrayData(arr))[selected];
        }, [&]() -> bool {
            uint8_t done = 0;
            api.field_static_get_value(g_questFinished, &done);
            return done != 0;
        }, g_active, hardmode != 0);
    }, &g_originalSwap);
    if (!g_chatHooked) g_chatHooked = hook::install(chat, +[](uint8_t turnIn, const MethodInfo* mi) -> Il2CppString* {
        if (!g_originalChat) return nullptr;
        if (turnIn || g_active.empty()) return g_originalChat(turnIn, mi);
        auto& api = il2cpp::api();
        uint8_t finished = 0;
        int32_t index = -1;
        Il2CppArray* array = nullptr;
        api.field_static_get_value(g_questFinished, &finished);
        api.field_static_get_value(g_questIndex, &index);
        api.field_static_get_value(g_questItems, &array);
        if (finished || !array || index < 0 ||
            static_cast<uintptr_t>(index) >= array->length)
            return g_originalChat(turnIn, mi);
        const int type = static_cast<const int32_t*>(arrayData(array))[index];
        const auto it = std::find_if(g_active.begin(), g_active.end(),
            [type](const Quest& q) { return q.type == type; });
        if (it == g_active.end()) return g_originalChat(turnIn, mi);
        std::string name = it->name;
        if (it->description.empty() && g_nameMethod) {
            int itemType = type;
            void* args[1] = {&itemType};
            Il2CppObject* exception = nullptr;
            auto* result = reinterpret_cast<Il2CppString*>(api.runtime_invoke(
                g_nameMethod, nullptr, args, &exception));
            if (!exception && result && result->length > 0) {
                std::string localized = utf8(result);
                if (!localized.empty()) name = std::move(localized);
            }
        }
        Il2CppString* output = api.string_new(Registry::chat(*it, name, currentCulture()).c_str());
        if (!output) return g_originalChat(turnIn, mi);
        int32_t corner = type;
        api.field_static_set_value(g_chatCorner, &corner);
        return output;
    }, &g_originalChat);
    return g_swapHooked && g_chatHooked;
}

bool writeFlagTable(FieldInfo* field, std::map<int, bool>* saved,
                    const std::vector<Quest>& quests) {
    if (!field || !saved) return false;
    auto& a = il2cpp::api();
    Il2CppArray* flags = nullptr;
    a.field_static_get_value(field, &flags);
    if (!flags || flags->length == 0 || flags->length > 65536) return false;
    int largest = -1;
    for (const auto& quest : quests) largest = std::max(largest, quest.type);
    if (largest >= 65536) return false;
    if (largest >= 0 && static_cast<uintptr_t>(largest) >= flags->length) {
        Il2CppArray* expanded = runtime::TypeTables::resizedCopy(flags, largest + 1);
        if (!expanded) return false;
        a.field_static_set_value(field, &expanded);
        flags = expanded;
    }
    auto* values = static_cast<uint8_t*>(arrayData(flags));
    std::set<int> upcoming;
    for (const auto& quest : quests) upcoming.insert(quest.type);
    for (int former : g_ownedTypes)
        if (!upcoming.contains(former) &&
            former >= 0 && static_cast<uintptr_t>(former) < flags->length) {
            const auto before = saved->find(former);
            if (before != saved->end()) values[former] = before->second ? 1 : 0;
            saved->erase(former);
        }
    for (const auto& quest : quests) {
        if (!saved->contains(quest.type))
            (*saved)[quest.type] = values[quest.type] != 0;
        values[quest.type] = 1;
    }
    return true;
}

bool writeQuestIds(const std::vector<Quest>& quests) {
    auto& a = il2cpp::api();
    Il2CppArray* original = nullptr;
    a.field_static_get_value(g_questItems, &original);
    if (!original || original->length == 0 || original->length > 65536)
        return false;
    const auto* data = static_cast<const int32_t*>(arrayData(original));
    int32_t selected = -1;
    a.field_static_get_value(g_questIndex, &selected);
    const std::vector<int> originalIds(data, data + original->length);
    const QuestListUpdate update = Registry::reconcileIds(
        originalIds, g_ownedTypes, quests, selected);
    if (update.ids.empty() || update.ids.size() > 65536) return false;
    if (!update.changed) return true;
    Il2CppClass* element = a.class_get_element_class(
        a.object_get_class(reinterpret_cast<Il2CppObject*>(original)));
    if (!element) return false;
    Il2CppArray* updated = a.array_new(element, update.ids.size());
    if (!updated) return false;
    std::copy(update.ids.begin(), update.ids.end(),
              static_cast<int32_t*>(arrayData(updated)));
    int32_t next = update.selected;
    a.field_static_set_value(g_questItems, &updated);
    a.field_static_set_value(g_questIndex, &next);
    return true;
}

bool updateQuests(const std::vector<Quest>& quests) {
    if (!writeQuestIds(quests)) return false;
    if (!writeFlagTable(g_questFlag, &g_previousFlags, quests)) return false;
    if (g_weaponRackFlag &&
        !writeFlagTable(g_weaponRackFlag, &g_previousRackFlags, quests))
        return false;
    std::set<int> upcoming;
    for (const auto& quest : quests) upcoming.insert(quest.type);
    g_ownedTypes = std::move(upcoming);
    return true;
}

}

void pumpOnGameThread() {
    const uint64_t revision = registry().generation();
    if (revision == 0 && g_ownedTypes.empty()) return;
    if (revision == g_appliedGeneration && g_active.empty() && g_ownedTypes.empty()) return;
    bool pending = revision != g_appliedGeneration;
    if (!pending && ++g_poll % 120u != 0) return;
    if (!g_swapHooked || !g_chatHooked || !g_questItems) {
        if (!findContracts()) {
            if (++g_resolveFailures == 1 || g_resolveFailures == 120)
                BL_WARN("[CSharp][AnglerQuest] Stage=ResolveNativeHooks Result=Pending Reason=MissingOrIncompatibleIL2CPPContract");
            return;
        }
    }
    const std::vector<Quest> quests = registry().snapshot();
    if (!updateQuests(quests)) {
        if (++g_resolveFailures == 1 || g_resolveFailures == 120)
            BL_WARN("[CSharp][AnglerQuest] Stage=PublishArrays Result=Pending Reason=InvalidOrUnreadyTerrariaTables");
        return;
    }
    g_resolveFailures = 0;
    g_appliedGeneration = revision;
    g_active = quests;
    if (!quests.empty() && !installFishingOnGameThread(&g_active)) {
        if (!g_fishingWarning)
            BL_WARN("[CSharp][AnglerQuest] Stage=FishingHook Result=Pending Reason=UnverifiedFishingAttemptABI");
        g_fishingWarning = true;
    } else g_fishingWarning = false;
    if (pending)
        BL_INFO("[CSharp][AnglerQuest] Stage=Ready Quests=%zu Hooks=2 Result=Installed",
                quests.size());
}

void invalidateWorldSession() {
    g_appliedGeneration = std::numeric_limits<uint64_t>::max();
    g_poll = 0;
}

bool hooksInstalled() { return g_swapHooked && g_chatHooked; }

}
