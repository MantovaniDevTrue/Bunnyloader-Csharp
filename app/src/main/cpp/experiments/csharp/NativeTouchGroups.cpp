#include "experiments/csharp/NativeTouchGroups.h"
#include "experiments/csharp/NativeTouchProfiles.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::touch {
namespace {

struct Group {
    uint32_t slotRoot = 0;
    std::string slotId;
    std::array<int32_t, 16> actions{};
    int count = 0;
    int before = -1;
};
std::array<Group, 16> groups;
int count = 0;
uint64_t frames = 0;
const void* cachedProfile = nullptr;
uint64_t discoveryAttempts = 0;
bool available = false;

size_t statesOffset = 0;
const MethodInfo* containsMethod = nullptr;
const MethodInfo* itemMethod = nullptr;

void clear() {
    auto& api = il2cpp::api();
    for (auto& g : groups) {
        if (g.slotRoot && api.gchandle_free) api.gchandle_free(g.slotRoot);
        g = {};
    }
    count = 0;
}
bool discover(void* controller) {
    auto& api = il2cpp::api();
    if (!controller || !api.object_get_class || !api.field_get_offset ||
        !api.class_instance_size || !api.field_get_flags ||
        !api.field_get_type || !api.class_from_il2cpp_type ||
        !api.class_is_valuetype || !api.class_get_method_from_name ||
        !api.method_get_param_count || !api.method_is_instance ||
        !api.runtime_invoke || !api.gchandle_new ||
        !api.gchandle_free || !api.gchandle_get_target || !api.string_new)
        return false;
    Il2CppClass* cls = api.object_get_class(
        reinterpret_cast<Il2CppObject*>(controller));
    FieldInfo* field = il2cpp::findField(cls, "GroupStates");
    if (!field) field = il2cpp::findField(cls, "<GroupStates>k__BackingField");
    if (!field || (api.field_get_flags(field) & 0x10u))
        return false;
    Il2CppClass* fieldType =
        api.class_from_il2cpp_type(api.field_get_type(field));
    if (!fieldType || api.class_is_valuetype(fieldType)) return false;
    const size_t off = api.field_get_offset(field);
    if (off < sizeof(Il2CppObject) ||
        off + sizeof(void*) > api.class_instance_size(cls))
        return false;
    statesOffset = off;
    containsMethod = api.class_get_method_from_name(fieldType, "ContainsKey", 1);
    itemMethod = api.class_get_method_from_name(fieldType, "get_Item", 1);
    return containsMethod && itemMethod &&
           api.method_is_instance(containsMethod) &&
           api.method_is_instance(itemMethod) &&
           api.method_get_param_count(containsMethod) == 1 &&
           api.method_get_param_count(itemMethod) == 1;
}
int indexOf(void* controller, const Group& group) {
    auto& api = il2cpp::api();
    if (!group.slotRoot || !api.object_get_class ||
        !api.field_get_offset || !api.class_instance_size) return -1;
    Il2CppObject* table = nullptr;
    std::memcpy(&table, static_cast<const char*>(controller) + statesOffset,
                sizeof(table));
    if (!table) return -1;
    Il2CppObject* key = api.gchandle_get_target(group.slotRoot);
    if (!key) return -1;
    void* args[1] = {&key};
    Il2CppObject* ex = nullptr;
    Il2CppObject* contains = api.runtime_invoke(containsMethod, table, args, &ex);
    if (ex || !contains || !api.object_get_class) return -1;
    Il2CppClass* cls = api.object_get_class(contains);
    if (!api.class_get_name || std::strcmp(api.class_get_name(cls), "Boolean") != 0)
        return -1;
    bool found = *reinterpret_cast<uint8_t*>(
        reinterpret_cast<char*>(contains) + sizeof(Il2CppObject)) != 0;
    if (!found) return -1;
    Il2CppObject* state = api.runtime_invoke(itemMethod, table, args, &ex);
    if (ex || !state) return -1;
    cls = api.object_get_class(state);
    FieldInfo* field = cls ? il2cpp::findField(cls, "pressedIndex") : nullptr;
    if (!field) field = cls ? il2cpp::findField(cls, "<pressedIndex>k__BackingField") : nullptr;
    if (!field || !api.field_get_flags ||
        (api.field_get_flags(field) & 0x10u)) return -1;
    const size_t position = api.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + sizeof(int32_t) > api.class_instance_size(cls))
        return -1;
    int32_t index = -1;
    std::memcpy(&index, reinterpret_cast<char*>(state) + position,
                sizeof(index));
    return index;
}
void refresh() {
    const int activeSlots = nativeTouchActiveSlotCount();
    if (activeSlots < 0) {
        clear();
        return;
    }
    std::array<Group, 16> pending{};
    int pendingCount = 0;
    for (int slot = 0; slot < activeSlots && pendingCount < 16; ++slot) {
        SlotAction button;
        if (!nativeTouchSlot(slot, &button) || button.style != 2 ||
            button.groupCount < 1) continue;
        bool containsMod = false;
        for (int i = 0; i < button.groupCount; ++i)
            if (nativeTouchIndexByAction(button.group[i]) >= 0)
                containsMod = true;
        if (!containsMod) continue;
        std::string hardwareId;
        if (!nativeTouchSlotId(slot, &hardwareId)) continue;
        Group& group = pending[static_cast<size_t>(pendingCount++)];
        group.slotId = std::move(hardwareId);
        group.count = button.groupCount;
        for (int i = 0; i < button.groupCount; ++i)
            group.actions[static_cast<size_t>(i)] = button.group[static_cast<size_t>(i)];
    }
    bool changed = pendingCount != count;
    for (int i = 0; !changed && i < count; ++i) {
        const Group& previous = groups[static_cast<size_t>(i)];
        const Group& next = pending[static_cast<size_t>(i)];
        changed = previous.slotId != next.slotId ||
                  previous.count != next.count ||
                  previous.actions != next.actions;
    }
    if (!changed) return;
    clear();
    auto& api = il2cpp::api();
    for (int i = 0; i < pendingCount; ++i) {
        Group& next = pending[static_cast<size_t>(i)];
        Il2CppString* name = api.string_new(next.slotId.c_str());
        if (!name) continue;
        const uint32_t handle = api.gchandle_new(
            reinterpret_cast<Il2CppObject*>(name), false);
        if (!handle) continue;
        next.slotRoot = handle;
        groups[static_cast<size_t>(count++)] = std::move(next);
    }
}
}

