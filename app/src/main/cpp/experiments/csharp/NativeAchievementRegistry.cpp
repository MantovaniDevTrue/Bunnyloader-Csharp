#include "experiments/csharp/NativeAchievementRegistry.h"
#include "experiments/csharp/NativeAchievementProgress.h"
#include "experiments/csharp/ContentTexture.h"
#include "content/common/ContentAssets.h"
#include "mods/ModLoader.h"

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
    Il2CppClass* flagCondition = nullptr;
    Il2CppClass* floatCondition = nullptr;
    Il2CppClass* localizedText = nullptr;
    Il2CppObject* manager = nullptr;
    const MethodInfo* achievementCtor = nullptr;
    const MethodInfo* localizedCtor = nullptr;
    const MethodInfo* conditionFactory = nullptr;
    const MethodInfo* flagFactory = nullptr;
    const MethodInfo* completeFlag = nullptr;
    const MethodInfo* floatFactory = nullptr;
    const MethodInfo* addCondition = nullptr;
    const MethodInfo* trackCondition = nullptr;
    const MethodInfo* trackMultiple = nullptr;
    const MethodInfo* registerAchievement = nullptr;
    const MethodInfo* registerIcon = nullptr;
    const MethodInfo* registerCategory = nullptr;
    const MethodInfo* setProgress = nullptr;
    const MethodInfo* setFloatProgress = nullptr;
    const MethodInfo* setTitle = nullptr;
    const MethodInfo* setDescription = nullptr;
    int titleOffset = -1;
    int descriptionOffset = -1;
};

struct NativeCondition {
    uint32_t root = 0;
    std::string key;
    int kind = 0;
    double maximum = 1;
    double applied = -1;
};

struct NativeEntry {
    uint32_t achievementRoot = 0;
    std::vector<NativeCondition> conditions;
    uint32_t titleRoot = 0;
    uint32_t descriptionRoot = 0;
    uint32_t hiddenRoot = 0;
    uint32_t assetRoot = 0;
    bool hiddenApplied = false;
    bool managerRegistered = false;
    bool iconRegistered = false;
    bool registered = false;
};

