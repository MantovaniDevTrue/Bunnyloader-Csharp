// Host regression: run the production prefix hooks against an IL2CPP fixture
// whose static field reads do NOT execute class constructors (the real ABI).
// This is not a device/Unity integration test.
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#ifndef PREFIX_GAMEPLAY_SOURCE
#define PREFIX_GAMEPLAY_SOURCE "../app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp"
#endif
#include PREFIX_GAMEPLAY_SOURCE

struct Il2CppClass { bool initialized = false; };
struct FieldInfo { Il2CppClass* parent; Il2CppArray* value = nullptr; size_t stride; };
struct MethodInfo { int unused = 0; };

namespace {
std::vector<void*> allocations;
std::map<Il2CppArray*, size_t> strides;
Il2CppClass lang, sets, integers;
FieldInfo names{&lang, nullptr, sizeof(void*)}, chances{&sets, nullptr, 1};
MethodInfo prefixMethod, rollableMethod, nextDoubleMethod;
Il2CppObject localized{}, randomObject{};
std::vector<bl::csharp::ManagedPrefixEntry> descriptors;
struct Item {
    Il2CppObject object{};
    int type = 6199, damage = 12, useAnimation = 20, mana = 0, ammo = 0;
    float knockBack = 2;
    uint8_t accessory = 0, consumable = 0, ranged = 1, prefix = 0;
};
struct Player { Il2CppObject object{}; int defense = 10; };
struct Box { Il2CppObject object{}; uint8_t value = 0; } boxed;
int constructorRuns = 0, nativeRolls = 0;

Il2CppArray* allocate(size_t count, size_t stride) {
    auto* p = static_cast<Il2CppArray*>(std::calloc(1, sizeof(Il2CppArray) + count * stride));
    assert(p);
    allocations.push_back(p);
    p->length = count;
    strides[p] = stride;
    return p;
}
void initialize(Il2CppClass* cls) {
    if (cls->initialized) return;
    cls->initialized = true;
    ++constructorRuns;
    for (auto* field : {&names, &chances})
        if (field->parent == cls) field->value = allocate(98, field->stride);
}
void getStatic(FieldInfo* f, void* out) {
    // Storage exists, .cctor has not necessarily run.
    *static_cast<Il2CppArray**>(out) = f->value;
}
void setStatic(FieldInfo* f, void* value) { f->value = static_cast<Il2CppArray*>(value); }
Il2CppArray* vanillaRollable(Il2CppObject* object, const MethodInfo*) {
    if (!reinterpret_cast<Item*>(object)->accessory) return nullptr;
    auto* a = allocate(1, sizeof(int32_t));
    *static_cast<int32_t*>(arrayData(a)) = 71;
    return a;
}
bool vanillaRoll(Il2CppObject*, Il2CppObject*, int32_t* result, const MethodInfo*) {
    ++nativeRolls;
    *result = 71;
    return true;
}
Il2CppObject* invoke(const MethodInfo* method, void* self, void** args, Il2CppObject** error) {
    using namespace bl::csharp;
    *error = nullptr;
    if (method == &rollableMethod)
        return reinterpret_cast<Il2CppObject*>(hookRollable(static_cast<Il2CppObject*>(self), method));
    assert(method == &prefixMethod);
    auto* item = static_cast<Item*>(self);
    assert(*static_cast<int32_t*>(args[0]) == -2);
    *static_cast<uint8_t*>(args[1]) = 0;
    int32_t selected = -1;
    boxed.value = hookRoll(&item->object, &randomObject, &selected, nullptr);
    if (boxed.value) item->prefix = static_cast<uint8_t>(selected);
    return &boxed.object;
}
}

namespace bl::log { void write(int, const char*, ...) {} }
namespace bl::il2cpp { Api& api() { static Api value{}; return value; } }
namespace bl::runtime {
bool isModItem(int type) { return type >= 6196; }
int vanillaPrefixCount() { return 98; }
std::string modPrefixKey(int type) {
    return type == 98 ? "fixture/Guarded" : type == 99 ? "fixture/Mighty" : "";
}
Il2CppArray* TypeTables::resizedCopy(Il2CppArray* old, uintptr_t size) {
    const size_t stride = strides.at(old);
    auto* out = allocate(size, stride);
    std::memcpy(arrayData(out), arrayData(old), std::min(size, old->length) * stride);
    return out;
}
namespace content {
Il2CppObject* makeLocalizedText(const std::string&, const std::string&) { return &localized; }
bool setTableElement(FieldInfo* f, int index, Il2CppObject* value) {
    if (static_cast<uintptr_t>(index) >= f->value->length) return false;
    static_cast<Il2CppObject**>(arrayData(f->value))[index] = value;
    return true;
}
}
}
namespace bl::csharp {
std::vector<ManagedPrefixEntry> snapshotManagedPrefixes() { return descriptors; }
int managedNativeAccessoryDefense(int type) { return type == 6196 ? 7 : 0; }
int managedNativeAccessoryMoveSpeedPercent(int type) { return type == 6200 ? 8 : 0; }
}

