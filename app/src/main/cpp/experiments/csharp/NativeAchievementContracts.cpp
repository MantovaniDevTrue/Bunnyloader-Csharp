#include "experiments/csharp/NativeAchievementContracts.h"

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <cstring>
#include <initializer_list>
#include <string>

namespace bl::csharp::achievement {
namespace {

bool typeIs(const Il2CppType* type, const char* expected) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* actual = a.type_get_name(type);
    bool valid = actual && std::strcmp(actual, expected) == 0;
    if (actual) a.il2cpp_free(actual);
    return valid;
}

const MethodInfo* method(Il2CppClass* cls, const char* name, const char* ret,
                         bool instance, std::initializer_list<const char*> parameters) {
    auto& a = il2cpp::api();
    if (!cls || !a.class_get_methods || !a.method_get_name || !a.method_is_instance ||
        !a.method_get_param_count || !a.method_get_param ||
        !a.method_get_return_type) return nullptr;
    void* cursor = nullptr;
    const MethodInfo* matched = nullptr;
    while (const MethodInfo* candidate = a.class_get_methods(cls, &cursor)) {
        const char* candidateName = a.method_get_name(candidate);
        if (!candidateName || std::strcmp(name, candidateName) != 0 ||
            a.method_is_instance(candidate) != instance ||
            a.method_get_param_count(candidate) != parameters.size() ||
            !typeIs(a.method_get_return_type(candidate), ret))
            continue;
        uint32_t index = 0;
        bool valid = true;
        for (const char* expected : parameters)
            if (!typeIs(a.method_get_param(candidate, index++), expected)) {
                valid = false;
                break;
            }
        if (!valid) continue;
        if (matched) return nullptr;
        matched = candidate;
    }
    return matched;
}

bool referenceField(Il2CppClass* cls, const char* name, const char* type, bool isStatic) {
    auto& a = il2cpp::api();
    if (!cls || !a.class_get_field_from_name || !a.field_get_flags || !a.field_get_type)
        return false;
    FieldInfo* field = a.class_get_field_from_name(cls, name);
    return field && !!(a.field_get_flags(field) & 0x10u) == isStatic &&
           typeIs(a.field_get_type(field), type);
}

bool conditionContract(int kind) {
    auto cls = [&](const char* name) {
        return il2cpp::findClassQuiet("Terraria.GameContent.Achievements", name);
    };
    switch (kind) {
        case 0:
            return method(cls("CustomFlagCondition"), "Create",
                "Terraria.Achievements.AchievementCondition", false, {"System.String"});
        case 1:
            return method(cls("CustomIntCondition"), "Create",
                "Terraria.Achievements.AchievementCondition", false,
                {"System.String", "System.Int32"});
        case 2:
            return method(cls("CustomFloatCondition"), "Create",
                "Terraria.Achievements.AchievementCondition", false,
                {"System.String", "System.Single"});
        case 3:
        case 4:
        case 5:
        case 6:
            return false;
    }
    return false;
}

}

ManagerContractResult inspectManagerContracts(const std::vector<Descriptor>& descriptors) {
    ManagerContractResult output;
    auto& a = il2cpp::api();
    if (!a.gameImage || !a.runtime_invoke || !a.object_new || !a.string_new ||
        !a.field_static_get_value || !a.field_get_flags || !a.field_get_type ||
        !a.field_get_offset || !a.gc_wbarrier_set_field) {
        output.diagnostic = "IL2CPPManagerExportsMissing";
        return output;
    }
    Il2CppClass* main = il2cpp::findClassQuiet("Terraria", "Main");
    Il2CppClass* manager = il2cpp::findClassQuiet("Terraria.Achievements", "AchievementManager");
    Il2CppClass* achievement = il2cpp::findClassQuiet("Terraria.Achievements", "Achievement");
    Il2CppClass* language = il2cpp::findClassQuiet("Terraria.Localization", "LocalizedText");
    if (!main || !manager || !achievement || !language ||
        !referenceField(main, "Achievements", "Terraria.Achievements.AchievementManager", true)) {
        output.diagnostic = "ManagerOrAchievementTypeUnavailable";
        return output;
    }
    auto* achievementField = a.class_get_field_from_name(main, "Achievements");
    Il2CppObject* object = nullptr;
    a.field_static_get_value(achievementField, &object);
    if (!object) {
        output.diagnostic = "ManagerNotInitialized";
        return output;
    }
    output.available = true;

    if (!method(achievement, ".ctor", "System.Void", true, {"System.String"}) ||
        !method(achievement, "AddCondition", "System.Void", true,
                {"Terraria.Achievements.AchievementCondition"}) ||
        !method(language, ".ctor", "System.Void", true,
                {"System.String", "System.String"}) ||
        !referenceField(achievement, "FriendlyName",
                        "Terraria.Localization.LocalizedText", false) ||
        !referenceField(achievement, "Description",
                        "Terraria.Localization.LocalizedText", false)) {
        output.diagnostic = "AchievementConstructionContractMismatch";
        return output;
    }

    for (const Descriptor& achievementDescriptor : descriptors) {
        for (const ConditionDescriptor& condition : achievementDescriptor.conditions) {
            if (!conditionContract(condition.kind)) {
                output.diagnostic = "AchievementConditionContractUnavailable";
                return output;
            }
        }
    }
    output.canConstruct = true;

    if (!method(manager, "GetAchievement", "Terraria.Achievements.Achievement", true,
                {"System.String"}) ||
        !method(manager, "Register", "System.Void", true,
                {"Terraria.Achievements.Achievement"}) ||
        !method(manager, "RegisterIconIndex", "System.Void", true,
                {"System.String", "System.Int32"}) ||
        !method(manager, "RegisterAchievementCategory", "System.Void", true,
                {"System.String", "Terraria.Achievements.AchievementCategory"})) {
        output.diagnostic = "AchievementRegistrationContractMismatch";
        return output;
    }
    output.canRegister = true;

    if (method(manager, "Load", "System.Void", true,
                {"System.String", "System.Boolean"}) &&
        referenceField(manager, "_savePath", "System.String", false) &&
        referenceField(manager, "_isCloudSave", "System.Boolean", false) &&
        referenceField(achievement, "_completedCount", "System.Int32", false))
        output.canReload = true;

    if (method(achievement, "UseConditionsCompletedTracker", "System.Void", true, {}) &&
        method(achievement, "UseTrackerFromCondition", "System.Void", true,
               {"System.String"}))
        output.canPresent = true;

    output.diagnostic = output.canReload && output.canPresent
        ? "ContractsResolved" : "RegistrationResolvedPresentationOrReloadPending";
    return output;
}

}
