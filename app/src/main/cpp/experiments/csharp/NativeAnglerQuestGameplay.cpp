#include "experiments/csharp/NativeAnglerQuestGameplay.h"
#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include "experiments/csharp/NativeAnglerQuestFishing.h"
#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/FieldContract.h"
#include "experiments/csharp/NativeStaticValue.h"

#include <algorithm>
#include <array>
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
native_static::Value g_questFinished;
FieldInfo* g_hardMode = nullptr;
FieldInfo* g_netMode = nullptr;
native_static::Value g_chatCorner;
FieldInfo* g_questFlag = nullptr;
FieldInfo* g_weaponRackFlag = nullptr;
const MethodInfo* g_nameMethod = nullptr;
const MethodInfo* g_activeCultureMethod = nullptr;
const MethodInfo* g_cultureNameMethod = nullptr;
uint64_t g_appliedGeneration = std::numeric_limits<uint64_t>::max();
unsigned g_poll = 0;
unsigned g_resolveFailures = 0;
const char* g_contractDiagnostic = "NotChecked";
std::vector<Quest> g_active;
uint64_t g_effectiveListFingerprint = 0;
std::set<int> g_ownedTypes;
std::map<int, bool> g_previousFlags;
std::map<int, bool> g_previousRackFlags;
bool g_fishingWarning = false;
bool g_sessionBoundaryPending = false;
Il2CppArray* g_lastQuestItems = nullptr;
Il2CppArray* g_lastFishFlags = nullptr;
Il2CppArray* g_lastRackFlags = nullptr;

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

bool staticPrimitive(const native_static::Value& value,const char* kind,size_t width) {
    auto& a=il2cpp::api();
    if(!value||!a.class_get_namespace||!a.class_get_name||!a.class_is_valuetype||!a.class_value_size)return false;
    const char* ns=a.class_get_namespace(value.type),*name=a.class_get_name(value.type);
    return ns&&name&&!std::strcmp(ns,"System")&&!std::strcmp(name,kind)&&
        a.class_is_valuetype(value.type)&&native_static::size(value)==width;
}

