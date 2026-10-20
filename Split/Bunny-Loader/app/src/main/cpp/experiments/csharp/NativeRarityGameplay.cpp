#include "experiments/csharp/NativeRarityGameplay.h"
#include "experiments/csharp/NativeRarityPalette.h"
#include "experiments/csharp/NativeRarityColorWire.h"
#include "experiments/csharp/NativeRarityTextColor.h"
#include "experiments/csharp/NativeStaticValue.h"
#include "il2cpp/ValueLayout.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "content/common/GameRefs.h"
#include <cmath>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

struct RaritySample {
    int type = -1;
    std::string name;
};

struct RarityMod {
    std::string id;
    std::string assembly;
    std::vector<RaritySample> rarities;
    size_t cursor = 0;
    bool ready = false;
    bool failed = false;
};

std::mutex g_mutex;
std::vector<RarityMod> g_mods;
NativeRarityPalette g_palette;
NativeColorLayout g_colorLayout;
std::atomic<bool> g_hookReady{false};
unsigned g_tick = 0;
size_t g_modCursor = 0;
using PopupColorFn = uint32_t (*)(int32_t, const MethodInfo*);
PopupColorFn g_originalPopup = nullptr;
using RarityInitFn = void (*)(const MethodInfo*);
RarityInitFn g_originalRarityInitialize = nullptr;
std::atomic<bool> g_tableHookReady{false};
std::array<bool, NativeRarityPalette::kMaximumCustom> g_tableOwned{};
Il2CppObject* g_lastRarityTable = nullptr;

bool parameterIs(Il2CppClass* cls, const MethodInfo* method, uint32_t index,
    const char* ns, const char* name) {
    auto& a = bl::il2cpp::api();
    if (!method || !a.method_get_param_count || !a.method_get_param ||
        !a.class_from_il2cpp_type || !a.class_get_namespace || !a.class_get_name ||
        index >= a.method_get_param_count(method)) return false;
    auto* type = a.class_from_il2cpp_type(a.method_get_param(method, index));
    const char* actualNs = type ? a.class_get_namespace(type) : nullptr;
    const char* actualName = type ? a.class_get_name(type) : nullptr;
    return type && (!cls || cls == type) && actualNs && actualName &&
        std::strcmp(actualNs, ns) == 0 && std::strcmp(actualName, name) == 0;
}

bool isReturn(const MethodInfo* method, const char* ns, const char* name) {
    auto& a = bl::il2cpp::api();
    if (!method || !a.method_get_return_type || !a.class_from_il2cpp_type ||
        !a.class_get_namespace || !a.class_get_name) return false;
    auto* type = a.class_from_il2cpp_type(a.method_get_return_type(method));
    const char* actualNs = type ? a.class_get_namespace(type) : nullptr;
    const char* actualName = type ? a.class_get_name(type) : nullptr;
    return actualNs && actualName && !std::strcmp(actualNs, ns) &&
        !std::strcmp(actualName, name);
}

struct RarityTable {
    FieldInfo* field = nullptr;
    const MethodInfo* contains = nullptr;
    const MethodInfo* set = nullptr;
    Il2CppClass* color = nullptr;
};
RarityTable g_table;

bool bindRarityTable(std::string* error) {
    using namespace bl::il2cpp;
    auto& a = api();
    auto* owner = findClass({"Terraria.GameContent.UI", "ItemRarity", {}});
    FieldInfo* field = owner ? findField(owner, "_rarities") : nullptr;
    if (!field || !a.field_get_flags || !(a.field_get_flags(field) & 0x10u) ||
        !a.field_static_get_value || !a.runtime_class_init ||
        !a.object_get_class || !a.class_get_method_from_name ||
        !a.method_is_instance || !a.runtime_invoke) {
        *error = "Reason=RarityTableUnavailable";
        return false;
    }
    a.runtime_class_init(owner);
    Il2CppObject* table = nullptr;
    a.field_static_get_value(field, &table);
    auto* tableClass = table ? a.object_get_class(table) : nullptr;
    if (!tableClass) {
        *error = "Reason=RarityTableNotInitialized";
        return false;
    }
    const auto* contains = a.class_get_method_from_name(tableClass, "ContainsKey", 1);
    const auto* set = a.class_get_method_from_name(tableClass, "set_Item", 2);
    if (!contains || !set || !a.method_is_instance(contains) || !a.method_is_instance(set) ||
        !parameterIs(nullptr, contains, 0, "System", "Int32") ||
        !parameterIs(nullptr, set, 0, "System", "Int32") ||
        !isReturn(contains, "System", "Boolean") ||
        !isReturn(set, "System", "Void") ||
        !a.class_from_il2cpp_type || !a.method_get_param) {
        *error = "Reason=RarityDictionaryMethodsMismatch";
        return false;
    }
    auto* color = a.class_from_il2cpp_type(a.method_get_param(set, 1));
    NativeColorLayout validated;
    if (!validated.bind(color, error)) return false;
    g_colorLayout = validated;
    g_table = {field, contains, set, color};
    return true;
}

