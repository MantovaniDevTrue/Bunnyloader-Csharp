#include "experiments/csharp/NativeTouchLayoutHooks.h"
#include "experiments/csharp/NativeTouchLayoutAccess.h"
#include "experiments/csharp/NativeTouchLayoutJson.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/GuardedCall.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::touch {
namespace {
using SaveFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using ShareFn = Il2CppString* (*)(Il2CppObject*, const MethodInfo*);
SaveFn originalSave = nullptr;
SaveFn originalLoad = nullptr;
ShareFn originalShare = nullptr;
std::atomic<bool> installed{false};
std::atomic<bool> restored{false};
uint64_t restoredVersion = 0;
bool attempted = false;

struct Changed {
    int32_t* pointer = nullptr;
    int32_t value = 0;
};
using Edits = std::vector<Changed>;

void strip(const std::vector<NativeLayoutBinding>& bindings, Edits* undo) {
    const int unbound = nativeTouchActionUnbound();
    if (unbound < 0) return;
    const auto clear = [&](int32_t* ptr) {
        if (ptr && nativeTouchIndexByAction(*ptr) >= 0) {
            undo->push_back({ptr, *ptr});
            *ptr = unbound;
        }
    };
    for (const auto& binding : bindings) {
        clear(binding.action);
        for (int32_t* action : binding.group) clear(action);
    }
}
void restore(Edits* undo) {
    for (auto it = undo->rbegin(); it != undo->rend(); ++it)
        *it->pointer = it->value;
    undo->clear();
}
std::string identity(int32_t raw, const std::string& previous, int unbound) {
    const int id = nativeTouchIndexByAction(raw);
    if (id >= 0) return registry().fullName(id);
    if (raw == unbound && !previous.empty() &&
        registry().find(previous) < 0) return previous;
    return {};
}

void capture(const std::vector<NativeLayoutBinding>& bindings,
             SavedProfiles* result) {
    const int unbound = nativeTouchActionUnbound();
    std::map<std::string, std::map<std::string, SavedSlot>> old;
    for (const auto& [profile, rows] : *result)
        for (const auto& row : rows) old[profile].emplace(row.slot, row);
    std::set<std::string> present;
    SavedProfiles captured;

    for (const auto& binding : bindings) {
        present.insert(binding.profile);
        const auto found = old[binding.profile].find(binding.slot);
        const SavedSlot* before = found != old[binding.profile].end()
            ? &found->second : nullptr;
        SavedSlot row;
        row.slot = binding.slot;
        row.index = binding.index;
        row.action = identity(*binding.action,
            before ? before->action : std::string{}, unbound);
        bool any = !row.action.empty();
        for (size_t i = 0; i < binding.group.size(); ++i) {
            const std::string prior = before && i < before->group.size()
                ? before->group[i] : std::string{};
            std::string value = identity(*binding.group[i], prior, unbound);
            if (!value.empty()) any = true;
            row.group.push_back(std::move(value));
        }
        if (any) captured[binding.profile].push_back(std::move(row));
    }
    // Preserva layouts de personagens não carregados e mods removidos.
    for (auto& [profile, rows] : *result)
        if (!present.contains(profile)) captured.emplace(profile, std::move(rows));
    *result = std::move(captured);
}
void restoreBindings(const std::vector<NativeLayoutBinding>& bindings,
           const SavedProfiles& saved) {
    const int unbound = nativeTouchActionUnbound();
    if (unbound < 0) return;
    for (const auto& binding : bindings) {
        auto profile = saved.find(binding.profile);
        if (profile == saved.end()) continue;
        const SavedSlot* row = nullptr;
        for (const auto& entry : profile->second)
            if (entry.slot == binding.slot) { row = &entry; break; }
        if (!row)
            for (const auto& entry : profile->second)
                if (entry.index == binding.index) { row = &entry; break; }
        if (!row) continue;
        if (*binding.action == unbound && !row->action.empty()) {
            int id = registry().find(row->action);
            if (id >= 0) *binding.action = nativeTouchActionOf(id);
        }
        for (size_t j = 0; j < binding.group.size() &&
                           j < row->group.size(); ++j) {
            if (*binding.group[j] != unbound || row->group[j].empty()) continue;
            int id = registry().find(row->group[j]);
            if (id >= 0) *binding.group[j] = nativeTouchActionOf(id);
        }
    }
}
bool collect(Il2CppObject* styles, std::vector<NativeLayoutBinding>* rows) {
    return styles && nativeTouchActionUnbound() >= 0 &&
           enumerateNativeTouchLayouts(styles, rows);
}
bool validateMethod(const MethodInfo* method, bool instance,
                    const char* parameter, bool isStringReturn) {
    auto& a = il2cpp::api();
    if (!method || !a.method_is_instance || !a.method_get_param_count ||
        !a.method_get_return_type || !a.method_get_param ||
        !a.type_get_name || !a.il2cpp_free ||
        a.method_is_instance(method) != instance ||
        a.method_get_param_count(method) != 1 ||
        !il2cpp::methodPointer(method)) return false;
    char* argument = a.type_get_name(a.method_get_param(method, 0));
    bool correct = argument && std::string(argument).find(parameter) != std::string::npos;
    if (argument) a.il2cpp_free(argument);
    if (!correct) return false;
    char* returned = a.type_get_name(a.method_get_return_type(method));
    const bool valid = returned && std::strcmp(
        returned, isStringReturn ? "System.String" : "System.Void") == 0;
    if (returned) a.il2cpp_free(returned);
    return valid;
}

void onSave(Il2CppObject* styles, Il2CppObject* prefs, const MethodInfo* method) {
    SaveFn original = __atomic_load_n(&originalSave, __ATOMIC_ACQUIRE);
    if (!original) return;
    if (!installed.load(std::memory_order_acquire)) {
        original(styles, prefs, method);
        return;
    }
    std::vector<NativeLayoutBinding> bindings;
    if (!collect(styles, &bindings)) {
        original(styles, prefs, method);
        return;
    }
    const std::string directory = nativeTouchSaveDirectory();
    SavedProfiles profiles;
    // Arquivo corrompido não deve ser substituído por um novo arquivo vazio.
    if (!directory.empty() && loadTouchLayoutFile(directory, &profiles)) {
        capture(bindings, &profiles);
        saveTouchLayoutFile(directory, profiles);
    }
    Edits undo;
    strip(bindings, &undo);
    auto call = [&] { original(styles, prefs, method); };
    il2cpp::guarded(call);
    restore(&undo);
}
void onLoad(Il2CppObject* styles, Il2CppObject* prefs, const MethodInfo* method) {
    SaveFn original = __atomic_load_n(&originalLoad, __ATOMIC_ACQUIRE);
    if (!original) return;
    original(styles, prefs, method);
    if (!installed.load(std::memory_order_acquire)) return;
    std::vector<NativeLayoutBinding> bindings;
    if (!collect(styles, &bindings)) return;
    const std::string directory = nativeTouchSaveDirectory();
    SavedProfiles profiles;
    if (!directory.empty() && loadTouchLayoutFile(directory, &profiles)) {
        restoreBindings(bindings, profiles);
        restoredVersion = registry().registrationVersion();
        restored.store(true, std::memory_order_release);
    }
}
Il2CppString* onShare(Il2CppObject* profile, const MethodInfo* method) {
    ShareFn original = __atomic_load_n(&originalShare, __ATOMIC_ACQUIRE);
    if (!original) return nullptr;
    if (!installed.load(std::memory_order_acquire))
        return original(profile, method);
    std::vector<NativeLayoutBinding> bindings;
    if (!enumerateNativeTouchShareProfile(profile, &bindings))
        return original(profile, method);
    Edits undo;
    strip(bindings, &undo);
    Il2CppString* code = nullptr;
    auto call = [&] { code = original(profile, method); };
    il2cpp::guarded(call);
    restore(&undo);
    return code;
}
}

