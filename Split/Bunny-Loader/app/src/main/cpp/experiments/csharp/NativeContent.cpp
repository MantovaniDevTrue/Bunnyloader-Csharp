#include "experiments/csharp/NativeContent.h"

#include "content/common/GameRefs.h"
#include "content/items/ModItems.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <sys/stat.h>

namespace bl::csharp {
namespace {

struct NativeRefs {
    int itemDamage = -1;
    int itemUseTime = -1;
    int itemUseAnimation = -1;
    int itemUseStyle = -1;
    int itemShoot = -1;
    int itemShootSpeed = -1;
    int itemKnockBack = -1;
    int itemRanged = -1;
    int itemNoMelee = -1;
    int itemAutoReuse = -1;
    int itemRare = -1;
    int itemMaxStack = -1;
    int projAiStyle = -1;
    int projFriendly = -1;
    int projHostile = -1;
    int projRanged = -1;
    int projPenetrate = -1;
    int projTimeLeft = -1;
    int projIgnoreWater = -1;
    int projTileCollide = -1;
    int projWidth = -1;
    int projHeight = -1;
};

struct NativeWeaponDefaults {
    int damage;
    int useTime;
    float shootSpeed;
    int shootType;
};

struct NativeProjectileDefaults {
    int width;
    int height;
    int aiStyle;
    int penetrate;
    int timeLeft;
    bool friendly;
    bool hostile;
};

using ItemDefaultsFn = void (*)(Il2CppObject*, int32_t, int32_t, const MethodInfo*);
using ProjectileDefaultsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);

NativeRefs g_refs{};
std::map<int, NativeWeaponDefaults> g_weapons;
std::map<int, NativeProjectileDefaults> g_projectiles;
ItemDefaultsFn g_originalItem = nullptr;
ProjectileDefaultsFn g_originalProjectile = nullptr;
bool g_ready = false;

bool validName(const char* value) {
    if (!value || !*value) return false;
    size_t length = 0;
    for (const unsigned char* s = reinterpret_cast<const unsigned char*>(value); *s; ++s) {
        ++length;
        if (length > 72) return false;
        const unsigned char c = *s;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

bool validTexture(const char* path) {
    if (!path || !*path || path[0] == '/' || path[0] == '\\') return false;
    std::string p(path);
    return p.size() <= 160 && p.find("..") == std::string::npos &&
           p.find('\\') == std::string::npos &&
           p.compare(0, 8, "Content/") == 0 &&
           p.size() > 12 && p.substr(p.size() - 4) == ".png";
}

void hkItemDefaults(Il2CppObject* self, int32_t type, int32_t variant, const MethodInfo* method) {
    auto it = g_weapons.find(type);
    if (it == g_weapons.end() || !self) {
        if (g_originalItem) g_originalItem(self, type, variant, method);
        return;
    }
    bl::runtime::prepareModItem(self, type);
    const auto& r = g_refs;
    const auto& d = it->second;
    bl::runtime::field<int32_t>(self, r.itemDamage) = d.damage;
    bl::runtime::field<int32_t>(self, r.itemUseTime) = d.useTime;
    bl::runtime::field<int32_t>(self, r.itemUseAnimation) = d.useTime;
    bl::runtime::field<int32_t>(self, r.itemUseStyle) = 5;
    bl::runtime::field<int32_t>(self, r.itemShoot) = d.shootType;
    bl::runtime::field<float>(self, r.itemShootSpeed) = d.shootSpeed;
    bl::runtime::field<float>(self, r.itemKnockBack) = 2.0f;
    bl::runtime::field<uint8_t>(self, r.itemRanged) = 1;
    bl::runtime::field<uint8_t>(self, r.itemNoMelee) = 1;
    bl::runtime::field<uint8_t>(self, r.itemAutoReuse) = 1;
    bl::runtime::field<int32_t>(self, r.itemRare) = 2;
    bl::runtime::field<int32_t>(self, r.itemMaxStack) = 1;
    bl::runtime::finishModItem(self, type);
}

void hkProjectileDefaults(Il2CppObject* self, int32_t type, const MethodInfo* method) {
    if (g_originalProjectile) g_originalProjectile(self, type, method);
    auto it = g_projectiles.find(type);
    if (it == g_projectiles.end() || !self) return;
    const auto& r = g_refs;
    const auto& d = it->second;
    bl::runtime::field<int32_t>(self, r.projAiStyle) = d.aiStyle;
    bl::runtime::field<uint8_t>(self, r.projFriendly) = d.friendly ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projHostile) = d.hostile ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projRanged) = 1;
    bl::runtime::field<int32_t>(self, r.projPenetrate) = d.penetrate;
    bl::runtime::field<int32_t>(self, r.projTimeLeft) = d.timeLeft;
    bl::runtime::field<uint8_t>(self, r.projIgnoreWater) = 1;
    bl::runtime::field<uint8_t>(self, r.projTileCollide) = 1;
    bl::runtime::field<int32_t>(self, r.projWidth) = d.width;
    bl::runtime::field<int32_t>(self, r.projHeight) = d.height;
    bl::runtime::finishModProjectile(self, type);
}

}

