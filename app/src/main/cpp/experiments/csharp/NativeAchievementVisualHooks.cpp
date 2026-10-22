#include "experiments/csharp/NativeAchievementVisualHooks.h"

#include "experiments/csharp/NativeAchievementRegistry.h"
#include "content/common/ContentAssets.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace bl::csharp {
namespace {

struct Vector2 { float x, y; };

using DrawFn = void (*)(Il2CppObject*, Il2CppObject*, int32_t,
                       Vector2, float, const MethodInfo*);
using OpenFn = void (*)(Il2CppObject*, int32_t, int32_t, const MethodInfo*);
using PopupFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);

DrawFn originalDraw = nullptr;
OpenFn originalOpen = nullptr;
PopupFn originalPopup = nullptr;

Il2CppClass* menuClass = nullptr;
Il2CppClass* popupClass = nullptr;
FieldInfo* iconsField = nullptr;
int popupTextureOffset = -1;
int menuListOffset = -1;
const MethodInfo* sortedGetter = nullptr;
bool drawInstalled = false;
bool popupInstalled = false;
bool openInstalled = false;
std::atomic<bool> ready{false};
std::map<const Il2CppObject*, std::size_t> menuRevisions;

const MethodInfo* check(Il2CppClass* cls, const char* sig,
                        bool instance, std::string* error) {
    auto& a = il2cpp::api();
    const MethodInfo* method = cls
        ? il2cpp::findMethodBySignature(cls, il2cpp::parseSignature(sig))
        : nullptr;
    if (!method || !il2cpp::methodPointer(method) ||
        !a.method_is_instance || a.method_is_instance(method) != instance) {
        *error = std::string("Achievement visual ABI unavailable: ") + sig;
        return nullptr;
    }
    return method;
}

Il2CppClass* findMenuClass() {
    auto& a = il2cpp::api();
    if (!a.gameImage || !a.image_get_class_count ||
        !a.image_get_class || !a.class_get_name) return nullptr;
    Il2CppClass* found = nullptr;
    const size_t count = a.image_get_class_count(a.gameImage);
    for (size_t i = 0; i < count; ++i) {
        Il2CppClass* candidate = a.image_get_class(a.gameImage, i);
        const char* name = candidate ? a.class_get_name(candidate) : nullptr;
        if (name && std::strcmp(name, "GUIAchievementsMenu") == 0) {
            if (found) return nullptr; // mais de uma classe seria ambíguo
            found = candidate;
        }
    }
    return found;
}

Il2CppObject* call(const MethodInfo* method, void* self,
                   void** args = nullptr) {
    auto& a = il2cpp::api();
    if (!method || !a.runtime_invoke) return nullptr;
    Il2CppObject* exception = nullptr;
    auto* result = a.runtime_invoke(method, self, args, &exception);
    return exception ? nullptr : result;
}

Il2CppObject* achievementAt(Il2CppObject* menu, int32_t index) {
    if (!menu || index < 0 || !sortedGetter) return nullptr;
    auto& a = il2cpp::api();
    Il2CppObject* list = call(sortedGetter, menu);
    if (!list || !a.object_get_class) return nullptr;
    auto* listClass = a.object_get_class(list);
    // Os métodos da lista são resolvidos uma vez por classe; não repetir
    // busca de métodos do IL2CPP para cada entrada desenhada a 60 FPS.
    static Il2CppClass* cachedClass = nullptr;
    static const MethodInfo* countGetter = nullptr;
    static const MethodInfo* itemGetter = nullptr;
    if (listClass != cachedClass) {
        cachedClass = listClass;
        countGetter = listClass
            ? a.class_get_method_from_name(listClass, "get_Count", 0) : nullptr;
        itemGetter = listClass
            ? a.class_get_method_from_name(listClass, "get_Item", 1) : nullptr;
    }
    if (!countGetter || !itemGetter) return nullptr;
    Il2CppObject* boxedCount = call(countGetter, list);
    if (!boxedCount) return nullptr;
    int count = *reinterpret_cast<int*>(
        reinterpret_cast<char*>(boxedCount) + sizeof(Il2CppObject));
    if (count < 0 || count > 4096 || index >= count) return nullptr;
    void* args[]{&index};
    return call(itemGetter, list, args);
}