bool writeRarityTable(int type, uint32_t argb) {
    auto& a = bl::il2cpp::api();
    if (type < NativeRarityPalette::kFirstCustom ||
        type >= NativeRarityPalette::kFirstCustom + NativeRarityPalette::kMaximumCustom ||
        !g_table.field || !g_table.contains || !g_table.set ||
        !a.field_static_get_value || !a.object_get_class || !a.runtime_invoke)
        return false;
    Il2CppObject* table = nullptr;
    a.field_static_get_value(g_table.field, &table);
    if (!table) return false;
    if (table != g_lastRarityTable) {
        g_tableOwned.fill(false);
        g_lastRarityTable = table;
    }
    const int slot = type - NativeRarityPalette::kFirstCustom;
    int32_t key = type;
    void* hasArgs[] = {&key};
    Il2CppObject* exception = nullptr;
    auto* exists = a.runtime_invoke(g_table.contains, table, hasArgs, &exception);
    if (exception || !exists) return false;
    bool present = *(reinterpret_cast<const uint8_t*>(exists) + sizeof(Il2CppObject)) != 0;
    if (present && !g_tableOwned[static_cast<size_t>(slot)]) return false;
    if (!present) g_tableOwned[static_cast<size_t>(slot)] = false;
    uint32_t value = g_colorLayout.fromArgb(argb);
    void* arguments[] = {&key, &value};
    exception = nullptr;
    a.runtime_invoke(g_table.set, table, arguments, &exception);
    if (exception) return false;
    g_tableOwned[static_cast<size_t>(slot)] = true;
    return true;
}

void initializeRarityTable(const MethodInfo* method) {
    g_tableOwned.fill(false);
    g_lastRarityTable = nullptr;
    if (g_originalRarityInitialize) g_originalRarityInitialize(method);
    if (!g_tableHookReady.load(std::memory_order_acquire)) return;
    const auto entries = nativeRarities().snapshot();
    for (const auto& entry : *entries) {
        if (!entry.managed) continue;
        uint32_t color = 0;
        if (g_palette.read(entry.type, &color))
            writeRarityTable(entry.type, color);
    }
}

bool installRarityTableHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_tableHookReady.load(std::memory_order_acquire)) return true;
    auto* owner = findClass({"Terraria.GameContent.UI", "ItemRarity", {}});
    const auto* initialize = owner ? findMethodBySignature(
        owner, parseSignature("void Initialize()")) : nullptr;
    auto& a = api();
    if (!initialize || !a.method_is_instance || a.method_is_instance(initialize) ||
        !a.method_get_param_count || a.method_get_param_count(initialize) != 0 ||
        !isReturn(initialize, "System", "Void") || !bindRarityTable(error)) {
        if (error->empty()) *error = "Reason=RarityInitializeSignatureMismatch";
        return false;
    }
    if (!bl::hook::install(initialize, initializeRarityTable, &g_originalRarityInitialize)) {
        *error = "Reason=RarityInitializeHookRejected";
        return false;
    }
    g_tableHookReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=ItemRarity.Initialize Result=Ready");
    return true;
}


using PrefixFn = bool (*)(Il2CppObject*, int32_t, bool*, const MethodInfo*);
using PrefixStatsFn = bool (*)(Il2CppObject*, int32_t, float*, float*, float*,
    float*, float*, float*, int32_t*, int32_t*, int32_t*, float*, const MethodInfo*);
PrefixFn g_originalPrefix = nullptr;
const MethodInfo* g_prefixStats = nullptr;
int g_rareOffset = -1;
int g_prefixOffset = -1;
std::atomic<bool> g_prefixReady{false};

float currentPrefixMultiplier(Il2CppObject* item) {
    if (!item || !g_prefixStats || g_prefixOffset < 0) return 1.f;
    const auto prefix = static_cast<int32_t>(bl::runtime::field<uint8_t>(item, g_prefixOffset));
    if (prefix == 0) return 1.f;
    auto* pointer = bl::il2cpp::methodPointer(g_prefixStats);
    if (!pointer) return 1.f;
    float dmg = 1.f, kb = 1.f, speed = 1.f, size = 1.f, shot = 1.f, mana = 1.f, value = 1.f;
    int32_t crit = 0, tag = 0, pen = 0;
    auto fn = reinterpret_cast<PrefixStatsFn>(pointer);
    if (!fn(item, prefix, &dmg, &kb, &speed, &size, &shot, &mana,
            &crit, &tag, &pen, &value, g_prefixStats) ||
        !std::isfinite(value) || value < 0.f || value > 100.f) return 1.f;
    return value;
}