bool nativeTouchLayoutHooksInstalled() {
    return installed.load(std::memory_order_acquire);
}
bool nativeTouchLayoutsRestored() {
    return restored.load(std::memory_order_acquire);
}

bool reapplyNativeTouchLayoutsOnGameThread() {
    if (!nativeTouchLayoutHooksInstalled()) return false;
    const uint64_t version = registry().registrationVersion();
    if (restored.load(std::memory_order_acquire) &&
        restoredVersion == version) return true;
    Il2CppObject* styles = currentNativeTouchStyles();
    std::vector<NativeLayoutBinding> bindings;
    if (!collect(styles, &bindings)) return false;
    SavedProfiles saved;
    const std::string directory = nativeTouchSaveDirectory();
    if (directory.empty() || !loadTouchLayoutFile(directory, &saved))
        return false;
    restoreBindings(bindings, saved);
    restoredVersion = version;
    restored.store(true, std::memory_order_release);
    return true;
}

void pumpNativeTouchLayoutHooksOnGameThread() {
    if (nativeTouchLayoutHooksInstalled() || attempted ||
        !nativeTouchObserverInstalled() || registry().count() == 0)
        return;
    attempted = true;
    Il2CppClass* styles = il2cpp::findClassQuiet("", "VirtualControlsStyles_Layout");
    const MethodInfo* save = styles ? il2cpp::findMethodBySignature(
        styles, il2cpp::parseSignature("void SaveConfiguration(Preferences preferences)"))
        : nullptr;
    const MethodInfo* load = styles ? il2cpp::findMethodBySignature(
        styles, il2cpp::parseSignature("void LoadConfiguration(Preferences preferences)"))
        : nullptr;
    const MethodInfo* share = styles ? il2cpp::findMethodBySignature(
        styles, il2cpp::parseSignature("string GetShareCode(VirtualControlsProfile_Layout profile)"))
        : nullptr;
    if (!validateMethod(save, true, "Preferences", false) ||
        !validateMethod(load, true, "Preferences", false) ||
        !validateMethod(share, false, "VirtualControlsProfile_Layout", true))
        return;
    if (!bl::hook::install(save, onSave, &originalSave) || !originalSave)
        return;
    if (!bl::hook::install(load, onLoad, &originalLoad) || !originalLoad)
        return;
    if (!bl::hook::install(share, onShare, &originalShare) || !originalShare)
        return;
    installed.store(true, std::memory_order_release);
    // Restore também é invocado quando o conteúdo C# fica pronto após
    // LoadConfiguration; o host_ready só pode ligar após todos os hooks.
}
} // namespace bl::csharp::touch