void hkDraw(Il2CppObject* self, Il2CppObject* grid, int32_t index,
            Vector2 position, float scale, const MethodInfo* method) {
    if (!originalDraw) return;
    Il2CppObject* achievement = achievementAt(self, index);
    Il2CppObject* asset = nativeAchievementIconAsset(achievement);
    if (!asset || !iconsField) {
        originalDraw(self, grid, index, position, scale, method);
        return;
    }
    auto& a = il2cpp::api();
    Il2CppObject* texture = bl::runtime::content::textureAssetValue(asset);
    if (!texture) {
        originalDraw(self, grid, index, position, scale, method);
        return;
    }
    Il2CppObject* old = nullptr;
    a.field_static_get_value(iconsField, &old);
    a.field_static_set_value(iconsField, &texture);
    originalDraw(self, grid, index, position, scale, method);
    a.field_static_set_value(iconsField, &old);
}

void hkOpen(Il2CppObject* self, int32_t backTo, int32_t selected,
            const MethodInfo* method) {
    bool rebuilt = false;
    if (self && menuListOffset >= 0) {
        const auto status = nativeAchievementRegistryStatus();
        const std::size_t current =
            status.registered * static_cast<std::size_t>(0x9e3779b9u) ^
            static_cast<std::size_t>(status.appliedRevisions);
        if ((status.registered > 0 || status.appliedRevisions > 0) &&
            menuRevisions[self] != current) {
            auto& a = il2cpp::api();
            void** location = reinterpret_cast<void**>(
                reinterpret_cast<char*>(self) + menuListOffset);
            a.gc_wbarrier_set_field(self, location, nullptr);
            menuRevisions[self] = current;
            rebuilt = true;
        }
    }
    if (originalOpen) originalOpen(self, backTo, selected, method);
    if (rebuilt) releaseRetiredNativeAchievements();
}

void hkPopup(Il2CppObject* self, Il2CppObject* achievement,
             const MethodInfo* method) {
    if (originalPopup) originalPopup(self, achievement, method);
    Il2CppObject* asset = nativeAchievementIconAsset(achievement);
    if (!asset || !self || popupTextureOffset < 0) return;
    auto& a = il2cpp::api();
    void** slot = reinterpret_cast<void**>(
        reinterpret_cast<char*>(self) + popupTextureOffset);
    a.gc_wbarrier_set_field(self, slot, asset);
}