bool prefixRarity(Il2CppObject* item, int32_t requested, bool* topTier, const MethodInfo* method) {
    if (!g_originalPrefix) return false;
    if (!item || !g_prefixReady.load(std::memory_order_acquire) ||
        g_rareOffset < 0 || requested == -3)
        return g_originalPrefix(item, requested, topTier, method);
    const int32_t base = bl::runtime::field<int32_t>(item, g_rareOffset);
    const auto identity = nativeRarities().identityForType(base);
    if (!identity.managed || identity.mod.empty())
        return g_originalPrefix(item, requested, topTier, method);
    const bool succeeded = g_originalPrefix(item, requested, topTier, method);
    if (!succeeded) {
        bl::runtime::field<int32_t>(item, g_rareOffset) = base;
        return false;
    }

    RarityMod owner;
    {
        std::unique_lock lock(g_mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            for (const auto& mod : g_mods)
                if (mod.id == identity.mod && mod.ready && !mod.failed) {
                    owner.id = mod.id;
                    owner.assembly = mod.assembly;
                    break;
                }
        }
    }
    const int32_t vanilla = bl::runtime::field<int32_t>(item, g_rareOffset);
    if (owner.id.empty()) {
        bl::runtime::field<int32_t>(item, g_rareOffset) = base;
        return true;
    }

    const float value = currentPrefixMultiplier(item);
    char number[48]{};
    const auto encoded = std::to_chars(number, number + sizeof(number), value);
    if (encoded.ec != std::errc{})
        bl::runtime::field<int32_t>(item, g_rareOffset) = base;
    else {
        std::string response;
        const std::string payload(number, encoded.ptr);
        const bool called = invokeManagedRarity(owner.id, owner.assembly, identity.name,
            1, payload + "," + std::to_string(vanilla), &response, 1800);
        int resultingType = base;
        if (called && decodeRarityPrefixWire(response, &resultingType) &&
            (resultingType < NativeRarityRegistry::kVanillaCount ||
             !nativeRarities().stableKeyForType(resultingType).empty()))
            bl::runtime::field<int32_t>(item, g_rareOffset) = resultingType;
        else
            bl::runtime::field<int32_t>(item, g_rareOffset) = base;
    }
    return true;
}

bool installPrefixRarityHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_prefixReady.load(std::memory_order_acquire)) return true;
    auto& a = api();
    auto* item = findClass({"Terraria", "Item", {}});
    const auto* prefix = item ? findMethodBySignature(item,
        parseSignature("bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)")) : nullptr;
    const auto* stats = item ? findMethodBySignature(item,
        parseSignature("bool TryGetPrefixStatMultipliersForItem(int rolledPrefix, out float dmg, out float kb, out float spd, out float size, out float shtspd, out float mcst, out int crt, out int tagdmg, out int arpen, out float value)")) : nullptr;
    if (!prefix || !stats || !a.method_is_instance || !a.method_is_instance(prefix) ||
        !a.method_is_instance(stats) || !methodPointer(stats)) {
        *error = "Reason=PrefixMethodSignatureUnavailable";
        return false;
    }
    const auto rareField = findField(item, "rare");
    const auto prefixField = findField(item, "prefix");
    auto validField = [&](FieldInfo* field, const char* typeName) {
        if (!field || !a.field_get_type || !a.field_get_flags || !a.class_from_il2cpp_type ||
            !a.class_get_namespace || !a.class_get_name ||
            (a.field_get_flags(field) & 0x10u)) return false;
        auto* type = a.class_from_il2cpp_type(a.field_get_type(field));
        const char* ns = type ? a.class_get_namespace(type) : nullptr;
        const char* name = type ? a.class_get_name(type) : nullptr;
        return ns && name && !std::strcmp(ns, "System") && !std::strcmp(name, typeName);
    };
    if (!validField(rareField, "Int32") || !validField(prefixField, "Byte")) {
        *error = "Reason=ItemRarityOrPrefixFieldAbiMismatch";
        return false;
    }
    const int rarityOffset = fieldOffset(item, "rare");
    const int prefixOffset = fieldOffset(item, "prefix");
    if (rarityOffset < 0 || prefixOffset < 0) {
        *error = "Reason=ItemRarityFieldOffsetsUnavailable";
        return false;
    }
    g_rareOffset = rarityOffset;
    g_prefixOffset = prefixOffset;
    g_prefixStats = stats;
    if (!bl::hook::install(prefix, prefixRarity, &g_originalPrefix)) {
        *error = "Reason=ItemPrefixHookRejected";
        return false;
    }
    g_prefixReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Item.Prefix Result=Ready");
    return true;
}

uint32_t popupRarityColor(int32_t type, const MethodInfo* method) {
    uint32_t argb = 0;
    if (g_hookReady.load(std::memory_order_acquire) && g_palette.read(type, &argb))
        return g_colorLayout.fromArgb(argb);
    return g_originalPopup ? g_originalPopup(type, method) : 0xffffffffu;
}

bool installPopupRarityHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_hookReady.load(std::memory_order_acquire)) return true;
    auto& a = api();
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    const MethodInfo* method = item ? findMethodBySignature(
        item, parseSignature("Color GetPopupRarityColor(int itemRarity)")) : nullptr;
    if (!method || !a.method_is_instance || a.method_is_instance(method) ||
        !a.method_get_return_type || !a.method_get_param_count ||
        !a.method_get_param || a.method_get_param_count(method) != 1 ||
        !a.class_from_il2cpp_type || !a.class_get_namespace || !a.class_get_name) {
        *error = "Reason=PopupMethodSignatureUnavailable";
        return false;
    }
    auto* argument = a.class_from_il2cpp_type(a.method_get_param(method, 0));
    const char* argumentNamespace = argument ? a.class_get_namespace(argument) : nullptr;
    const char* argumentName = argument ? a.class_get_name(argument) : nullptr;
    if (!argumentNamespace || !argumentName ||
        std::strcmp(argumentNamespace, "System") || std::strcmp(argumentName, "Int32")) {
        *error = "Reason=PopupRarityArgumentAbiMismatch";
        return false;
    }
    auto* color = a.class_from_il2cpp_type(a.method_get_return_type(method));
    if (!g_colorLayout.bind(color, error)) return false;
    if (!bl::hook::install(method, popupRarityColor, &g_originalPopup)) {
        *error = "Reason=PopupColorHookRejected";
        return false;
    }
    g_hookReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Item.GetPopupRarityColor Result=Ready");
    return true;
}

