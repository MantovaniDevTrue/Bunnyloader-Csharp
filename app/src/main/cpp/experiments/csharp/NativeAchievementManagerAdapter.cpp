#include "experiments/csharp/NativeAchievementManagerAdapter.h"

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace bl::csharp {
namespace {

bool call(const MethodInfo* method, Il2CppObject* owner,
          void** args, Il2CppObject** result, std::string* error) {
    const auto& a = il2cpp::api();
    if (!method || !owner || !a.runtime_invoke) {
        *error = "Achievement manager dictionary invocation unavailable";
        return false;
    }
    Il2CppObject* exception = nullptr;
    Il2CppObject* returned = a.runtime_invoke(method, owner, args, &exception);
    if (exception) {
        *error = "Achievement manager dictionary operation threw";
        return false;
    }
    if (result) *result = returned;
    return true;
}

bool unboxBool(Il2CppObject* result, bool* value) {
    if (!result || !value) return false;
    std::uint8_t byte = 0;
    std::memcpy(&byte,
        reinterpret_cast<const char*>(result) + sizeof(Il2CppObject), 1);
    *value = byte != 0;
    return true;
}

}

bool NativeAchievementManagerAdapter::prepare(
    Il2CppObject* manager, std::string* error) {
    if (!error) return false;
    error->clear();
    dictionary_ = nullptr;
    lookup_ = nullptr;
    remove_ = nullptr;
    const auto& a = il2cpp::api();
    if (!manager || !a.object_get_class || !a.field_get_type ||
        !a.class_from_il2cpp_type || !a.type_get_name ||
        !a.il2cpp_free || !a.method_is_instance || !a.runtime_invoke) {
        *error = "Achievement manager reflection API unavailable";
        return false;
    }
    auto* managerClass = a.object_get_class(manager);
    if (!managerClass) {
        *error = "Achievement manager runtime type missing";
        return false;
    }
    // Verified original Terraria mobile contract is required at runtime;
    // a different dictionary/private field must never be guessed.
    FieldInfo* field = il2cpp::findField(managerClass, "_achievements");
    if (!field || !a.field_get_flags || (a.field_get_flags(field) & 0x10u)) {
        *error = "AchievementManager._achievements unavailable";
        return false;
    }
    auto* declared = a.field_get_type(field);
    char* raw = declared ? a.type_get_name(declared) : nullptr;
    const std::string fieldType = raw ? raw : "";
    if (raw) a.il2cpp_free(raw);
    if (fieldType.find("System.Collections.Generic.Dictionary") == std::string::npos ||
        fieldType.find("System.String") == std::string::npos ||
        fieldType.find("Terraria.Achievements.Achievement") == std::string::npos) {
        *error = "AchievementManager._achievements is not Dictionary<string, Achievement>";
        return false;
    }
    auto* dictionaryClass = a.class_from_il2cpp_type(declared);
    if (!dictionaryClass) {
        *error = "Achievement dictionary runtime type missing";
        return false;
    }
    const auto* lookup = il2cpp::findMethodBySignature(
        dictionaryClass,
        il2cpp::parseSignature(
            "bool TryGetValue(string key, out Achievement value)"));
    const auto* remove = il2cpp::findMethodBySignature(
        dictionaryClass,
        il2cpp::parseSignature("bool Remove(string key)"));
    if (!lookup || !remove || !a.method_is_instance(lookup) ||
        !a.method_is_instance(remove)) {
        *error = "Achievement dictionary typed lookup/removal unavailable";
        return false;
    }
    const auto offset = il2cpp::fieldOffset(managerClass, "_achievements");
    if (offset < static_cast<int>(sizeof(Il2CppObject))) {
        *error = "Achievement dictionary field offset invalid";
        return false;
    }
    Il2CppObject* dictionary = nullptr;
    std::memcpy(&dictionary,
        reinterpret_cast<const char*>(manager) + offset,
        sizeof(dictionary));
    if (!dictionary ||
        !a.class_is_assignable_from ||
        !a.class_is_assignable_from(dictionaryClass, a.object_get_class(dictionary))) {
        *error = "Achievement manager dictionary instance type mismatch";
        return false;
    }
    dictionary_ = dictionary;
    lookup_ = lookup;
    remove_ = remove;
    return true;
}

bool NativeAchievementManagerAdapter::containsExact(
    const std::string& name, Il2CppObject* expected,
    std::string* error) const {
    if (!error) return false;
    error->clear();
    if (!dictionary_ || !expected || name.empty() || name.size() > 256) {
        *error = "Achievement dictionary binding unavailable";
        return false;
    }
    const auto& a = il2cpp::api();
    if (!a.string_new) {
        *error = "Achievement dictionary string allocation unavailable";
        return false;
    }
    auto* key = a.string_new(name.c_str());
    if (!key) {
        *error = "Achievement name allocation failed";
        return false;
    }
    Il2CppObject* actual = nullptr;
    void* args[]{key, &actual};
    Il2CppObject* value = nullptr;
    bool found = false;
    if (!call(lookup_, dictionary_, args, &value, error) ||
        !unboxBool(value, &found)) {
        if (error->empty()) *error = "Achievement dictionary lookup returned invalid bool";
        return false;
    }
    if (!found || actual != expected) {
        *error = "Achievement dictionary entry changed or belongs to another mod";
        return false;
    }
    return true;
}

bool NativeAchievementManagerAdapter::removeExact(
    const std::string& name, Il2CppObject* expected,
    std::string* error) {
    if (!containsExact(name, expected, error)) return false;
    auto* key = il2cpp::api().string_new(name.c_str());
    if (!key) {
        *error = "Achievement removal key allocation failed";
        return false;
    }
    void* args[]{key};
    Il2CppObject* result = nullptr;
    bool removed = false;
    if (!call(remove_, dictionary_, args, &result, error) ||
        !unboxBool(result, &removed) || !removed) {
        if (error->empty()) *error = "Achievement dictionary Remove returned false";
        return false;
    }
    return true;
}

}