int main() {
    using namespace bl::csharp;
    auto& api = bl::il2cpp::api();
    api.field_get_parent = [](FieldInfo* f) { return f->parent; };
    api.runtime_class_init = initialize;
    api.field_static_get_value = getStatic;
    api.field_static_set_value = setStatic;
    api.array_new = [](Il2CppClass*, uintptr_t count) { return allocate(count, sizeof(int32_t)); };
    api.runtime_invoke = invoke;
    g_refs.names = &names;
    g_refs.chances = &chances;
    g_refs.intClass = &integers;
    g_refs.rollableMethod = &rollableMethod;
    g_refs.nextDoubleMethod = &nextDoubleMethod;
#define OFFSET(member) g_refs.member = offsetof(Item, member)
    OFFSET(type); OFFSET(damage); OFFSET(useAnimation); OFFSET(mana);
    OFFSET(ammo); OFFSET(knockBack); OFFSET(accessory); OFFSET(consumable);
    OFFSET(ranged); OFFSET(prefix);
#undef OFFSET
    g_refs.playerDefense = offsetof(Player, defense);
    g_originalRollable = vanillaRollable;
    g_originalRoll = vanillaRoll;
    g_hooksReady = true;
    ManagedPrefixEntry guarded;
    guarded.name = "Guarded"; guarded.type = 98; guarded.category = 5;
    guarded.rollWeight = 1; guarded.accessoryDefense = 4;
    ManagedPrefixEntry mighty;
    mighty.name = "Mighty"; mighty.type = 99; mighty.category = 4;
    mighty.rollWeight = 1; mighty.multipliers[0] = 1.2f;
    descriptors = {guarded, mighty};

    // Regression: first prefix access, before ANY vanilla natural roll.
    assert(!names.value && !chances.value);
    Item weapon;
    auto* candidates = hookRollable(&weapon.object, nullptr);
    assert(candidates && candidates->length == 1);
    assert(*static_cast<int32_t*>(arrayData(candidates)) == 99);
    assert(constructorRuns == 2 && names.value->length == 100 && chances.value->length == 100);
    // Warm calls must not rerun constructors or lose installed names.
    assert(ensurePrefixTables(descriptors));
    assert(constructorRuns == 2);
    assert(static_cast<Il2CppObject**>(arrayData(names.value))[99] == &localized);

    for (int damage : {12, 18}) {
        weapon.damage = damage; weapon.prefix = 0;
        bool reached = false;
        assert(applyManagedPrefixViaNativeRoll(&weapon.object, 99, &prefixMethod, &reached));
        assert(reached && weapon.prefix == 99 && nativeRolls == 0 && !g_restoreRoll.item);
        float dmg=1, kb=1, spd=1, size=1, shoot=1, mana=1, value=1;
        int crit=0, tag=0, pen=0;
        assert(hookStats(&weapon.object, 99, &dmg, &kb, &spd, &size, &shoot,
                         &mana, &crit, &tag, &pen, &value, nullptr));
        assert(static_cast<int>(std::nearbyint(damage*dmg)) == (damage == 12 ? 14 : 22));
    }
    Item accessory;
    accessory.type=6196; accessory.damage=-1; accessory.accessory=1;
    bool reached=false;
    assert(applyManagedPrefixViaNativeRoll(&accessory.object, 98, &prefixMethod, &reached));
    assert(reached && accessory.prefix==98 && nativeRolls==0);
    Player player;
    hookGrant(&player.object, &accessory.object, nullptr);
    assert(player.defense==14);
    assert(!applyManagedPrefixViaNativeRoll(&accessory.object, 99, &prefixMethod, &reached));
    assert(!reached && accessory.prefix==98);

    api.string_new = [](const char* text) -> Il2CppString* {
        const size_t length = std::strlen(text);
        void* memory = std::calloc(1, sizeof(Il2CppString) + (length + 1) * sizeof(char16_t));
        allocations.push_back(memory);
        auto* result = static_cast<Il2CppString*>(memory);
        result->length = static_cast<int32_t>(length);
        for (size_t i = 0; i < length; ++i)
            result->chars[i] = static_cast<unsigned char>(text[i]);
        return result;
    };
    api.gc_wbarrier_set_field = [](Il2CppObject*, void** field, void* value) {
        *field = value;
    };
    Il2CppArray* lines = allocate(6, sizeof(Il2CppString*));
    Il2CppArray* modifier = allocate(6, 1);
    Il2CppArray* bad = allocate(6, 1);
    int32_t lineCount = 0;
    g_inventoryTooltipReady = true;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 2);
    auto** text = static_cast<Il2CppString**>(arrayData(lines));
    assert(text[0] && text[1] && text[0]->length >= 2 && text[1]->length >= 2);
    assert(text[0]->chars[0] == '+' && text[0]->chars[1] == '7');
    assert(text[1]->chars[0] == '+' && text[1]->chars[1] == '4');
    assert(static_cast<uint8_t*>(arrayData(modifier))[0] == 0);
    assert(static_cast<uint8_t*>(arrayData(modifier))[1] == 1);
    accessory.prefix = 0;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 1);
    accessory.type = 6199;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 0);
    accessory.type = 6200;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 1);
    text = static_cast<Il2CppString**>(arrayData(lines));
    assert(text[0] && text[0]->chars[0] == '+' && text[0]->chars[1] == '8');
    assert(static_cast<uint8_t*>(arrayData(modifier))[0] == 0);

    // Language/table regeneration must be handled even after the first success.
    names.value=allocate(98,sizeof(void*));
    chances.value=allocate(98,1);
    assert(ensurePrefixTables(descriptors));
    assert(names.value->length==100 && chances.value->length==100 && constructorRuns==2);
    // Genuine unavailability is not turned into a fake successful restoration.
    auto saved=api.runtime_class_init;
    api.runtime_class_init=nullptr;
    assert(!ensurePrefixTables(descriptors));
    api.runtime_class_init=saved;
    for(void* p : allocations) std::free(p);
    std::cout << "cold tables, warm tables, prefix restoration and two inventory tooltip lines: PASS\n";
}