struct RarityTextVec2 { float x, y; };
using RarityDrawStringFn = void (*)(Il2CppObject*, Il2CppObject*, Il2CppString*,
    RarityTextVec2, uint32_t, float, RarityTextVec2, float, int32_t, float,
    const MethodInfo*);
using RarityMouseStaticFn = void (*)(Il2CppObject*, bool, const MethodInfo*);
using RarityMouseInstanceFn = void (*)(Il2CppObject*, Il2CppObject*, bool, const MethodInfo*);

RarityDrawStringFn g_originalRarityDrawString = nullptr;
RarityMouseStaticFn g_originalRarityMouseStatic = nullptr;
RarityMouseInstanceFn g_originalRarityMouseInstance = nullptr;
std::atomic<bool> g_groundTextHookReady{false};
bool g_rarityDrawStringHookInstalled = false;
NativeColorLayout g_rarityDrawColor;
int g_cacheRareOffset = -1;
int g_cacheTextOffset = -1;
int g_cacheBuffOffset = -1;
int g_cacheDiffOffset = -1;
int g_cacheDiffWidth = 0;
int g_hoverItemTypeOffset = -1;
native_static::Value g_hoverItem;
thread_local Il2CppString* g_rarityTextTarget = nullptr;
thread_local uint32_t g_rarityTextColor = 0;
thread_local bool g_rarityTextMatchByContent = false;

bool rarityTextMatches(Il2CppString* text) {
    const auto* target = g_rarityTextTarget;
    if (!text || !target) return false;
    if (target == text) return true;
    if (!g_rarityTextMatchByContent) return false;
    return sameNativeRarityName(text->chars, text->length,
        target->chars, target->length);
}

uint32_t nativeRarityToArgb(uint32_t native) {
    std::array<uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &native, bytes.size());
    return (static_cast<uint32_t>(bytes[g_rarityDrawColor.channels[3]]) << 24) |
        (static_cast<uint32_t>(bytes[g_rarityDrawColor.channels[0]]) << 16) |
        (static_cast<uint32_t>(bytes[g_rarityDrawColor.channels[1]]) << 8) |
        static_cast<uint32_t>(bytes[g_rarityDrawColor.channels[2]]);
}

void rarityDrawString(Il2CppObject* batch, Il2CppObject* font, Il2CppString* text,
    RarityTextVec2 position, uint32_t color, float rotation, RarityTextVec2 origin,
    float scale, int32_t effects, float depth, const MethodInfo* method) {
    if (rarityTextMatches(text)) {
        const uint32_t original = nativeRarityToArgb(color);
        const uint8_t opacity = static_cast<uint8_t>(original >> 24);
        if (shouldTintNativeRarityText(original, opacity))
            color = g_rarityDrawColor.fromArgb(
                tintNativeRarityText(g_rarityTextColor, opacity));
    }
    if (g_originalRarityDrawString)
        g_originalRarityDrawString(batch, font, text, position, color, rotation,
            origin, scale, effects, depth, method);
}

template <typename F>
void rarityMouseScope(Il2CppObject* info, F&& original) {
    if (!g_groundTextHookReady.load(std::memory_order_acquire) || !info) {
        original();
        return;
    }
    Il2CppString* text = bl::runtime::field<Il2CppString*>(info, g_cacheTextOffset);
    Il2CppString* buff = bl::runtime::field<Il2CppString*>(info, g_cacheBuffOffset);
    const int rarity = bl::runtime::field<int32_t>(info, g_cacheRareOffset);
    const int diff = g_cacheDiffWidth == 1
        ? static_cast<int>(bl::runtime::field<uint8_t>(info, g_cacheDiffOffset))
        : bl::runtime::field<int32_t>(info, g_cacheDiffOffset);
    uint32_t color = 0;
    if (!text || !g_palette.read(rarity, &color) || diff != 0 ||
        (buff && buff->length > 0)) {
        original();
        return;
    }
    Il2CppObject* hover = nullptr;
    if (!native_static::read(g_hoverItem, &hover, sizeof(hover))) {
        original();
        return;
    }
    if (hover && bl::runtime::field<int32_t>(hover, g_hoverItemTypeOffset) >= 1) {
        original();
        return;
    }
    const auto previousText = g_rarityTextTarget;
    const auto previousColor = g_rarityTextColor;
    const bool previousMatch = g_rarityTextMatchByContent;
    g_rarityTextTarget = text;
    g_rarityTextColor = color;
    g_rarityTextMatchByContent = false;
    original();
    g_rarityTextTarget = previousText;
    g_rarityTextColor = previousColor;
    g_rarityTextMatchByContent = previousMatch;
}

void rarityMouseStatic(Il2CppObject* info, bool worldMouse, const MethodInfo* method) {
    rarityMouseScope(info, [&] {
        if (g_originalRarityMouseStatic)
            g_originalRarityMouseStatic(info, worldMouse, method);
    });
}

