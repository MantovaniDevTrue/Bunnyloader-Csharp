#include "experiments/csharp/NativeHairUi.h"

#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::hair {
namespace {

using namespace bl::il2cpp;
using Clock = std::chrono::steady_clock;
using WarrantsFn = bool (*)(Il2CppObject*, const MethodInfo*);
using RebuildFn = void (*)(Il2CppObject*, const MethodInfo*);

struct Refs {
    Il2CppClass* helper = nullptr;
    int creationOffset = -1;
    int stylistOffset = -1;
    int availableOffset = -1;
    const MethodInfo* getter = nullptr;
    bool warrantsHooked = false;
    bool rebuildHooked = false;
};

Refs refs;
WarrantsFn previousWarrants = nullptr;
RebuildFn previousRebuild = nullptr;
const Registry* activeRegistry = nullptr;
UnlockQuery activeQuery = nullptr;
void* activeContext = nullptr;
bool enabled = false;
Il2CppObject* recentHelper = nullptr;
Clock::time_point refreshedAt{};
UnlockState unlocked;
HairUiStatus status;

bool booleanField(Il2CppClass* type, const char* fieldName, int* offset) {
    auto& apiRef = api();
    if (!type || !apiRef.class_instance_size || !apiRef.field_get_flags ||
        !apiRef.class_from_il2cpp_type || !apiRef.field_get_type ||
        !apiRef.class_get_name || !apiRef.class_get_namespace) return false;
    FieldInfo* field = findField(type, fieldName);
    if (!field || (apiRef.field_get_flags(field) & 0x10u) != 0) return false;
    auto* fieldType = apiRef.class_from_il2cpp_type(apiRef.field_get_type(field));
    if (!fieldType || !apiRef.class_is_valuetype(fieldType) ||
        std::strcmp(apiRef.class_get_name(fieldType), "Boolean") != 0 ||
        std::strcmp(apiRef.class_get_namespace(fieldType), "System") != 0) return false;
    const int place = fieldOffset(type, fieldName);
    const uint32_t size = apiRef.class_instance_size(type);
    if (place < static_cast<int>(sizeof(Il2CppObject)) ||
        static_cast<uint32_t>(place) >= size) return false;
    *offset = place;
    return true;
}

bool contexts(Il2CppObject* helper, bool* creation, bool* stylist) {
    if (!helper || !creation || !stylist || refs.creationOffset < 0 ||
        refs.stylistOffset < 0) return false;
    uint8_t atCreation = 0, atStylist = 0;
    const auto* bytes = reinterpret_cast<const uint8_t*>(helper);
    std::memcpy(&atCreation, bytes + refs.creationOffset, 1);
    std::memcpy(&atStylist, bytes + refs.stylistOffset, 1);
    if (atCreation > 1 || atStylist > 1 || (atCreation != 0 && atStylist != 0))
        return false;
    *creation = atCreation != 0;
    *stylist = atStylist != 0;
    return true;
}

Il2CppObject* availableList(Il2CppObject* helper) {
    auto& a = api();
    if (refs.getter) {
        Il2CppObject* exception = nullptr;
        Il2CppObject* result = a.runtime_invoke(refs.getter, helper, nullptr, &exception);
        return exception ? nullptr : result;
    }
    if (refs.availableOffset >= 0) {
        Il2CppObject* result = nullptr;
        std::memcpy(&result, reinterpret_cast<char*>(helper) + refs.availableOffset,
                    sizeof(result));
        return result;
    }
    return nullptr;
}

bool invokeInt(Il2CppObject* list, const char* methodName, int type, bool* value) {
    auto& a = api();
    if (!list || !a.object_get_class || !a.class_get_method_from_name ||
        !a.method_get_param_count || !a.method_get_param ||
        !a.class_from_il2cpp_type || !a.class_get_name || !a.class_get_namespace ||
        !a.runtime_invoke) return false;
    Il2CppClass* listType = a.object_get_class(list);
    const MethodInfo* method = listType
        ? a.class_get_method_from_name(listType, methodName, 1) : nullptr;
    if (!method || a.method_get_param_count(method) != 1) return false;
    Il2CppClass* parameter = a.class_from_il2cpp_type(a.method_get_param(method, 0));
    if (!parameter || std::strcmp(a.class_get_namespace(parameter), "System") != 0 ||
        std::strcmp(a.class_get_name(parameter), "Int32") != 0) return false;

    void* arguments[1] = {&type};
    Il2CppObject* exception = nullptr;
    Il2CppObject* result = a.runtime_invoke(method, list, arguments, &exception);
    if (exception) return false;
    if (value) {
        if (!result) return false;
        *value = *(reinterpret_cast<uint8_t*>(result) + sizeof(Il2CppObject)) != 0;
    }
    return true;
}

bool updateAvailable(Il2CppObject* helper, bool force) {
    if (!enabled || !activeRegistry) return false;
    bool creation = false, stylist = false;
    if (!contexts(helper, &creation, &stylist)) return false;
    if (recentHelper != helper) {
        recentHelper = helper;
        unlocked.Reset();
        force = true;
    }
    const auto now = Clock::now();
    if (!force && now - refreshedAt < std::chrono::milliseconds(250))
        return false;
    refreshedAt = now;
    const bool changed = unlocked.Refresh(*activeRegistry, creation, stylist,
                                          activeQuery, activeContext);
    status.visibleStyles = static_cast<int>(unlocked.AvailableTypes().size());
    return changed;
}

bool warrants(Il2CppObject* helper, const MethodInfo* method) {
    const bool vanilla = previousWarrants ? previousWarrants(helper, method) : false;
    if (!enabled || !helper) return vanilla;
    const bool changed = updateAvailable(helper, false);
    return vanilla || changed;
}

void rebuild(Il2CppObject* helper, const MethodInfo* method) {
    if (previousRebuild) previousRebuild(helper, method);
    if (!enabled || !helper || !activeRegistry) return;
    updateAvailable(helper, true);
    Il2CppObject* list = availableList(helper);
    if (!list) return;

    for (const int hairType : unlocked.AvailableTypes()) {
        bool contains = false;
        if (!invokeInt(list, "Contains", hairType, &contains)) return;
        if (!contains && !invokeInt(list, "Add", hairType, nullptr)) return;
    }
}

bool bindMetadata(std::string* error) {
    auto& a = api();
    if (!a.runtime_invoke || !a.object_get_class || !a.method_is_instance ||
        !a.class_instance_size || !a.class_get_method_from_name ||
        !a.class_from_il2cpp_type || !a.field_get_type ||
        !a.class_get_name || !a.class_get_namespace) {
        *error = "HairstyleIl2CppApiUnavailable";
        return false;
    }
    refs.helper = findClass({"Terraria.GameContent", "HairstyleUnlocksHelper", {}});
    if (!refs.helper ||
        !booleanField(refs.helper, "_isAtCharacterCreation", &refs.creationOffset) ||
        !booleanField(refs.helper, "_isAtStylist", &refs.stylistOffset)) {
        *error = "HairstyleHelperStateLayoutUnavailable";
        return false;
    }

    refs.getter = a.class_get_method_from_name(refs.helper, "get_AvailableHairstyles", 0);
    refs.availableOffset = -1;
    if (refs.getter && !a.method_is_instance(refs.getter)) refs.getter = nullptr;
    if (!refs.getter) {
        FieldInfo* field = findField(refs.helper, "AvailableHairstyles");
        if (field && a.field_get_flags && !(a.field_get_flags(field) & 0x10u)) {
            const int offset = fieldOffset(refs.helper, "AvailableHairstyles");
            const auto size = a.class_instance_size(refs.helper);
            if (offset >= static_cast<int>(sizeof(Il2CppObject)) &&
                static_cast<uint32_t>(offset) <= size &&
                sizeof(void*) <= size - static_cast<uint32_t>(offset)) {
                auto* member = a.class_from_il2cpp_type(a.field_get_type(field));
                if (member && !a.class_is_valuetype(member))
                    refs.availableOffset = offset;
            }
        }
    }
    if (!refs.getter && refs.availableOffset < 0) {
        *error = "AvailableHairstylesListUnavailable";
        return false;
    }
    return true;
}

} // namespace