bool findContracts() {
    g_contractDiagnostic = "MissingIL2CPPExports";
    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.field_static_set_value ||
        !a.array_new || !a.object_get_class || !a.string_new ||
        !a.class_get_method_from_name || !a.method_get_param_count ||
        !a.runtime_invoke || !a.method_is_instance || !a.field_get_flags)
        return false;
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* lang = il2cpp::findClass({"Terraria", "Lang", {}});
    Il2CppClass* sets = il2cpp::findClass({"Terraria.ID", "ItemID", "Sets"});
    if (!main || !lang || !sets) {
        g_contractDiagnostic = !main ? "TerrariaMainMissing" :
            !lang ? "TerrariaLangMissing" : "ItemIDSetsMissing";
        return false;
    }
    if (a.runtime_class_init) {
        a.runtime_class_init(main);
        a.runtime_class_init(sets);
        a.runtime_class_init(lang);
    }
    const MethodInfo* swap = a.class_get_method_from_name(main, "AnglerQuestSwap", 0);
    const MethodInfo* chat = a.class_get_method_from_name(lang, "AnglerQuestChat", 1);
    if (!methodSignature(swap, "System.Void") ||
        !methodSignature(chat, "System.String", "System.Boolean")) {
        g_contractDiagnostic = !methodSignature(swap, "System.Void") ?
            "AnglerQuestSwapSignatureMismatch" : "AnglerQuestChatSignatureMismatch";
        return false;
    }
    g_questItems = il2cpp::findField(main, "anglerQuestItemNetIDs");
    g_questIndex = il2cpp::findField(main, "anglerQuest");
    g_questFinished = native_static::resolve(main, "anglerQuestFinished");
    g_hardMode = il2cpp::findField(main, "hardMode");
    g_netMode = il2cpp::findField(main, "netMode");
    if (!staticField(g_netMode) ||
        !typeName(a.field_get_type(g_netMode), "System.Int32")) {
        g_contractDiagnostic = "NetModeStaticInt32Mismatch";
        return false;
    }
    g_chatCorner = native_static::resolve(main, "npcChatCornerItem");
    g_questFlag = il2cpp::findField(sets, "IsQuestFish");
    g_weaponRackFlag = il2cpp::findField(sets, "CanBePlacedOnWeaponRacks");
    if (!arrayType(g_weaponRackFlag, "Boolean"))
        g_weaponRackFlag = nullptr;
    if (!arrayType(g_questItems, "Int32") ||
        !arrayType(g_questFlag, "Boolean") ||
        !staticField(g_questIndex) || !g_questFinished ||
        !staticField(g_hardMode) || (!g_chatCorner || !g_chatCorner.writable()) ||
        !typeName(a.field_get_type(g_questIndex), "System.Int32") ||
        !staticPrimitive(g_chatCorner, "Int32", 4) ||
        !staticPrimitive(g_questFinished, "Boolean", 1) ||
        !typeName(a.field_get_type(g_hardMode), "System.Boolean")) {
        g_contractDiagnostic = !arrayType(g_questItems, "Int32") ? "QuestItemNetIDsInt32ArrayMismatch" :
            !arrayType(g_questFlag, "Boolean") ? "ItemIsQuestFishBooleanArrayMismatch" :
            !staticField(g_questIndex) ? "QuestIndexNotStatic" :
            !g_questFinished ? "QuestFinishedNotStatic" :
            !staticField(g_hardMode) ? "HardModeNotStatic" :
            (!g_chatCorner || !g_chatCorner.writable()) ? "NpcChatCornerNotStatic" :
            !typeName(a.field_get_type(g_questIndex), "System.Int32") ? "QuestIndexNotInt32" :
            !staticPrimitive(g_chatCorner, "Int32", 4) ? "NpcChatCornerNotInt32" :
            !staticPrimitive(g_questFinished, "Boolean", 1) ? "QuestFinishedNotBoolean" :
            "HardModeNotBoolean";
        return false;
    }
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
        if (g_active.empty() ||
            activeGeneration().load(std::memory_order_acquire) != registry().generation())
            return;
        auto& api = il2cpp::api();
        int32_t mode = 0;
        api.field_static_get_value(g_netMode, &mode);
        if (mode < 0 || mode > 3 || mode == 1) return;
        uint8_t finished = 0, hardmode = 0;
        if (!g_questFinished || !g_hardMode) return;
        if (!native_static::read(g_questFinished, &finished, sizeof(finished))) return;
        api.field_static_get_value(g_hardMode, &hardmode);
        if (finished) return;
        std::map<int,bool> decisions;
        int callbackBudget = 4;
        Registry::reroll([&] { g_originalSwap(mi); }, [&]() -> int {
            Il2CppArray* arr = nullptr;
            int32_t selected = -1;
            api.field_static_get_value(g_questItems, &arr);
            api.field_static_get_value(g_questIndex, &selected);
            if (!il2cpp::nativeArrayContract(arr, "Int32", sizeof(int32_t), 65536) ||
                selected < 0 || static_cast<uintptr_t>(selected) >= arr->length)
                return -1;
            return static_cast<const int32_t*>(arrayData(arr))[selected];
        }, [&]() -> bool {
            uint8_t done = 0;
            // Stop rerolling if a property getter fails; keep the engine result.
            return !native_static::read(g_questFinished, &done, sizeof(done)) || done != 0;
        }, g_active, hardmode != 0, [&](const Quest& quest,bool mode) {
            if (!Registry::available(quest,mode)) return false;
            if ((quest.callbackMask&1)==0) return true;
            const auto cached=decisions.find(quest.type);
            if (cached!=decisions.end()) return cached->second;
            if (callbackBudget==0) {
                decisions[quest.type]=false;
                return false;
            }
            --callbackBudget;
            bool allowed=false;
            const bool ok=bl::csharp::invokeManagedAnglerQuestCallback(
                quest.mod,quest.name,4,mode?1:0,0,quest.type,&allowed);
            if (activeGeneration().load(std::memory_order_acquire) != registry().generation())
                return true;
            decisions[quest.type]=ok&&allowed;
            return ok&&allowed;
        });
    }, &g_originalSwap);
    if (!g_chatHooked) g_chatHooked = hook::install(chat, +[](uint8_t turnIn, const MethodInfo* mi) -> Il2CppString* {
        if (!g_originalChat) return nullptr;
        if (turnIn || g_active.empty() ||
            activeGeneration().load(std::memory_order_acquire) != registry().generation())
            return g_originalChat(turnIn, mi);
        auto& api = il2cpp::api();
        uint8_t finished = 0;
        int32_t index = -1;
        Il2CppArray* array = nullptr;
        if (!native_static::read(g_questFinished, &finished, sizeof(finished)))
            return g_originalChat(turnIn, mi);
        api.field_static_get_value(g_questIndex, &index);
        api.field_static_get_value(g_questItems, &array);
        if (finished || !il2cpp::nativeArrayContract(array, "Int32", sizeof(int32_t), 65536) ||
            index < 0 || static_cast<uintptr_t>(index) >= array->length)
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
        if (!output || activeGeneration().load(std::memory_order_acquire) != registry().generation())
            return g_originalChat(turnIn, mi);
        int32_t corner = type;
        if (!native_static::write(g_chatCorner, &corner, sizeof(corner)))
            return g_originalChat(turnIn, mi);
        return output;
    }, &g_originalChat);
    const bool both = g_swapHooked && g_chatHooked;
    g_contractDiagnostic = both ? "Ready" : (!g_swapHooked ? "QuestSwapHookRejected" :
        "QuestChatHookRejected");
    return both;
}