void rarityMouseInstance(Il2CppObject* self, Il2CppObject* info,
    bool worldMouse, const MethodInfo* method) {
    rarityMouseScope(info, [&] {
        if (g_originalRarityMouseInstance)
            g_originalRarityMouseInstance(self, info, worldMouse, method);
    });
}

bool validateRarityTextField(Il2CppClass* cls, const char* name,
    Il2CppClass* expected, int* offset) {
    using namespace bl::il2cpp;
    auto& a = api();
    auto* field = findField(cls, name);
    if (!field || !a.field_get_flags || (a.field_get_flags(field) & 0x10u) ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        a.class_from_il2cpp_type(a.field_get_type(field)) != expected) return false;
    *offset = fieldOffset(cls, name);
    return *offset >= static_cast<int>(sizeof(Il2CppObject));
}

bool validateRarityTextVec2(Il2CppClass* type, std::string* error) {
    using namespace bl::il2cpp;
    auto& a = api();
    ValueLayout layout;
    if (!type || !a.class_get_name || !a.class_get_namespace ||
        !a.field_get_flags || !a.field_get_offset) return false;
    const char* typeNamespace = a.class_get_namespace(type);
    const char* typeName = a.class_get_name(type);
    if (!typeNamespace || !typeName ||
        std::strcmp(typeNamespace, "Microsoft.Xna.Framework") ||
        std::strcmp(typeName, "Vector2") ||
        !resolveValueLayout(type, &layout, error) || layout.size != 8) return false;
    for (const auto& [name, expected] :
        {std::pair{"X", size_t{0}}, std::pair{"Y", size_t{4}}}) {
        auto* field = findField(type, name);
        if (!field || !a.field_get_type || !a.class_from_il2cpp_type ||
            !a.field_get_offset || !a.class_get_namespace || !a.class_get_name ||
            (a.field_get_flags(field) & 0x10u)) return false;
        auto* primitive = a.class_from_il2cpp_type(a.field_get_type(field));
        const char* ns = primitive ? a.class_get_namespace(primitive) : nullptr;
        const char* kind = primitive ? a.class_get_name(primitive) : nullptr;
        size_t actual = 0;
        if (!ns || !kind || std::strcmp(ns, "System") ||
            std::strcmp(kind, "Single") ||
            !layout.offset(a.field_get_offset(field), 4, &actual) ||
            actual != expected) return false;
    }
    return true;
}

bool installRarityDrawStringHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_rarityDrawStringHookInstalled) return true;
    auto& a = api();
    auto* batch = findClass({"Microsoft.Xna.Framework.Graphics", "SpriteBatch", {}});
    auto* font = findClass({"Microsoft.Xna.Framework.Graphics", "SpriteFont", {}});
    auto* stringType = findClass({"System", "String", {}});
    auto* floatType = findClass({"System", "Single", {}});
    if (!batch || !font || !stringType || !floatType ||
        !a.method_is_instance || !a.method_get_param_count ||
        !a.method_get_param || !a.class_from_il2cpp_type) {
        *error = "Reason=RarityTextDrawTypesUnavailable";
        return false;
    }
    const auto* draw = findMethodBySignature(batch, parseSignature(
        "void DrawString(SpriteFont spriteFont, string text, Vector2 position, Color color, float rotation, Vector2 origin, float scale, SpriteEffects effects, float layerDepth)"));
    if (!draw || !a.method_is_instance(draw) ||
        a.method_get_param_count(draw) != 9 ||
        !isReturn(draw, "System", "Void") ||
        a.class_from_il2cpp_type(a.method_get_param(draw, 0)) != font ||
        a.class_from_il2cpp_type(a.method_get_param(draw, 1)) != stringType ||
        a.class_from_il2cpp_type(a.method_get_param(draw, 4)) != floatType ||
        a.class_from_il2cpp_type(a.method_get_param(draw, 6)) != floatType ||
        a.class_from_il2cpp_type(a.method_get_param(draw, 8)) != floatType) {
        *error = "Reason=RarityTextDrawSignatureMismatch";
        return false;
    }
    auto* vector = a.class_from_il2cpp_type(a.method_get_param(draw, 2));
    auto* origin = a.class_from_il2cpp_type(a.method_get_param(draw, 5));
    auto* color = a.class_from_il2cpp_type(a.method_get_param(draw, 3));
    auto* effects = a.class_from_il2cpp_type(a.method_get_param(draw, 7));
    NativeColorLayout layout;
    uint32_t effectAlignment = 0;
    const char* effectsNamespace = effects && a.class_get_namespace
        ? a.class_get_namespace(effects) : nullptr;
    const char* effectsName = effects && a.class_get_name
        ? a.class_get_name(effects) : nullptr;
    if (vector != origin || !validateRarityTextVec2(vector, error) ||
        !layout.bind(color, error) || !effects ||
        !a.class_is_enum || !a.class_is_enum(effects) ||
        !a.class_value_size || !a.class_enum_basetype ||
        a.class_value_size(effects, &effectAlignment) != 4 ||
        !effectsNamespace || !effectsName ||
        std::strcmp(effectsNamespace, "Microsoft.Xna.Framework.Graphics") ||
        std::strcmp(effectsName, "SpriteEffects")) {
        if (error->empty()) *error = "Reason=RarityTextDrawValueAbiMismatch";
        return false;
    }
    g_rarityDrawColor = layout;
    if (!bl::hook::install(draw, rarityDrawString, &g_originalRarityDrawString)) {
        *error = "Reason=RarityDrawStringHookRejected";
        return false;
    }
    g_rarityDrawStringHookInstalled = true;
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=SpriteBatch.DrawString Result=Ready");
    return true;
}

