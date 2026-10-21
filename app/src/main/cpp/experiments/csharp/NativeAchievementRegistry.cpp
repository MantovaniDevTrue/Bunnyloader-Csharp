#include "experiments/csharp/NativeAchievementRegistry.h"

#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

// A origem das assinaturas e a classe AchievementLoader do ExampleModBunny.
// Nunca usar metodo por nome/contagem se houver overloads indistintos.
struct NativeMethods {
    Il2CppClass* achievement = nullptr;
    Il2CppClass* condition = nullptr;
    Il2CppClass* localizedText = nullptr;
    Il2CppObject* manager = nullptr;
    const MethodInfo* achievementCtor = nullptr;
    const MethodInfo* localizedCtor = nullptr;
    const MethodInfo* conditionFactory = nullptr;
    const MethodInfo* addCondition = nullptr;
    const MethodInfo* trackCondition = nullptr;
    const MethodInfo* registerAchievement = nullptr;
    const MethodInfo* registerIcon = nullptr;
    const MethodInfo* registerCategory = nullptr;
    const MethodInfo* setProgress = nullptr;
    const MethodInfo* setTitle = nullptr;
    const MethodInfo* setDescription = nullptr;
    int titleOffset = -1;
    int descriptionOffset = -1;
};

struct NativeEntry {
    uint32_t achievementRoot = 0;
    uint32_t conditionRoot = 0;
    int applied = -1;
    bool registered = false;
};

NativeMethods methods{};
std::map<std::string, NativeEntry> entries;
std::string lastError;
std::uint64_t revisions = 0;
bool prepared = false;

const MethodInfo* require(Il2CppClass* cls, const char* declaration,
                          bool instance, std::string* error) {
    const auto& a = il2cpp::api();
    const auto signature = il2cpp::parseSignature(declaration);
    const MethodInfo* fn = cls ? il2cpp::findMethodBySignature(cls, signature) : nullptr;
    if (!fn || !a.method_is_instance || a.method_is_instance(fn) != instance) {
        *error = std::string("Native achievement signature unavailable: ") + declaration;
        return nullptr;
    }
    return fn;
}

bool invoke(const MethodInfo* fn, void* owner, void** args,
            Il2CppObject** result, std::string* error) {
    auto& a = il2cpp::api();
    if (!a.runtime_invoke || !fn) {
        *error = "Native achievement invocation API unavailable";
        return false;
    }
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = a.runtime_invoke(fn, owner, args, &exception);
    if (exception) {
        *error = "Native achievement invocation failed: " +
                 il2cpp::describeMethod(fn);
        return false;
    }
    if (result) *result = value;
    return true;
}

bool setLocalized(Il2CppObject* achievement, Il2CppObject* title,
                  Il2CppObject* description, std::string* error) {
    auto& a = il2cpp::api();
    if (methods.setTitle) {
        void* args[]{title};
        if (!invoke(methods.setTitle, achievement, args, nullptr, error)) return false;
    } else {
        if (!a.gc_wbarrier_set_field || methods.titleOffset < 0) {
            *error = "Missing achievement title reference barrier";
            return false;
        }
        void** location = reinterpret_cast<void**>(
            reinterpret_cast<char*>(achievement) + methods.titleOffset);
        a.gc_wbarrier_set_field(achievement, location, title);
    }
    if (methods.setDescription) {
        void* args[]{description};
        if (!invoke(methods.setDescription, achievement, args, nullptr, error)) return false;
    } else {
        if (!a.gc_wbarrier_set_field || methods.descriptionOffset < 0) {
            *error = "Missing achievement description reference barrier";
            return false;
        }
        void** location = reinterpret_cast<void**>(
            reinterpret_cast<char*>(achievement) + methods.descriptionOffset);
        a.gc_wbarrier_set_field(achievement, location, description);
    }
    return true;
}

