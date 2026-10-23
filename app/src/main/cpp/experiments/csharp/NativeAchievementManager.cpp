#include "experiments/csharp/NativeAchievementManager.h"

#include "experiments/csharp/NativeAchievementContracts.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>

namespace bl::csharp::achievement {
namespace {

struct Methods {
    Il2CppObject* manager = nullptr;
    Il2CppClass* achievement = nullptr;
    Il2CppClass* localizedText = nullptr;
    const MethodInfo* ctor = nullptr;
    const MethodInfo* localizedCtor = nullptr;
    const MethodInfo* addCondition = nullptr;
    const MethodInfo* registerAchievement = nullptr;
    const MethodInfo* unregisterAchievement = nullptr;
    const MethodInfo* getAchievement = nullptr;
    const MethodInfo* registerIcon = nullptr;
    const MethodInfo* registerCategory = nullptr;
    const MethodInfo* useCountTracker = nullptr;
    const MethodInfo* useValueTracker = nullptr;
};

bool typeMatches(const Il2CppType* type, const char* expected) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* name = a.type_get_name(type);
    const bool same = name && std::strcmp(name, expected) == 0;
    if (name) a.il2cpp_free(name);
    return same;
}

const MethodInfo* findMethod(Il2CppClass* cls, const char* name, const char* result,
                             bool instance, std::initializer_list<const char*> args) {
    auto& a = il2cpp::api();
    if (!cls || !a.class_get_methods || !a.method_get_name ||
        !a.method_is_instance || !a.method_get_param_count ||
        !a.method_get_param || !a.method_get_return_type) return nullptr;
    void* cursor = nullptr;
    const MethodInfo* found = nullptr;
    while (const MethodInfo* method = a.class_get_methods(cls, &cursor)) {
        const char* current = a.method_get_name(method);
        if (!current || std::strcmp(name, current) ||
            a.method_is_instance(method) != instance ||
            a.method_get_param_count(method) != args.size() ||
            !typeMatches(a.method_get_return_type(method), result)) continue;
        uint32_t index = 0;
        bool match = true;
        for (const char* arg : args)
            if (!typeMatches(a.method_get_param(method, index++), arg)) {
                match = false;
                break;
            }
        if (!match) continue;
        if (found) return nullptr;
        found = method;
    }
    return found;
}

bool invoke(const MethodInfo* method, Il2CppObject* self, void** args,
            Il2CppObject** result = nullptr) {
    auto& a = il2cpp::api();
    if (!method || !a.runtime_invoke) return false;
    Il2CppObject* error = nullptr;
    Il2CppObject* value = a.runtime_invoke(method, self, args, &error);
    if (result) *result = error ? nullptr : value;
    return !error;
}

bool field(Il2CppClass* cls, const char* name, const char* type,
           size_t* offset) {
    auto& a = il2cpp::api();
    if (!cls || !offset || !a.class_get_field_from_name ||
        !a.field_get_type || !a.field_get_flags || !a.field_get_offset ||
        !a.class_instance_size) return false;
    FieldInfo* f = a.class_get_field_from_name(cls, name);
    if (!f || (a.field_get_flags(f) & 0x10u) ||
        !typeMatches(a.field_get_type(f), type)) return false;
    const size_t size = a.class_instance_size(cls);
    const size_t at = a.field_get_offset(f);
    if (at < sizeof(Il2CppObject) || at + sizeof(void*) > size) return false;
    *offset = at;
    return true;
}

bool assign(Il2CppObject* target, size_t offset, Il2CppObject* value) {
    auto& a = il2cpp::api();
    if (!target || !value || !a.gc_wbarrier_set_field) return false;
    auto** slot = reinterpret_cast<void**>(reinterpret_cast<char*>(target) + offset);
    a.gc_wbarrier_set_field(target, slot, value);
    return true;
}

bool resolve(Methods* out) {
    if (!out) return false;
    auto& a = il2cpp::api();
    if (!a.object_new || !a.string_new || !a.gchandle_new ||
        !a.gchandle_free || !a.gchandle_get_target ||
        !a.field_static_get_value || !a.gc_wbarrier_set_field) return false;
    auto main = il2cpp::findClassQuiet("Terraria", "Main");
    auto manager = il2cpp::findClassQuiet("Terraria.Achievements", "AchievementManager");
    out->achievement = il2cpp::findClassQuiet("Terraria.Achievements", "Achievement");
    out->localizedText = il2cpp::findClassQuiet("Terraria.Localization", "LocalizedText");
    if (!main || !manager || !out->achievement || !out->localizedText ||
        !a.class_get_field_from_name || !a.field_get_type ||
        !a.field_get_flags) return false;
    FieldInfo* field = a.class_get_field_from_name(main, "Achievements");
    if (!field || !(a.field_get_flags(field) & 0x10u) ||
        !typeMatches(a.field_get_type(field), "Terraria.Achievements.AchievementManager"))
        return false;
    a.field_static_get_value(field, &out->manager);
    if (!out->manager) return false;
    out->ctor = findMethod(out->achievement, ".ctor", "System.Void",
                           true, {"System.String"});
    out->localizedCtor = findMethod(out->localizedText, ".ctor", "System.Void",
                                    true, {"System.String", "System.String"});
    out->addCondition = findMethod(out->achievement, "AddCondition", "System.Void",
                                   true, {"Terraria.Achievements.AchievementCondition"});
    out->registerAchievement = findMethod(manager, "Register", "System.Void",
                                           true, {"Terraria.Achievements.Achievement"});
    out->unregisterAchievement = findMethod(manager, "Unregister", "System.Void",
                                             true, {"Terraria.Achievements.Achievement"});
    out->getAchievement = findMethod(manager, "GetAchievement",
                                     "Terraria.Achievements.Achievement",
                                     true, {"System.String"});
    out->registerIcon = findMethod(manager, "RegisterIconIndex",
                                   "System.Void", true,
                                   {"System.String", "System.Int32"});
    out->registerCategory = findMethod(manager, "RegisterAchievementCategory",
                                       "System.Void", true,
                                       {"System.String", "Terraria.Achievements.AchievementCategory"});
    out->useCountTracker = findMethod(out->achievement, "UseConditionsCompletedTracker",
                                      "System.Void", true, {});
    out->useValueTracker = findMethod(out->achievement, "UseTrackerFromCondition",
                                      "System.Void", true, {"System.String"});
    return out->ctor && out->localizedCtor && out->addCondition &&
           out->registerAchievement && out->unregisterAchievement &&
           out->getAchievement && out->registerIcon && out->registerCategory &&
           out->useCountTracker && out->useValueTracker;
}

bool lookup(const Methods& refs, const std::string& name, Il2CppObject** result) {
    auto* key = il2cpp::api().string_new(name.c_str());
    void* arguments[] = {key};
    return invoke(refs.getAchievement, refs.manager, arguments, result);
}

bool localized(const Methods& refs, const std::string& key,
               const std::string& fallback, Il2CppObject** result) {
    auto& a = il2cpp::api();
    Il2CppObject* object = a.object_new(refs.localizedText);
    if (!object) return false;
    void* args[] = {a.string_new(key.c_str()), a.string_new(fallback.c_str())};
    if (!args[0] || !args[1] || !invoke(refs.localizedCtor, object, args))
        return false;
    *result = object;
    return true;
}

const MethodInfo* setter(int kind) {
    if (kind == 0) return nullptr;
    const char* type = kind == 1 ? "CustomIntCondition" : "CustomFloatCondition";
    auto cls = il2cpp::findClassQuiet("Terraria.GameContent.Achievements", type);
    return findMethod(cls, "set_Value", "System.Void", true,
                      {kind == 1 ? "System.Int32" : "System.Single"});
}

bool setProgress(Il2CppObject* condition, int kind, float value) {
    if (kind == 0) {
        if (value == 0) return true;
        auto cls = il2cpp::findClassQuiet("Terraria.Achievements", "AchievementCondition");
        auto completion = findMethod(cls, "Complete", "System.Void", true, {});
        return completion && invoke(completion, condition, nullptr);
    }
    const MethodInfo* property = setter(kind);
    if (!property) return false;
    int32_t integral = static_cast<int32_t>(value);
    float decimal = value;
    void* args[] = {kind == 1 ? static_cast<void*>(&integral)
                              : static_cast<void*>(&decimal)};
    return invoke(property, condition, args);
}

bool makeCondition(const ConditionDescriptor& definition, float value,
                   Il2CppObject** condition) {
    if (!condition || definition.kind < 0 || definition.kind > 2 ||
        definition.maximum > 10000000.f || definition.maximum < 1.f ||
        value < 0.f || value > definition.maximum) return false;
    static const char* names[] = {"CustomFlagCondition", "CustomIntCondition",
                                   "CustomFloatCondition"};
    auto cls = il2cpp::findClassQuiet("Terraria.GameContent.Achievements",
                                     names[definition.kind]);
    if (!cls) return false;
    const char* arg = definition.kind == 1 ? "System.Int32" : "System.Single";
    const MethodInfo* creator = definition.kind == 0
        ? findMethod(cls, "Create", "Terraria.Achievements.AchievementCondition",
                     false, {"System.String"})
        : findMethod(cls, "Create", "Terraria.Achievements.AchievementCondition",
                     false, {"System.String", arg});
    if (!creator || (definition.kind > 0 && !setter(definition.kind))) return false;
    auto& api = il2cpp::api();
    void* args[2] = {api.string_new(definition.key.c_str()), nullptr};
    if (!args[0]) return false;
    int32_t maximum = static_cast<int32_t>(definition.maximum);
    float floatMax = definition.maximum;
    if (definition.kind > 0)
        args[1] = definition.kind == 1
            ? static_cast<void*>(&maximum) : static_cast<void*>(&floatMax);
    if (!invoke(creator, nullptr, args, condition) || !*condition) return false;
    return setProgress(*condition, definition.kind, value);
}

void freeRoots(uint32_t root, const std::vector<uint32_t>& conditions) {
    auto& api = il2cpp::api();
    if (!api.gchandle_free) return;
    for (uint32_t handle : conditions)
        if (handle) api.gchandle_free(handle);
    if (root) api.gchandle_free(root);
}

}