bool installRarityGroundTextHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_groundTextHookReady.load(std::memory_order_acquire)) return true;
    auto& a = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* cache = main ? findNested(main, "MouseTextCache") : nullptr;
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* stringType = findClass({"System", "String", {}});
    Il2CppClass* intType = findClass({"System", "Int32", {}});
    Il2CppClass* byteType = findClass({"System", "Byte", {}});
    Il2CppClass* boolType = findClass({"System", "Boolean", {}});
    if (!main || !cache || !item || !stringType || !intType ||
        !byteType || !boolType || !a.class_is_valuetype ||
        a.class_is_valuetype(cache) || !a.method_is_instance ||
        !a.method_get_param_count || !a.method_get_param ||
        !a.class_from_il2cpp_type) {
        *error = "Reason=MouseTextCacheReferenceAbiUnavailable";
        return false;
    }
    int rare = -1, text = -1, buff = -1, diff = -1, hoverType = -1;
    if (!validateRarityTextField(cache, "rare", intType, &rare) ||
        !validateRarityTextField(cache, "cursorText", stringType, &text) ||
        !validateRarityTextField(cache, "buffTooltip", stringType, &buff) ||
        !validateRarityTextField(item, "type", intType, &hoverType)) {
        *error = "Reason=MouseTextCacheFieldsUnavailable";
        return false;
    }
    int diffWidth = 1;
    if (!validateRarityTextField(cache, "diff", byteType, &diff)) {
        if (!validateRarityTextField(cache, "diff", intType, &diff)) {
            *error = "Reason=MouseTextDiffAbiUnavailable";
            return false;
        }
        diffWidth = 4;
    }
    auto hover = native_static::resolve(main, "HoverItem");
    if (!hover || hover.type != item ||
        native_static::size(hover) != sizeof(Il2CppObject*)) {
        *error = "Reason=HoverItemReferenceUnavailable";
        return false;
    }
    const auto* mouse = findMethodBySignature(main,
        parseSignature("void MouseTextInner(Main.MouseTextCache info, bool worldMouse)"));
    if (!mouse || !a.method_is_instance || !a.method_get_param_count ||
        !a.method_get_param || !a.class_from_il2cpp_type ||
        a.method_get_param_count(mouse) != 2 ||
        a.class_from_il2cpp_type(a.method_get_param(mouse, 0)) != cache ||
        a.class_from_il2cpp_type(a.method_get_param(mouse, 1)) != boolType ||
        !isReturn(mouse, "System", "Void")) {
        *error = "Reason=MouseTextSignatureUnavailable";
        return false;
    }
    if (!installRarityDrawStringHook(error)) return false;
    g_cacheRareOffset = rare;
    g_cacheTextOffset = text;
    g_cacheBuffOffset = buff;
    g_cacheDiffOffset = diff;
    g_cacheDiffWidth = diffWidth;
    g_hoverItemTypeOffset = hoverType;
    g_hoverItem = hover;
    const bool hooked = a.method_is_instance(mouse)
        ? bl::hook::install(mouse, rarityMouseInstance, &g_originalRarityMouseInstance)
        : bl::hook::install(mouse, rarityMouseStatic, &g_originalRarityMouseStatic);
    if (!hooked) {
        *error = "Reason=MouseTextInnerHookRejected";
        return false;
    }
    g_groundTextHookReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Main.MouseTextInner Result=Ready");
    return true;
}

using RarityTooltipStaticFn = void (*)(Il2CppObject*, int32_t, uint8_t,
    int32_t, int32_t, const MethodInfo*);
using RarityTooltipInstanceFn = void (*)(Il2CppObject*, Il2CppObject*, int32_t,
    uint8_t, int32_t, int32_t, const MethodInfo*);
using RarityAffixNameFn = Il2CppString* (*)(Il2CppObject*, const MethodInfo*);
RarityTooltipStaticFn g_originalRarityTooltipStatic = nullptr;
RarityTooltipInstanceFn g_originalRarityTooltipInstance = nullptr;
const MethodInfo* g_rarityAffixMethod = nullptr;
RarityAffixNameFn g_rarityAffixName = nullptr;
int g_rarityTooltipExpertOffset = -1;
std::atomic<bool> g_tooltipHookReady{false};

template <typename F>
void rarityTooltipScope(int32_t rare, uint8_t diff, F&& original) {
    if (!g_tooltipHookReady.load(std::memory_order_acquire) || diff != 0) {
        original();
        return;
    }
    uint32_t color = 0;
    if (!g_palette.read(rare, &color) || !g_rarityAffixName) {
        original();
        return;
    }
    Il2CppObject* item = nullptr;
    if (!native_static::read(g_hoverItem, &item, sizeof(item)) || !item ||
        bl::runtime::field<int32_t>(item, g_hoverItemTypeOffset) < 1 ||
        bl::runtime::field<uint8_t>(item, g_rarityTooltipExpertOffset) != 0) {
        original();
        return;
    }
    Il2CppString* name = g_rarityAffixName(item, g_rarityAffixMethod);
    if (!name || name->length <= 0 || name->length > 256) {
        original();
        return;
    }
    const auto priorText = g_rarityTextTarget;
    const auto priorColor = g_rarityTextColor;
    const bool priorMatch = g_rarityTextMatchByContent;
    g_rarityTextTarget = name;
    g_rarityTextColor = color;
    g_rarityTextMatchByContent = true;
    original();
    g_rarityTextTarget = priorText;
    g_rarityTextColor = priorColor;
    g_rarityTextMatchByContent = priorMatch;
}