bool preflight(std::string* error) {
    if (prepared) return true;
    auto& a = il2cpp::api();
    if (!a.object_new || !a.string_new || !a.gchandle_new ||
        !a.gchandle_get_target || !a.gchandle_free || !a.runtime_invoke ||
        !a.field_static_get_value || !a.field_get_type ||
        !a.class_from_il2cpp_type || !a.class_get_name ||
        !a.class_get_namespace) {
        *error = "Native achievement required IL2CPP exports absent";
        return false;
    }
    auto* mainClass = il2cpp::findClass({"Terraria", "Main", {}});
    methods.achievement = il2cpp::findClass({"Terraria.Achievements", "Achievement", {}});
    methods.condition = il2cpp::findClass({
        "Terraria.GameContent.Achievements", "CustomIntCondition", {}});
    methods.localizedText = il2cpp::findClass({
        "Terraria.Localization", "LocalizedText", {}});
    if (!mainClass || !methods.achievement || !methods.condition ||
        !methods.localizedText) {
        *error = "Native achievement classes unavailable";
        return false;
    }
    // O Bunny JS acessa Main.Achievements. Achar o campo não é suficiente:
    // confirmar também o tipo real do objeto antes de chamar métodos.
    FieldInfo* managerField = il2cpp::findField(mainClass, "Achievements");
    if (!managerField) {
        *error = "Native Main.Achievements field unavailable";
        return false;
    }
    Il2CppClass* managerClass = a.class_from_il2cpp_type(a.field_get_type(managerField));
    const char* managerNs = managerClass ? a.class_get_namespace(managerClass) : nullptr;
    const char* managerName = managerClass ? a.class_get_name(managerClass) : nullptr;
    if (!managerNs || !managerName ||
        std::strcmp(managerNs, "Terraria.Achievements") != 0 ||
        std::strcmp(managerName, "AchievementManager") != 0) {
        *error = "Main.Achievements has unexpected type";
        return false;
    }
    Il2CppObject* manager = nullptr;
    a.field_static_get_value(managerField, &manager);
    if (!manager) {
        *error = "Main.Achievements not initialized";
        return false;
    }

    NativeMethods resolved{};
    resolved.achievement = methods.achievement;
    resolved.condition = methods.condition;
    resolved.localizedText = methods.localizedText;
    resolved.manager = manager;
    resolved.achievementCtor = require(resolved.achievement,
        "void .ctor(string name)", true, error);
    resolved.localizedCtor = require(resolved.localizedText,
        "void .ctor(string key, string text)", true, error);
    resolved.conditionFactory = require(resolved.condition,
        "AchievementCondition Create(string name, int maxValue)", false, error);
    resolved.addCondition = require(resolved.achievement,
        "void AddCondition(AchievementCondition condition)", true, error);
    resolved.trackCondition = require(resolved.achievement,
        "void UseTrackerFromCondition(string conditionName)", true, error);
    resolved.registerAchievement = require(managerClass,
        "void Register(Achievement achievement)", true, error);
    resolved.registerIcon = require(managerClass,
        "void RegisterIconIndex(string achievementName, int iconIndex)", true, error);
    resolved.registerCategory = require(managerClass,
        "void RegisterAchievementCategory(string achievementName, AchievementCategory category)",
        true, error);
    resolved.setProgress = require(resolved.condition,
        "void set_Value(int value)", true, error);
    if (!resolved.achievementCtor || !resolved.localizedCtor ||
        !resolved.conditionFactory || !resolved.addCondition ||
        !resolved.trackCondition || !resolved.registerAchievement ||
        !resolved.registerIcon || !resolved.registerCategory ||
        !resolved.setProgress)
        return false;

    // FriendlyName/Description são propriedades no Bunny. Aceitar setter
    // validado ou campo de referência com write barrier, nunca offset cru.
    for (const auto& fieldName :
         {std::pair{"FriendlyName", &resolved.titleOffset},
          std::pair{"Description", &resolved.descriptionOffset}}) {
        std::string fieldError;
        if (!il2cpp::nativeFieldContract(resolved.achievement, fieldName.first,
                "Terraria.Localization.LocalizedText", false, sizeof(void*),
                nullptr, fieldName.second, &fieldError)) {
            const std::string signature =
                std::string("void set_") + fieldName.first +
                "(LocalizedText value)";
            auto* setter = require(resolved.achievement,
                                  signature.c_str(), true, error);
            if (!setter) {
                *error = fieldError + " / " + *error;
                return false;
            }
            if (std::strcmp(fieldName.first, "FriendlyName") == 0)
                resolved.setTitle = setter;
            else resolved.setDescription = setter;
        } else if (!a.gc_wbarrier_set_field) {
            *error = "LocalizedText field requires IL2CPP write barrier";
            return false;
        }
    }
    methods = resolved;
    prepared = true;
    return true;
}