NativeMethods methods{};
std::map<std::string, NativeEntry> entries;
std::map<const Il2CppObject*, uint32_t> iconsByAchievement;
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
    methods.flagCondition = il2cpp::findClass({
        "Terraria.GameContent.Achievements", "CustomFlagCondition", {}});
    methods.floatCondition = il2cpp::findClass({
        "Terraria.GameContent.Achievements", "CustomFloatCondition", {}});
    methods.localizedText = il2cpp::findClass({
        "Terraria.Localization", "LocalizedText", {}});
    if (!mainClass || !methods.achievement || !methods.condition ||
        !methods.flagCondition || !methods.floatCondition || !methods.localizedText) {
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
    resolved.flagCondition = methods.flagCondition;
    resolved.floatCondition = methods.floatCondition;
    resolved.localizedText = methods.localizedText;
    resolved.manager = manager;
    resolved.achievementCtor = require(resolved.achievement,
        "void .ctor(string name)", true, error);
    resolved.localizedCtor = require(resolved.localizedText,
        "void .ctor(string key, string text)", true, error);
    resolved.conditionFactory = require(resolved.condition,
        "AchievementCondition Create(string name, int maxValue)", false, error);
    resolved.flagFactory = require(resolved.flagCondition,
        "AchievementCondition Create(string name)", false, error);
    auto* baseCondition = il2cpp::findClass({
        "Terraria.Achievements", "AchievementCondition", {}});
    resolved.completeFlag = require(baseCondition, "void Complete()", true, error);
    resolved.floatFactory = require(resolved.floatCondition,
        "AchievementCondition Create(string name, float maxValue)", false, error);
    resolved.addCondition = require(resolved.achievement,
        "void AddCondition(AchievementCondition condition)", true, error);
    resolved.trackCondition = require(resolved.achievement,
        "void UseTrackerFromCondition(string conditionName)", true, error);
    resolved.trackMultiple = require(resolved.achievement,
        "void UseConditionsCompletedTracker()", true, error);
    resolved.registerAchievement = require(managerClass,
        "void Register(Achievement achievement)", true, error);
    resolved.registerIcon = require(managerClass,
        "void RegisterIconIndex(string achievementName, int iconIndex)", true, error);
    resolved.registerCategory = require(managerClass,
        "void RegisterAchievementCategory(string achievementName, AchievementCategory category)",
        true, error);
    resolved.setProgress = require(resolved.condition,
        "void set_Value(int value)", true, error);
    resolved.setFloatProgress = require(resolved.floatCondition,
        "void set_Value(float value)", true, error);
    if (!resolved.achievementCtor || !resolved.localizedCtor ||
        !resolved.conditionFactory || !resolved.floatFactory ||
        !resolved.flagFactory || !resolved.completeFlag ||
        !resolved.addCondition || !resolved.trackCondition ||
        !resolved.trackMultiple || !resolved.registerAchievement ||
        !resolved.registerIcon || !resolved.registerCategory ||
        !resolved.setProgress || !resolved.setFloatProgress)
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

bool setConditionValue(const NativeCondition& condition, double value,
                       std::string* error) {
    auto& a = il2cpp::api();
    Il2CppObject* target = a.gchandle_get_target(condition.root);
    if (!target) { *error = "Native achievement condition GC root lost"; return false; }
    if (condition.kind == 0) {
        if (value < 1) {
            if (condition.applied > 0) {
                *error = "Native flag cannot be revoked within the same profile";
                return false;
            }
            return true;
        }
        return invoke(methods.completeFlag, target, nullptr, nullptr, error);
    }
    if (condition.kind == 2) {
        float native = static_cast<float>(value);
        void* args[]{&native};
        return invoke(methods.setFloatProgress, target, args, nullptr, error);
    }
    int native = static_cast<int>(value);
    void* args[]{&native};
    return invoke(methods.setProgress, target, args, nullptr, error);
}

bool apply(NativeEntry& entry, const AchievementMetadata& definition,
           std::string* error) {
    auto& a = il2cpp::api();
    if (entry.conditions.size() != definition.conditions.size()) {
        *error = "Native achievement condition count mismatch";
        return false;
    }
    const bool hidden = definition.hidden && !definition.completed;
    if (entry.hiddenApplied != hidden) {
        auto* achievement = a.gchandle_get_target(entry.achievementRoot);
        auto* title = a.gchandle_get_target(hidden ? entry.hiddenRoot : entry.titleRoot);
        auto* description = a.gchandle_get_target(
            hidden ? entry.hiddenRoot : entry.descriptionRoot);
        if (!achievement || !title || !description ||
            !setLocalized(achievement, title, description, error))
            return false;
        entry.hiddenApplied = hidden;
    }
    for (std::size_t i = 0; i < entry.conditions.size(); ++i) {
        auto& native = entry.conditions[i];
        const auto& wanted = definition.conditions[i];
        if (native.key != wanted.key || native.kind != wanted.kind ||
            native.maximum != wanted.maximum) {
            *error = "Native achievement condition identity changed";
            return false;
        }
        if (native.applied == wanted.value) continue;
        if (!setConditionValue(native, wanted.value, error)) return false;
        native.applied = wanted.value;
        ++revisions;
    }
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
    auto* obscured = textObject(
        "Bunny.Achievement.Hidden." + definition.name, "???", error);
    if (!title || !description || !obscured ||
        !setLocalized(achievement,
            definition.hidden && !definition.completed ? obscured : title,
            definition.hidden && !definition.completed ? obscured : description,
            error)) return false;

    out->hiddenApplied = definition.hidden && !definition.completed;
    out->achievementRoot = a.gchandle_new(achievement, false);
    out->titleRoot = a.gchandle_new(title, false);
    out->descriptionRoot = a.gchandle_new(description, false);
    out->hiddenRoot = a.gchandle_new(obscured, false);
    const auto releaseRoots = [&]() {
        for (uint32_t handle : {out->achievementRoot, out->titleRoot,
                                out->descriptionRoot, out->hiddenRoot,
                                out->assetRoot}) {
            if (handle) a.gchandle_free(handle);
        }
        for (const auto& condition : out->conditions)
            if (condition.root) a.gchandle_free(condition.root);
        *out = {};
    };
    if (!out->achievementRoot || !out->titleRoot ||
        !out->descriptionRoot || !out->hiddenRoot) {
        *error = "Native achievement GC root allocation failed";
        releaseRoots();
        return false;
    }
    if (definition.conditions.empty() || definition.conditions.size() > 32) {
        *error = "Native achievement condition list unavailable";
        releaseRoots();
        return false;
    }

    for (const auto& definitionCondition : definition.conditions) {
        const bool isFloat = definitionCondition.kind == 2;
        const bool isFlag = definitionCondition.kind == 0;
        const auto* factory = isFlag ? methods.flagFactory :
                              isFloat ? methods.floatFactory : methods.conditionFactory;
        auto* key = a.string_new(definitionCondition.key.c_str());
        if (!key) {
            *error = "Native condition name allocation failed";
            releaseRoots();
            return false;
        }
        int intMaximum = static_cast<int>(definitionCondition.maximum);
        float floatMaximum = static_cast<float>(definitionCondition.maximum);
        void* args[]{key, isFloat ? static_cast<void*>(&floatMaximum)
                                  : static_cast<void*>(&intMaximum)};
        Il2CppObject* nativeCondition = nullptr;
        if (!invoke(factory, nullptr, args, &nativeCondition, error) ||
            !nativeCondition) {
            if (error->empty()) *error = "Native condition factory returned null";
            releaseRoots();
            return false;
        }
        NativeCondition record;
        record.root = a.gchandle_new(nativeCondition, false);
        record.key = definitionCondition.key;
        record.kind = definitionCondition.kind;
        record.maximum = definitionCondition.maximum;
        if (!record.root) {
            *error = "Native condition GC root allocation failed";
            releaseRoots();
            return false;
        }
        out->conditions.push_back(std::move(record));
        void* addArgs[]{nativeCondition};
        if (!invoke(methods.addCondition, achievement, addArgs, nullptr, error) ||
            !setConditionValue(out->conditions.back(), definitionCondition.value, error)) {
            releaseRoots();
            return false;
        }
        out->conditions.back().applied = definitionCondition.value;
    }
    if (definition.conditions.size() > 1) {
        if (!invoke(methods.trackMultiple, achievement, nullptr, nullptr, error)) {
            releaseRoots();
            return false;
        }
    } else if (definition.conditions[0].kind == 1 ||
               definition.conditions[0].kind == 2) {
        auto* key = a.string_new(definition.conditions[0].key.c_str());
        if (!key) {
            *error = "Native tracker name allocation failed";
            releaseRoots();
            return false;
        }
        void* args[]{key};
        if (!invoke(methods.trackCondition, achievement, args, nullptr, error)) {
            releaseRoots();
            return false;
        }
    }

    const std::size_t uidEnd = definition.name.find('/');
    const std::string uid = definition.name.substr(0, uidEnd);
    const std::string root = bl::mods::rootOf(uid);
    std::string texture, reason;
    if (root.empty() ||
        !normalizeContentTexture(definition.texture.c_str(), &texture, &reason)) {
        *error = "Achievement atlas missing or invalid: " + definition.name +
                 " Reason=" + (root.empty() ? "UnknownModRoot" : reason);
        releaseRoots();
        return false;
    }
    int atlasWidth = 0, atlasHeight = 0;
    Il2CppObject* atlas = bl::runtime::content::loadTextureAsset(
        root + "/content/" + texture, nullptr, 0, "Achievement:" + definition.name,
        &atlasWidth, &atlasHeight);
    if (!atlas) {
        *error = "Achievement atlas failed to decode: " + definition.name;
        releaseRoots();
        return false;
    }
    if (atlasWidth != 592 || atlasHeight != 64) {
        bl::runtime::content::destroyTextureAsset(atlas);
        *error = "Achievement atlas must be 592x64: " + definition.name +
                 " got " + std::to_string(atlasWidth) + "x" +
                 std::to_string(atlasHeight);
        releaseRoots();
        return false;
    }
    out->assetRoot = a.gchandle_new(atlas, false);
    if (!out->assetRoot) {
        *error = "Achievement texture GC root allocation failed";
        bl::runtime::content::destroyTextureAsset(atlas);
        releaseRoots();
        return false;
    }
    return true;
}

bool commit(const AchievementMetadata& definition, NativeEntry* entry,
            std::string* error) {
    auto& a = il2cpp::api();
    auto* achievement = a.gchandle_get_target(entry->achievementRoot);
    if (!achievement) { *error = "Native achievement root unavailable"; return false; }
    if (!entry->managerRegistered) {
        void* args[]{achievement};
        if (!invoke(methods.registerAchievement, methods.manager, args, nullptr, error))
            return false;
        entry->managerRegistered = true;
    }
    if (!entry->iconRegistered) {
        auto* name = a.string_new(definition.name.c_str());
        if (!name) { *error = "RegisterIconIndex name allocation failed"; return false; }
        int index = definition.index;
        void* args[]{name, &index};
        if (!invoke(methods.registerIcon, methods.manager, args, nullptr, error))
            return false;
        entry->iconRegistered = true;
    }
    auto* name = a.string_new(definition.name.c_str());
    if (!name) { *error = "RegisterAchievementCategory name allocation failed"; return false; }
    int category = definition.category;
    void* args[]{name, &category};
    if (!invoke(methods.registerCategory, methods.manager, args, nullptr, error))
        return false;
    entry->registered = true;
    if (auto* target = a.gchandle_get_target(entry->achievementRoot))
        iconsByAchievement[target] = entry->assetRoot;
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
        if (it != entries.end()) {
            if (!it->second.registered && !commit(definition, &it->second, error)) {
                lastError = *error;
                return false;
            }
            continue;
        }
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

Il2CppObject* nativeAchievementIconAsset(const Il2CppObject* achievement) {
    if (!achievement) return nullptr;
    const auto it = iconsByAchievement.find(achievement);
    if (it == iconsByAchievement.end()) return nullptr;
    return il2cpp::api().gchandle_get_target(it->second);
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