void rarityTooltipStatic(Il2CppObject* cache, int32_t rare, uint8_t diff,
    int32_t x, int32_t y, const MethodInfo* method) {
    rarityTooltipScope(rare, diff, [&] {
        if (g_originalRarityTooltipStatic)
            g_originalRarityTooltipStatic(cache, rare, diff, x, y, method);
    });
}

void rarityTooltipInstance(Il2CppObject* self, Il2CppObject* cache,
    int32_t rare, uint8_t diff, int32_t x, int32_t y, const MethodInfo* method) {
    rarityTooltipScope(rare, diff, [&] {
        if (g_originalRarityTooltipInstance)
            g_originalRarityTooltipInstance(self, cache, rare, diff, x, y, method);
    });
}

bool installRarityTooltipHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_tooltipHookReady.load(std::memory_order_acquire)) return true;
    if (!installRarityDrawStringHook(error)) return false;
    auto& a = api();
    auto* main = findClass({"Terraria", "Main", {}});
    auto* item = findClass({"Terraria", "Item", {}});
    auto* cache = main ? findNested(main, "MouseTextCache") : nullptr;
    const auto* method = main ? findMethodBySignature(main, parseSignature(
        "void MouseText_DrawItemTooltip(Main.MouseTextCache info, int rare, byte diff, int X, int Y)")) : nullptr;
    const auto* affix = item ? findMethodBySignature(item,
        parseSignature("string AffixName()")) : nullptr;
    const auto* boolType = findClass({"System", "Boolean", {}});
    const auto* stringType = findClass({"System", "String", {}});
    auto* intType = findClass({"System", "Int32", {}});
    if (!method || !affix || !cache || !item || !boolType || !stringType ||
        !a.method_get_param_count || a.method_get_param_count(method) != 5 ||
        !a.method_is_instance || !a.method_is_instance(affix) ||
        a.method_get_param_count(affix) != 0 ||
        !isReturn(method, "System", "Void") ||
        !isReturn(affix, "System", "String") ||
        !parameterIs(cache, method, 0, "Terraria", "MouseTextCache") ||
        !parameterIs(nullptr, method, 1, "System", "Int32") ||
        !parameterIs(nullptr, method, 2, "System", "Byte") ||
        !parameterIs(nullptr, method, 3, "System", "Int32") ||
        !parameterIs(nullptr, method, 4, "System", "Int32")) {
        *error = "Reason=RarityTooltipSignatureMismatch";
        return false;
    }
    int expert = -1;
    int hoverType = -1;
    auto hover = native_static::resolve(main, "HoverItem");
    if (!intType || !a.class_is_valuetype || a.class_is_valuetype(cache) ||
        !validateRarityTextField(item, "expert",
            const_cast<Il2CppClass*>(boolType), &expert) ||
        !validateRarityTextField(item, "type", intType, &hoverType) ||
        !hover || hover.type != item ||
        native_static::size(hover) != sizeof(Il2CppObject*)) {
        *error = "Reason=RarityTooltipItemLayoutMismatch";
        return false;
    }
    auto affixPointer = methodPointer(affix);
    if (!affixPointer) {
        *error = "Reason=RarityAffixNamePointerMissing";
        return false;
    }
    g_rarityAffixMethod = affix;
    g_rarityAffixName = reinterpret_cast<RarityAffixNameFn>(affixPointer);
    g_rarityTooltipExpertOffset = expert;
    g_hoverItemTypeOffset = hoverType;
    g_hoverItem = hover;
    const bool hooked = a.method_is_instance(method)
        ? bl::hook::install(method, rarityTooltipInstance, &g_originalRarityTooltipInstance)
        : bl::hook::install(method, rarityTooltipStatic, &g_originalRarityTooltipStatic);
    if (!hooked) {
        *error = "Reason=RarityTooltipHookRejected";
        return false;
    }
    g_tooltipHookReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Main.MouseText_DrawItemTooltip Result=Ready");
    return true;
}

struct DiscoField {
    FieldInfo* field = nullptr;
    int bytes = 0;
};

DiscoField discoField(Il2CppClass* main, const char* name) {
    using namespace bl::il2cpp;
    auto& a = api();
    FieldInfo* field = main ? findField(main, name) : nullptr;
    if (!field || !a.field_get_flags || !(a.field_get_flags(field) & 0x10u) ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_namespace || !a.class_get_name) return {};
    auto* type = a.class_from_il2cpp_type(a.field_get_type(field));
    const char* ns = type ? a.class_get_namespace(type) : nullptr;
    const char* kind = type ? a.class_get_name(type) : nullptr;
    if (!ns || !kind || std::strcmp(ns, "System")) return {};
    if (!std::strcmp(kind, "Byte")) return {field, 1};
    if (!std::strcmp(kind, "Int32")) return {field, 4};
    return {};
}