Il2CppObject* textObject(const std::string& key, const std::string& value,
                         std::string* error) {
    auto& a = il2cpp::api();
    Il2CppObject* result = a.object_new(methods.localizedText);
    if (!result) { *error = "LocalizedText allocation failed"; return nullptr; }
    auto* nativeKey = a.string_new(key.c_str());
    auto* nativeValue = a.string_new(value.c_str());
    if (!nativeKey || !nativeValue) {
        *error = "LocalizedText strings allocation failed";
        return nullptr;
    }
    void* args[]{nativeKey, nativeValue};
    if (!invoke(methods.localizedCtor, result, args, nullptr, error))
        return nullptr;
    return result;
}

constexpr int kScaledTrackerMax = 1000000;

int scaled(double fraction) {
    return static_cast<int>(std::clamp(std::llround(fraction * kScaledTrackerMax),
                                      0LL, static_cast<long long>(kScaledTrackerMax)));
}

bool apply(NativeEntry& entry, const AchievementMetadata& definition,
           std::string* error) {
    auto& a = il2cpp::api();
    Il2CppObject* condition = a.gchandle_get_target(entry.conditionRoot);
    if (!condition) { *error = "Native condition GC root lost"; return false; }
    const int progress = scaled(definition.progress);
    if (entry.applied == progress) return true;
    int value = progress;
    void* args[]{&value};
    if (!invoke(methods.setProgress, condition, args, nullptr, error)) return false;
    entry.applied = progress;
    ++revisions;
    return true;
}

bool make(const AchievementMetadata& definition, NativeEntry* out,
          std::string* error) {
    auto& a = il2cpp::api();
    auto* achievement = a.object_new(methods.achievement);
    if (!achievement) { *error = "Achievement allocation failed"; return false; }
    auto* name = a.string_new(definition.name.c_str());
    if (!name) { *error = "Achievement name allocation failed"; return false; }
    void* ctorArgs[]{name};
    if (!invoke(methods.achievementCtor, achievement, ctorArgs, nullptr, error))
        return false;

    auto* title = textObject("Achievements." + definition.name + "_Name",
                             definition.title, error);
    auto* description = textObject("Achievements." + definition.name + "_Description",
                                   definition.description, error);
    if (!title || !description ||
        !setLocalized(achievement, title, description, error)) return false;

    auto* key = a.string_new("BunnyProgress");
    int maximum = kScaledTrackerMax;
    if (!key) { *error = "Condition key allocation failed"; return false; }
    void* conditionArgs[]{key, &maximum};
    Il2CppObject* condition = nullptr;
    if (!invoke(methods.conditionFactory, nullptr,
                conditionArgs, &condition, error) || !condition) {
        if (error->empty()) *error = "CustomIntCondition factory returned null";
        return false;
    }
    // Como a fonte gerenciada mantém a semântica das condições, a condição
    // nativa é um espelho para o menu/trackers. Não duplicar eventos nativos.
    void* addArgs[]{condition};
    if (!invoke(methods.addCondition, achievement, addArgs, nullptr, error))
        return false;
    void* trackerArgs[]{key};
    if (!invoke(methods.trackCondition, achievement, trackerArgs, nullptr, error))
        return false;

    // Recuperar o progresso ANTES do Register(Achievement). Conquistas de
    // sessões anteriores não devem parecer novos desbloqueios no menu.
    int initialProgress = scaled(definition.progress);
    void* initialArgs[]{&initialProgress};
    if (!invoke(methods.setProgress, condition, initialArgs, nullptr, error))
        return false;
    out->applied = initialProgress;

    out->achievementRoot = a.gchandle_new(achievement, false);
    out->conditionRoot = a.gchandle_new(condition, false);
    if (!out->achievementRoot || !out->conditionRoot) {
        *error = "Achievement GC root allocation failed";
        if (out->achievementRoot) a.gchandle_free(out->achievementRoot);
        if (out->conditionRoot) a.gchandle_free(out->conditionRoot);
        *out = {};
        return false;
    }
    return true;
}