NativeAchievementManager& nativeAchievementManager() {
    static NativeAchievementManager manager;
    return manager;
}

bool NativeAchievementManager::owns(const std::string& mod) const {
    return mods_.contains(mod);
}

bool NativeAchievementManager::install(
    const NativeRegistrationPlan& plan,
    const std::vector<Descriptor>& definitions,
    const std::vector<AchievementState>& states,
    std::string* error) {
    if (!error) return false;
    error->clear();
    if (!plan.prepared || definitions.size() != plan.names.size() ||
        states.size() != definitions.size()) {
        *error = "PlanNotReady";
        return false;
    }
    auto exists = mods_.find(plan.owner);
    if (exists != mods_.end()) {
        if (exists->second.revision == plan.revision) return true;
        *error = "ExistingRevisionMustUnloadFirst";
        return false;
    }
    if (!inspectManagerContracts(definitions).canUnregister) {
        *error = "NoSafeNativeUnregister";
        return false;
    }
    Methods refs;
    if (!resolve(&refs)) {
        *error = "NativeManagerContractUnavailable";
        return false;
    }
    size_t nameOffset = 0, descriptionOffset = 0;
    if (!field(refs.achievement, "FriendlyName",
               "Terraria.Localization.LocalizedText", &nameOffset) ||
        !field(refs.achievement, "Description",
               "Terraria.Localization.LocalizedText", &descriptionOffset)) {
        *error = "LocalizedTextFieldMismatch";
        return false;
    }
    for (const auto& descriptor : definitions) {
        Il2CppObject* prior = nullptr;
        if (!lookup(refs, descriptor.name, &prior) || prior) {
            *error = "NameAlreadyRegisteredOrLookupFailed";
            return false;
        }
    }

    Owner staging;
    staging.revision = plan.revision;
    auto rollback = [&] {
        bool okay = true;
        Owner retained;
        retained.revision = plan.revision;
        for (auto it = staging.entries.rbegin(); it != staging.entries.rend(); ++it) {
            auto& owned = *it;
            Il2CppObject* object = il2cpp::api().gchandle_get_target(owned.root);
            Il2CppObject* registered = nullptr;
            if (!object || !lookup(refs, owned.name, &registered)) {
                okay = false;
                retained.entries.push_back(std::move(owned));
                continue;
            }
            bool cleared = registered == nullptr;
            if (registered == object) {
                void* args[] = {object};
                Il2CppObject* after = nullptr;
                cleared = invoke(refs.unregisterAchievement, refs.manager, args) &&
                          lookup(refs, owned.name, &after) && after == nullptr;
            }
            if (cleared) freeRoots(owned.root, owned.conditions);
            else {
                okay = false;
                retained.entries.push_back(std::move(owned));
            }
        }
        staging.entries.clear();
        if (!retained.entries.empty()) {
            std::reverse(retained.entries.begin(), retained.entries.end());
            mods_.emplace(plan.owner, std::move(retained));
        }
        return okay;
    };

    for (size_t i = 0; i < definitions.size(); ++i) {
        const auto& definition = definitions[i];
        const auto& state = states[i];
        if (definition.name != plan.names[i] || state.name != definition.name) {
            *error = "PlanIdentityChanged";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        auto& api = il2cpp::api();
        Il2CppObject* object = api.object_new(refs.achievement);
        Il2CppString* key = api.string_new(definition.name.c_str());
        void* ctorArgs[] = {key};
        if (!object || !key || !invoke(refs.ctor, object, ctorArgs)) {
            *error = "NativeAchievementCtorFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        uint32_t root = api.gchandle_new(object, false);
        if (!root) {
            *error = "AchievementRootFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        Entry entry;
        entry.name = definition.name;
        entry.root = root;
        auto releaseEntry = [&] { freeRoots(entry.root, entry.conditions); };

        Il2CppObject *friendly = nullptr, *description = nullptr;
        if (!localized(refs, "Achievements." + definition.name + "_Name",
                       definition.friendlyName, &friendly) ||
            !localized(refs, "Achievements." + definition.name + "_Description",
                       definition.description, &description) ||
            !assign(object, nameOffset, friendly) ||
            !assign(object, descriptionOffset, description)) {
            releaseEntry();
            *error = "AchievementTextFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        bool conditionsReady = true;
        for (size_t j = 0; j < definition.conditions.size(); ++j) {
            Il2CppObject* condition = nullptr;
            if (!makeCondition(definition.conditions[j], state.conditions[j].value, &condition)) {
                conditionsReady = false;
                break;
            }
            uint32_t conditionRoot = api.gchandle_new(condition, false);
            if (!conditionRoot) {
                conditionsReady = false;
                break;
            }
            entry.conditions.push_back(conditionRoot);
            void* args[] = {condition};
            if (!invoke(refs.addCondition, object, args)) {
                conditionsReady = false;
                break;
            }
        }
        if (conditionsReady && definition.conditions.size() > 1)
            conditionsReady = invoke(refs.useCountTracker, object, nullptr);
        else if (conditionsReady && definition.conditions.size() == 1 &&
                 definition.conditions[0].kind > 0) {
            void* args[] = {api.string_new(definition.conditions[0].key.c_str())};
            conditionsReady = args[0] && invoke(refs.useValueTracker, object, args);
        }
        if (!conditionsReady) {
            releaseEntry();
            *error = "NativeAchievementConditionsFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        void* registerArgs[] = {object};
        if (!invoke(refs.registerAchievement, refs.manager, registerArgs)) {
            Il2CppObject* potentiallyRegistered = nullptr;
            if (lookup(refs, definition.name, &potentiallyRegistered) &&
                potentiallyRegistered == object)
                staging.entries.push_back(std::move(entry));
            else
                releaseEntry();
            *error = "ManagerRegisterFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
        staging.entries.push_back(std::move(entry));
        Il2CppString* name = api.string_new(definition.name.c_str());
        int index = definition.index;
        int category = definition.category;
        void* iconArgs[] = {name, &index};
        void* categoryArgs[] = {name, &category};
        Il2CppObject* confirmed = nullptr;
        if (!name || !invoke(refs.registerIcon, refs.manager, iconArgs) ||
            !invoke(refs.registerCategory, refs.manager, categoryArgs) ||
            !lookup(refs, definition.name, &confirmed) || confirmed != object) {
            *error = "ManagerRegistrationVerificationFailed";
            return rollback() ? false : (*error += " RollbackFailed", false);
        }
    }
    mods_.emplace(plan.owner, std::move(staging));
    return true;
}

bool NativeAchievementManager::synchronize(
    const std::string& mod, const std::vector<AchievementState>& states,
    std::string* error) {
    if (!error) return false;
    auto owner = mods_.find(mod);
    if (owner == mods_.end() || owner->second.entries.size() != states.size()) {
        *error = "NativeManagerNotInstalled";
        return false;
    }
    for (size_t i = 0; i < states.size(); ++i) {
        const auto& entry = owner->second.entries[i];
        const auto& state = states[i];
        if (entry.name != state.name || entry.conditions.size() != state.conditions.size()) {
            *error = "StateIdentityMismatch";
            return false;
        }
        for (size_t j = 0; j < state.conditions.size(); ++j) {
            auto* condition = il2cpp::api().gchandle_get_target(entry.conditions[j]);
            if (!condition ||
                !setProgress(condition, state.conditions[j].kind,
                             state.conditions[j].value)) {
                *error = "NativeConditionUpdateFailed";
                return false;
            }
        }
    }
    error->clear();
    return true;
}

bool NativeAchievementManager::retire(const std::string& mod, std::string* error) {
    if (!error) return false;
    auto it = mods_.find(mod);
    if (it == mods_.end()) return true;
    Methods refs;
    if (!resolve(&refs)) {
        *error = "NativeUnregisterContractUnavailable";
        return false;
    }
    for (auto entry = it->second.entries.rbegin(); entry != it->second.entries.rend(); ++entry) {
        Il2CppObject* target = il2cpp::api().gchandle_get_target(entry->root);
        if (!target) {
            *error = "OwnedAchievementLost";
            return false;
        }
        Il2CppObject* current = nullptr;
        if (!lookup(refs, entry->name, &current) ||
            (current && current != target)) {
            *error = "AchievementOwnerMismatch";
            return false;
        }
        if (current) {
            void* args[] = {target};
            Il2CppObject* after = nullptr;
            if (!invoke(refs.unregisterAchievement, refs.manager, args) ||
                !lookup(refs, entry->name, &after) || after != nullptr) {
                *error = "NativeUnregisterFailed";
                return false;
            }
        }
    }
    for (auto& entry : it->second.entries)
        freeRoots(entry.root, entry.conditions);
    mods_.erase(it);
    error->clear();
    return true;
}

}