bool resolve(std::string* error) {
    auto& a = il2cpp::api();
    if (!a.class_get_method_from_name || !a.class_from_il2cpp_type ||
        !a.class_is_valuetype || !a.class_value_size ||
        !a.method_get_param || !a.method_get_param_count ||
        !a.gc_wbarrier_set_field || !a.field_static_set_value ||
        !a.field_static_get_value || !a.runtime_invoke ||
        !a.object_get_class || !a.field_get_flags ||
        !a.field_get_type || !a.field_get_offset) {
        *error = "Achievement visual IL2CPP exports unavailable";
        return false;
    }
    menuClass = findMenuClass();
    popupClass = il2cpp::findClass({
        "Terraria.UI.InGamePopups", "AchievementUnlockedPopup", {}});
    if (!menuClass || !popupClass) {
        *error = "Achievement menu or popup class unavailable";
        return false;
    }

    const MethodInfo* draw = check(menuClass,
        "void AchievementDraw(ItemGrid_Layout gridLayout, int index, Vector2 position, float scale)",
        true, error);
    const MethodInfo* open = check(menuClass,
        "void Open(int backTo, int selected)", true, error);
    const MethodInfo* popup = check(popupClass,
        "void .ctor(Achievement achievement)", true, error);
    if (!draw || !open || !popup) return false;

    // Struct Vector2 de 8 bytes (float, float) usa ABI HFA AArch64.
    if (a.method_get_param_count(draw) != 4) {
        *error = "AchievementDraw parameter count changed";
        return false;
    }
    auto* grid = a.class_from_il2cpp_type(a.method_get_param(draw, 0));
    auto* vec = a.class_from_il2cpp_type(a.method_get_param(draw, 2));
    uint32_t alignment = 0;
    if (!grid || a.class_is_valuetype(grid) || !vec ||
        !a.class_is_valuetype(vec) || a.class_value_size(vec, &alignment) != 8) {
        *error = "AchievementDraw ARM64 grid/vector parameter ABI differs";
        return false;
    }

    sortedGetter = a.class_get_method_from_name(
        menuClass, "get_SortedAchievementsData", 0);
    if (!sortedGetter || !a.method_is_instance(sortedGetter)) {
        *error = "SortedAchievementsData getter unavailable";
        return false;
    }
    const auto* assetClass = bl::runtime::content::textureAssetClass();
    if (!assetClass || !a.field_get_type) {
        *error = "Native asset class unavailable";
        return false;
    }
    FieldInfo* textureField = il2cpp::findField(popupClass, "_achievementTexture");
    auto* fieldType = textureField
        ? a.class_from_il2cpp_type(a.field_get_type(textureField)) : nullptr;
    if (!fieldType || fieldType != assetClass) {
        *error = "AchievementUnlockedPopup texture field type differs";
        return false;
    }
    popupTextureOffset = il2cpp::fieldOffset(popupClass, "_achievementTexture");
    if (popupTextureOffset < static_cast<int>(sizeof(Il2CppObject))) {
        *error = "Achievement popup texture offset invalid";
        return false;
    }

    if (!il2cpp::nativeFieldContract(menuClass,
            "AchievementsIcons", "Microsoft.Xna.Framework.Graphics.Texture2D",
            true, sizeof(void*), &iconsField, nullptr, error))
        return false;

    FieldInfo* cacheField = il2cpp::findField(menuClass, "_achievements");
    if (!cacheField || !a.field_get_flags ||
        (a.field_get_flags(cacheField) & 0x10u)) {
        *error = "Achievements menu cache field unavailable or static";
        return false;
    }
    auto* cacheClass = a.class_from_il2cpp_type(a.field_get_type(cacheField));
    if (!cacheClass || a.class_is_valuetype(cacheClass)) {
        *error = "Achievements menu cache is not a reference";
        return false;
    }
    menuListOffset = il2cpp::fieldOffset(menuClass, "_achievements");
    if (menuListOffset < static_cast<int>(sizeof(Il2CppObject))) {
        *error = "Achievements menu cache offset invalid";
        return false;
    }

    if (!drawInstalled && !bl::hook::install(draw, hkDraw, &originalDraw)) {
        *error = "AchievementDraw hook installation failed";
        return false;
    }
    drawInstalled = true;
    if (!openInstalled && !bl::hook::install(open, hkOpen, &originalOpen)) {
        *error = "Achievement menu Open hook installation failed";
        return false;
    }
    openInstalled = true;
    if (!popupInstalled && !bl::hook::install(popup, hkPopup, &originalPopup)) {
        *error = "Achievement popup hook installation failed";
        return false;
    }
    popupInstalled = true;
    return true;
}
}

bool installNativeAchievementVisualHooks(std::string* error) {
    if (!error) return false;
    error->clear();
    if (ready.load(std::memory_order_acquire)) return true;
    if (!resolve(error)) return false;
    ready.store(true, std::memory_order_release);
    BL_INFO("[CSharp][AchievementLoader] Menu, hidden text and popup native visuals ready");
    return true;
}

bool nativeAchievementVisualHooksReady() {
    return ready.load(std::memory_order_acquire);
}

}