HairUiStatus InstallNativeHairUi(const Registry* registry, UnlockQuery query,
                                 void* context, bool texturesInstalled) {
    if (!registry || !texturesInstalled || registry->Size() == 0)
        return {refs.warrantsHooked && refs.rebuildHooked, false, 0,
                "HairTexturesOrRegistryNotReady"};
    if (!bindMetadata(&status.error)) return status;
    auto& a = api();
    const MethodInfo* warrantsMethod = findMethodBySignature(
        refs.helper, parseSignature("bool ListWarrantsRemake()"));
    const MethodInfo* rebuildMethod = findMethodBySignature(
        refs.helper, parseSignature("void RebuildList()"));
    if (!warrantsMethod || !rebuildMethod || !a.method_is_instance(warrantsMethod) ||
        !a.method_is_instance(rebuildMethod)) {
        status.error = "HairstyleHookSignaturesUnavailable";
        return status;
    }
    if (!refs.warrantsHooked) {
        if (!bl::hook::install(warrantsMethod, warrants, &previousWarrants)) {
            status.error = "ListWarrantsRemakeHookFailed";
            return status;
        }
        refs.warrantsHooked = true;
    }
    if (!refs.rebuildHooked) {
        if (!bl::hook::install(rebuildMethod, rebuild, &previousRebuild)) {
            status.error = "RebuildListHookFailed";
            return status;
        }
        refs.rebuildHooked = true;
    }

    unlocked.Reset();
    recentHelper = nullptr;
    refreshedAt = {};
    activeRegistry = registry;
    activeQuery = query;
    activeContext = context;
    enabled = true;
    status = {true, true, 0, {}};
    return status;
}

void ResetNativeHairUi() {
    enabled = false;
    activeRegistry = nullptr;
    activeQuery = nullptr;
    activeContext = nullptr;
    unlocked.Reset();
    recentHelper = nullptr;
    status.contentEnabled = false;
    status.visibleStyles = 0;
}

} // namespace bl::csharp::hair