struct PendingArrayRoots {
    std::array<uint32_t, 3> handles{};
    size_t count = 0;

    bool retain(Il2CppArray* array) {
        auto& a = il2cpp::api();
        if (!array || !a.gchandle_new || !a.gchandle_free ||
            count >= handles.size()) return false;
        const uint32_t handle = a.gchandle_new(reinterpret_cast<Il2CppObject*>(array), true);
        if (!handle) return false;
        handles[count++] = handle;
        return true;
    }

    ~PendingArrayRoots() {
        auto& a = il2cpp::api();
        if (!a.gchandle_free) return;
        for (size_t i = 0; i < count; ++i)
            a.gchandle_free(handles[i]);
    }
};

struct FlagPlan {
    FieldInfo* field = nullptr;
    Il2CppArray* array = nullptr;
    bool replace = false;
    std::map<int, bool> previous;
    std::map<int, uint8_t> writes;
};

bool prepareFlags(FieldInfo* field, const std::map<int, bool>& saved,
                  const std::set<int>& upcoming, Il2CppArray* lastPublished,
                  PendingArrayRoots* roots, FlagPlan* plan) {
    if (!field || !plan) return false;
    auto& a = il2cpp::api();
    Il2CppArray* original = nullptr;
    a.field_static_get_value(field, &original);
    if (!il2cpp::nativeArrayContract(original, "Boolean", 1, 65536) ||
        original->length == 0) return false;
    int maximum = -1;
    if (!upcoming.empty()) maximum = *upcoming.rbegin();
    if (maximum >= 65536) return false;
    Il2CppArray* next = original;
    if (maximum >= 0 && static_cast<uintptr_t>(maximum) >= original->length) {
        next = runtime::TypeTables::resizedCopy(original, maximum + 1);
        if (!il2cpp::nativeArrayContract(next, "Boolean", 1, 65536) ||
            next->length <= static_cast<uintptr_t>(maximum) ||
            !roots || !roots->retain(next)) return false;
    }
    plan->field = field;
    plan->array = next;
    plan->replace = next != original;
    plan->previous = saved;
    if (g_sessionBoundaryPending && lastPublished && original != lastPublished)
        plan->previous.clear();
    const auto* values = static_cast<const uint8_t*>(arrayData(next));
    for (int former : g_ownedTypes) {
        if (upcoming.contains(former) || former < 0 ||
            static_cast<uintptr_t>(former) >= next->length) continue;
        const auto before = plan->previous.find(former);
        if (before != plan->previous.end()) {
            if (!before->second && values[former])
                plan->writes[former] = 0;
            plan->previous.erase(before);
        }
    }
    for (int type : upcoming) {
        if (!plan->previous.contains(type))
            plan->previous[type] = values[type] != 0;
        if (!values[type]) plan->writes[type] = 1;
    }
    return true;
}

void applyFlags(const FlagPlan& plan) {
    auto& a = il2cpp::api();
    auto* values = static_cast<uint8_t*>(arrayData(plan.array));
    for (const auto& [type, value] : plan.writes) values[type] = value;
    if (plan.replace) {
        Il2CppArray* next = plan.array;
        a.field_static_set_value(plan.field, &next);
    }
}