std::string discoSnapshot() {
    using namespace bl::il2cpp;
    static bool bound = false;
    static DiscoField fields[3]{};
    if (!bound) {
        Il2CppClass* main = findClass({"Terraria", "Main", {}});
        fields[0] = discoField(main, "DiscoR");
        fields[1] = discoField(main, "DiscoG");
        fields[2] = discoField(main, "DiscoB");
        bound = fields[0].field && fields[1].field && fields[2].field;
    }
    if (!bound || !api().field_static_get_value) return "255,255,255";
    int channels[3]{};
    for (int i = 0; i < 3; ++i) {
        if (fields[i].bytes == 1) {
            uint8_t value = 255;
            api().field_static_get_value(fields[i].field, &value);
            channels[i] = value;
        } else {
            int32_t value = 255;
            api().field_static_get_value(fields[i].field, &value);
            channels[i] = std::clamp(value, 0, 255);
        }
    }
    return std::to_string(channels[0]) + "," +
        std::to_string(channels[1]) + "," + std::to_string(channels[2]);
}

}

bool nativeManagedRarityColor(int type, uint32_t* argb) {
    if (!argb) return false;
    const auto identity = nativeRarities().identityForType(type);
    return identity.managed && !identity.mod.empty() && g_palette.read(type, argb);
}

bool registerNativeRaritySampling(const std::string& mod, const std::string& assembly) {
    if (mod.empty() || assembly.empty()) return false;
    RarityMod entry;
    entry.id = mod;
    entry.assembly = assembly;
    const auto snapshot = nativeRarities().snapshot();
    for (const auto& rarity : *snapshot)
        if (rarity.managed && rarity.mod == mod) entry.rarities.push_back({rarity.type, rarity.name});
    if (entry.rarities.empty()) return false;
    std::lock_guard guard(g_mutex);
    for (const auto& previous : g_mods)
        if (previous.id == mod) return false;
    g_mods.push_back(std::move(entry));
    return true;
}

void pumpNativeRarityGameplay() {
    ++g_tick;
    if ((g_tick & 1u) != 0) return;
    RarityMod current;
    {
        std::lock_guard guard(g_mutex);
        if (g_mods.empty()) return;
        bool found = false;
        for (size_t i = 0; i < g_mods.size(); ++i) {
            g_modCursor = (g_modCursor + 1) % g_mods.size();
            auto& mod = g_mods[g_modCursor];
            if (mod.failed) continue;
            current.id = mod.id;
            current.assembly = mod.assembly;
            if (mod.ready && !mod.rarities.empty()) {
                current.rarities.push_back(mod.rarities[mod.cursor % mod.rarities.size()]);
                mod.cursor = (mod.cursor + 1) % mod.rarities.size();
            }
            found = true;
            break;
        }
        if (!found) return;
    }

    if (current.rarities.empty()) {
        std::string error;
        const int state = managedRarityWarmupState(current.id, current.assembly, &error);
        if (state == 0) return;
        std::lock_guard guard(g_mutex);
        for (auto& mod : g_mods) {
            if (mod.id != current.id) continue;
            mod.ready = state == 1;
            mod.failed = state < 0;
            if (mod.failed)
                BL_ERROR("[CSharp][RarityLoader] Mod=%s Stage=Warmup Result=Rejected Detail=%s",
                    mod.id.c_str(), error.c_str());
            break;
        }
        return;
    }

    std::string response;
    const auto& sample = current.rarities.front();
    if (!invokeManagedRarity(current.id, current.assembly, sample.name,
            0, discoSnapshot(), &response, 1200)) {
        if (g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Mod=%s Stage=SampleColor Result=Pending Detail=%s",
                current.id.c_str(), response.c_str());
        return;
    }
    uint32_t argb = 0;
    if (!decodeRarityColorWire(response, &argb) || !g_palette.publish(sample.type, argb)) {
        if (g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Mod=%s Stage=DecodeColor Result=Rejected Detail=%s",
                current.id.c_str(), response.c_str());
        return;
    }
    if (g_tableHookReady.load(std::memory_order_acquire))
        writeRarityTable(sample.type, argb);
    if (!g_tableHookReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (installRarityTableHook(&error)) writeRarityTable(sample.type, argb);
        else if (g_tick % 600u == 0) BL_WARN("[CSharp][RarityLoader] Stage=InstallTable Result=Pending Detail=%s",
            error.c_str());
    }
    if (!g_hookReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installPopupRarityHook(&error) && g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Stage=InstallPopup Result=Pending Detail=%s",
                error.c_str());
    }
    if (!g_groundTextHookReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installRarityGroundTextHook(&error) && g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Stage=InstallGroundText Result=Pending Detail=%s",
                error.c_str());
    }
    if (!g_tooltipHookReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installRarityTooltipHook(&error) && g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Stage=InstallTooltip Result=Pending Detail=%s",
                error.c_str());
    }
    if (!g_prefixReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installPrefixRarityHook(&error) && g_tick % 600u == 0)
            BL_WARN("[CSharp][RarityLoader] Stage=InstallPrefix Result=Pending Detail=%s",
                error.c_str());
    }
}

}