bool commit(const AchievementMetadata& definition, NativeEntry* entry,
            std::string* error) {
    auto& a = il2cpp::api();
    auto* achievement = a.gchandle_get_target(entry->achievementRoot);
    if (!achievement) { *error = "Native achievement root unavailable"; return false; }
    void* args[]{achievement};
    if (!invoke(methods.registerAchievement, methods.manager, args, nullptr, error))
        return false;
    entry->registered = true;
    auto* name = a.string_new(definition.name.c_str());
    if (!name) { *error = "RegisterIconIndex string allocation failed"; return false; }
    int index = definition.index;
    int category = definition.category;
    void* iconArgs[]{name, &index};
    if (!invoke(methods.registerIcon, methods.manager, iconArgs, nullptr, error))
        return false;
    void* categoryArgs[]{name, &category};
    if (!invoke(methods.registerCategory, methods.manager, categoryArgs, nullptr, error))
        return false;
    return true;
}
}

bool registerNativeAchievementObjects(
    const std::vector<AchievementMetadata>& definitions,
    std::string* error) {
    if (!error) return false;
    error->clear();
    if (!preflight(error)) { lastError = *error; return false; }
    if (definitions.empty() || definitions.size() > 256) {
        *error = "Invalid achievement registration batch";
        lastError = *error;
        return false;
    }

    // O mutex do registro nativo nunca é segurado durante runtime_invoke:
    // callbacks do jogo podem ser reentrantes.
    for (const auto& definition : definitions) {
        auto it = entries.find(definition.name);
        if (it != entries.end()) continue;
        NativeEntry entry{};
        if (!make(definition, &entry, error)) {
            lastError = *error;
            return false;
        }
        // Se o Add/Register nativo falhar, o nome não é registrado novamente
        // na mesma sessão: evita corromper o AchievementManager por retry.
        auto [inserted, created] = entries.emplace(definition.name, entry);
        if (!created) {
            *error = "Duplicate native achievement identity";
            return false;
        }
        if (!commit(definition, &inserted->second, error)) {
            lastError = *error;
            return false;
        }
    }
    return reconcileNativeAchievementObjects(definitions, error);
}

bool reconcileNativeAchievementObjects(
    const std::vector<AchievementMetadata>& definitions,
    std::string* error) {
    if (!error) return false;
    if (!prepared) { *error = "Native achievement registry uninitialized"; return false; }
    for (const auto& definition : definitions) {
        auto it = entries.find(definition.name);
        if (it == entries.end() || !it->second.registered) {
            *error = "Native achievement not registered: " + definition.name;
            return false;
        }
        if (!apply(it->second, definition, error)) {
            lastError = *error;
            return false;
        }
    }
    error->clear();
    return true;
}

NativeAchievementRegistryStatus nativeAchievementRegistryStatus() {
    NativeAchievementRegistryStatus status{};
    status.registered = static_cast<std::size_t>(
        std::count_if(entries.begin(), entries.end(),
                      [](const auto& item) { return item.second.registered; }));
    status.pending = entries.size() - status.registered;
    status.appliedRevisions = revisions;
    status.bindingsReady = prepared;
    status.error = lastError;
    return status;
}

}