bool updateQuests(const std::vector<Quest>& quests) {
    auto& a = il2cpp::api();
    Il2CppArray* original = nullptr;
    a.field_static_get_value(g_questItems, &original);
    if (!il2cpp::nativeArrayContract(original, "Int32", sizeof(int32_t), 65536) ||
        original->length == 0) return false;
    const auto* data = static_cast<const int32_t*>(arrayData(original));
    int32_t selected = -1;
    a.field_static_get_value(g_questIndex, &selected);
    const std::vector<int> originalIds(data, data + original->length);
    const bool freshList = g_sessionBoundaryPending && g_lastQuestItems &&
        original != g_lastQuestItems;
    const std::set<int> previouslyOwned = freshList ? std::set<int>{} : g_ownedTypes;
    const QuestListUpdate update = Registry::reconcileIds(
        originalIds, previouslyOwned, quests, selected);
    if (update.ids.empty() || update.ids.size() > 65536) return false;
    PendingArrayRoots roots;
    Il2CppArray* nextIds = nullptr;
    if (update.changed) {
        Il2CppClass* element = a.class_get_element_class(
            a.object_get_class(reinterpret_cast<Il2CppObject*>(original)));
        if (!element) return false;
        nextIds = a.array_new(element, update.ids.size());
        if (!il2cpp::nativeArrayContract(nextIds, "Int32", sizeof(int32_t), 65536) ||
            nextIds->length != update.ids.size() || !roots.retain(nextIds))
            return false;
        std::copy(update.ids.begin(), update.ids.end(),
                  static_cast<int32_t*>(arrayData(nextIds)));
    }
    std::set<int> upcoming;
    for (const auto& quest : quests) upcoming.insert(quest.type);
    FlagPlan fish, rack;
    if (!prepareFlags(g_questFlag, g_previousFlags, upcoming,
                      g_lastFishFlags, &roots, &fish) ||
        (g_weaponRackFlag &&
         !prepareFlags(g_weaponRackFlag, g_previousRackFlags, upcoming,
                       g_lastRackFlags, &roots, &rack)))
        return false;
    applyFlags(fish);
    if (g_weaponRackFlag) applyFlags(rack);
    if (update.changed) a.field_static_set_value(g_questItems, &nextIds);
    if (selected != update.selected) {
        int32_t next = update.selected;
        a.field_static_set_value(g_questIndex, &next);
    }
    g_effectiveListFingerprint = Registry::listFingerprint(update.ids, quests);
    g_lastQuestItems = update.changed ? nextIds : original;
    g_previousFlags = std::move(fish.previous);
    g_lastFishFlags = fish.array;
    if (g_weaponRackFlag) {
        g_previousRackFlags = std::move(rack.previous);
        g_lastRackFlags = rack.array;
    } else {
        g_previousRackFlags.clear();
        g_lastRackFlags = nullptr;
    }
    g_sessionBoundaryPending = false;
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
                BL_WARN("[CSharp][AnglerQuest] Stage=ResolveNativeHooks Result=Pending Reason=%s",
                        g_contractDiagnostic);
            return;
        }
    }
    const auto [appliedRevision, quests] = registry().snapshotVersion();
    pending = pending || appliedRevision != g_appliedGeneration;
    const uint64_t previousListFingerprint = g_effectiveListFingerprint;
    if (!updateQuests(quests)) {
        if (++g_resolveFailures == 1 || g_resolveFailures == 120)
            BL_WARN("[CSharp][AnglerQuest] Stage=PublishArrays Result=Pending Reason=InvalidOrUnreadyTerrariaTables");
        return;
    }
    g_resolveFailures = 0;
    pending = pending || previousListFingerprint != g_effectiveListFingerprint;
    g_appliedGeneration = appliedRevision;
    g_active = quests;
    activeGeneration().store(appliedRevision, std::memory_order_release);
    if (!quests.empty() && !installFishingOnGameThread(&g_active)) {
        if (!g_fishingWarning)
            BL_WARN("[CSharp][AnglerQuest] Stage=FishingHook Result=Pending Reason=%s",
                    fishingInstallDiagnostic());
        g_fishingWarning = true;
    } else g_fishingWarning = false;
    if (pending)
        BL_INFO("[CSharp][AnglerQuest] Stage=Ready Quests=%zu Manifest=%016llx EffectiveList=%016llx Hooks=2 Result=Installed",
                quests.size(),
                static_cast<unsigned long long>(Registry::manifestFingerprint(quests)),
                static_cast<unsigned long long>(g_effectiveListFingerprint));
}

void invalidateWorldSession() {
    g_sessionBoundaryPending = true;
    activeGeneration().store(std::numeric_limits<uint64_t>::max(), std::memory_order_release);
    g_appliedGeneration = std::numeric_limits<uint64_t>::max();
    g_poll = 0;
}

bool hooksInstalled() { return g_swapHooked && g_chatHooked; }

}