bool prepareNativeContent() {
    if (g_ready) return true;
    using namespace bl::il2cpp;
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    if (!item || !projectile) return false;

    NativeRefs r{};
    r.itemDamage = fieldOffset(item, "damage");
    r.itemUseTime = fieldOffset(item, "useTime");
    r.itemUseAnimation = fieldOffset(item, "useAnimation");
    r.itemUseStyle = fieldOffset(item, "useStyle");
    r.itemShoot = fieldOffset(item, "shoot");
    r.itemShootSpeed = fieldOffset(item, "shootSpeed");
    r.itemKnockBack = fieldOffset(item, "knockBack");
    r.itemRanged = fieldOffset(item, "ranged");
    r.itemNoMelee = fieldOffset(item, "noMelee");
    r.itemAutoReuse = fieldOffset(item, "autoReuse");
    r.itemRare = fieldOffset(item, "rare");
    r.itemMaxStack = fieldOffset(item, "maxStack");
    r.projAiStyle = fieldOffset(projectile, "aiStyle");
    r.projFriendly = fieldOffset(projectile, "friendly");
    r.projHostile = fieldOffset(projectile, "hostile");
    r.projRanged = fieldOffset(projectile, "ranged");
    r.projPenetrate = fieldOffset(projectile, "penetrate");
    r.projTimeLeft = fieldOffset(projectile, "timeLeft");
    r.projIgnoreWater = fieldOffset(projectile, "ignoreWater");
    r.projTileCollide = fieldOffset(projectile, "tileCollide");
    r.projWidth = fieldOffset(projectile, "width");
    r.projHeight = fieldOffset(projectile, "height");

    const int* all = &r.itemDamage;
    for (size_t i = 0; i < sizeof(NativeRefs) / sizeof(int); ++i) {
        if (all[i] < 0) {
            BL_ERROR("csharp native content: campo %zu indisponivel", i);
            return false;
        }
    }
    const MethodInfo* itemMethod = findMethodBySignature(
        item, parseSignature("void SetDefaults(int Type, ItemVariant variant)"));
    const MethodInfo* projectileMethod = findMethodBySignature(
        projectile, parseSignature("void SetDefaults(int Type)"));
    if (!itemMethod || !projectileMethod) return false;

    g_refs = r;
    if (!bl::hook::install(itemMethod, hkItemDefaults, &g_originalItem)) return false;
    if (!bl::hook::install(projectileMethod, hkProjectileDefaults, &g_originalProjectile)) return false;
    g_ready = true;
    BL_INFO("csharp native content: hooks C++ de item e projetil instalados");
    return true;
}

int registerNativeContent(void* context, const NativeContentSpec* spec) {
    if (!g_ready || !context || !spec || !validName(spec->name) ||
        !validTexture(spec->texture)) return -1;
    auto& ctx = *static_cast<NativeRegisterContext*>(context);
    const std::string path = ctx.modDir + "/content/" + spec->texture;
    if (!fileExists(path)) {
        BL_ERROR("csharp native: textura inexistente %s", path.c_str());
        return -1;
    }
    const std::string display = spec->displayName && *spec->displayName
        ? std::string(spec->displayName).substr(0, 128)
        : std::string(spec->name);
    if (spec->kind == NativeProjectile) {
        if (spec->width < 1 || spec->width > 512 ||
            spec->height < 1 || spec->height > 512 ||
            spec->timeLeft < 1 || spec->timeLeft > 36000 ||
            spec->aiStyle < 0 || spec->aiStyle > 255 ||
            spec->penetrate < -1 || spec->penetrate > 100) return -1;
        bl::runtime::ModProjectileDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModProjectile(std::move(def));
        if (type >= 0) {
            g_projectiles[type] = {spec->width, spec->height, spec->aiStyle,
                                   spec->penetrate, spec->timeLeft, true, false};
            ++ctx.projectiles;
        }
        return type;
    }
    if (spec->kind == NativeWeapon) {
        if (spec->damage < 1 || spec->damage > 1000 ||
            spec->useTime < 2 || spec->useTime > 120 ||
            spec->shootSpeed < 1.f || spec->shootSpeed > 40.f ||
            !bl::runtime::modProjectileBelongsTo(spec->shootType, ctx.modId)) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_weapons[type] = {spec->damage, spec->useTime, spec->shootSpeed, spec->shootType};
            ++ctx.items;
        }
        return type;
    }
    return -1;
}

}