void nativeTouchGroupBefore(void* controller) {
    if (!controller || registry().count() == 0) return;
    if (!available && (++discoveryAttempts == 1 ||
                       discoveryAttempts % 120 == 0))
        available = discover(controller);
    if (!available) return;
    const void* profile = nativeTouchActiveProfileIdentity();
    if (++frames == 1 || frames % 30 == 0 || cachedProfile != profile) {
        refresh();
        cachedProfile = profile;
    }
    if (!hostReady()) return;
    for (int i = 0; i < count; ++i)
        groups[static_cast<size_t>(i)].before =
            indexOf(controller, groups[static_cast<size_t>(i)]);
}
void nativeTouchGroupAfter(void* controller) {
    if (!controller || !available || !hostReady()) return;
    for (int i = 0; i < count; ++i) {
        auto& group = groups[static_cast<size_t>(i)];
        const int was = group.before;
        // O centro virtual de grupos (9) fecha o menu sem selecionar.
        if (was < 0 || was == 9 || was >= group.count ||
            indexOf(controller, group) != -1) continue;
        const int id = nativeTouchIndexByAction(group.actions[static_cast<size_t>(was)]);
        if (id >= 0 && !registry().ownerId(id).empty()) registry().tap(id);
    }
}
void resetNativeTouchGroupSession() {
    clear();
    frames = 0;
    cachedProfile = nullptr;
    discoveryAttempts = 0;
    available = false;
}
bool nativeTouchGroupBridgeReady() { return available; }

} // namespace bl::csharp::touch
