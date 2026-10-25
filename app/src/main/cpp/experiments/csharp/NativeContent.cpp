#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/ContentTexture.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/WeaponSpeedContract.h"
#include "experiments/csharp/WeaponSpeedContract.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/ManagedMinionBuff.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/GuardedCall.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/NativeDefaults.h"
#include "experiments/csharp/NativeVisualRenderer.h"
#include "experiments/csharp/NativeTileGameplay.h"
#include "experiments/csharp/NativeTileFlags.h"
#include "experiments/csharp/NativeTileAdjacencyPolicy.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include "experiments/csharp/NativeSaplingGrowthPolicy.h"
#include "experiments/csharp/NativeTileMiningPolicy.h"
#include "experiments/csharp/NativeTreeTexturePolicy.h"
#include "experiments/csharp/NativeGorePolicy.h"
#include "experiments/csharp/ManagedGoreRuntime.h"
#include "experiments/csharp/NativeManagedTablePublication.h"
#include "experiments/csharp/NativeBootEffects.h"

#include "content/common/GameRefs.h"
#include "content/items/ModItems.h"
#include "content/common/TypeTables.h"
#include "content/npcs/ModNpcs.h"
#include "content/buffs/ModBuffs.h"
#include "content/tiles/ModTiles.h"
#include "content/tiles/ModTrees.h"
#include "content/common/ContentAssets.h"
#include "content/walls/ModWalls.h"
#include "content/tiles/ModTileMap.h"
#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <cmath>
#include <string>
#include <sys/stat.h>

namespace bl::csharp {
using il2cpp::nativeFieldContract;
using il2cpp::nativeArrayContract;
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
    int itemSummon = -1;
    int itemNoMelee = -1;
    int itemAutoReuse = -1;
    int itemRare = -1;
    int itemMaxStack = -1;
    int projAiStyle = -1;
    int projFriendly = -1;
    int projHostile = -1;
    int projRanged = -1;
    int projMinion = -1;
    int projMinionSlots = -1;
    int projPenetrate = -1;
    int projTimeLeft = -1;
    int projIgnoreWater = -1;
    int projTileCollide = -1;
    int projWidth = -1;
    int projHeight = -1;
    int itemAccessory = -1;
    int itemShieldSlot = -1;
    int itemShoeSlot = -1;
    int playerHellfireTreads = -1;
    int playerHideVisibleAccessory = -1;
    int playerVelocity = -1;
    int playerMount = -1;
    int itemBuffType = -1;
    int itemBuffTime = -1;
    int itemConsumable = -1;
    int npcWidth = -1;
    int npcHeight = -1;
    int npcDamage = -1;
    int npcDefense = -1;
    int npcLifeMax = -1;
    int npcAiStyle = -1;
    int npcType = -1;
    int npcLife = -1;
    int npcPosition = -1;
    int npcVelocity = -1;
    int projectilePosition = -1;
    int projectileVelocity = -1;
    int playerPosition = -1;
    int playerWidth = -1;
    int playerHeight = -1;
    int projectileType = -1;
    int projectileDamage = -1;
    int projectileOwner = -1;
    int playerStatDefense = -1;
    int playerBuffType = -1;
    int playerBuffTime = -1;
    int itemType = -1;
    int playerMoveSpeed = -1;
    int playerArmor = -1;
    int playerTownNpcs = -1;
    int playerJumpSpeedBoost = -1;
    int playerExtraFall = -1;
    int playerWet = -1;
    int playerHead = -1;
    int playerBody = -1;
    int playerLegs = -1;
    int playerMaxRunSpeed = -1;
    int playerRunAcceleration = -1;
    int playerFrameShadow = -1;
    int playerFrameOutline = -1;
    int playerWhoAmI = -1;
    int playerLifeRegen = -1;
    int playerManaRegen = -1;
    int playerDead = -1;
    int playerLife = -1;
    int playerLifeMax = -1;
    int playerImmune = -1;
    int playerImmuneTime = -1;
    int playerHurtCooldowns = -1;
    int playerOwnedProjectileCounts = -1;   // opcional: so buffs de minion
    int sourceNpcOffset = -1;
    int sourceProjectileOffset = -1;
    int sourceSize = 0;
};

struct NativeWeaponDefaults {
    int damage;
    int useTime;
    float shootSpeed;
    int shootType;
    bool summon = false;
    int minionBuffType = -1;
    int minionBuffTime = 0;
};

struct NativeAccessoryDefaults {
    int defenseBonus;
    int moveSpeedPercent = 0;
    int movementAccelerationPercent = 0;
    bool frameShadow = false;
    std::string worldJoinMessage;
    int lifeRegenBonus = 0;
    int manaRegenBonus = 0;
    int visualMode = 0;
    std::string visualTexture = {};
    float visualScale = 1.f;
    float visualOpacity = 1.f;
    uint32_t visualColor = 0xffffffffu;
    int nativeEquipSlot = -1;
};

const MethodInfo* g_removeMinionBuff=nullptr;
std::string g_minionBindError;
using BuffEquipFn=void(*)(Il2CppObject*,int32_t,const MethodInfo*);
BuffEquipFn g_originalBuffEquip=nullptr;
bool g_buffMultipliersInstalled=false;
struct NativeBuffDefaults {
    int defenseBonus;
    int minionProjectileType = -1;
    int defenseMultiplierPercent=100;
};

struct NativePotionDefaults {
    int buffType;
    int duration;
};

struct NativeNpcDefaults {
    int width;
    int height;
    int damage;
    int defense;
    int lifeMax;
    int aiStyle;
};

struct NativeProjectileDefaults {
    int width;
    int height;
    int aiStyle;
    int penetrate;
    int timeLeft;
    bool friendly;
    bool hostile;
    bool minion = false;
    bool minionContactDamage = false;
    float minionSlots = 0.f;
    bool tileCollide = true;
    bool ignoreWater = true;
    int tileCollisionMode = 0;
    float bounceRestitution = 1.f;
    float bounceMinimumSpeed = 0.f;
    int tileCollisionSurvivalLimit = 0;
    int tilePlatformCollision = 0;
    int tileCollisionBoxWidth = 0;
    int tileCollisionBoxHeight = 0;
    float tileCollisionAnchorOffsetX = 0.f;
    float tileCollisionAnchorOffsetY = 0.f;
    int frames = 1;
    int animationFrameTicks=6,minionScanTicks=8;
    float minionTargetRange=700.f;
};

struct ManagedHurtBinding {
    std::string modId;
    std::string assemblyPath;
    std::vector<int> accessoryTypes;
    bool active = false;
};

std::mutex g_hurtBindingsMutex;
std::map<std::string, ManagedHurtBinding> g_hurtBindings;

using ItemDefaultsFn = void (*)(Il2CppObject*, int32_t, int32_t, const MethodInfo*);
using ProjectileDefaultsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using ProjectileDamageFn = void (*)(Il2CppObject*, const MethodInfo*);
using NpcDefaultsPtrFn = void (*)(Il2CppObject*, int32_t, const void*, const MethodInfo*);
using ApplyEquipFn = void (*)(Il2CppObject*, int32_t, Il2CppObject*, const MethodInfo*);
using UpdateBuffsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using HorizontalMovementFn = void (*)(Il2CppObject*, const MethodInfo*);
using SetArmorEffectVisualsFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using PlayerSpawnFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PlayerUpdateFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PlayerStageFn = void (*)(Il2CppObject*, const MethodInfo*);
using PlayerHurtFn = double (*)(Il2CppObject*, const void*, int32_t, int32_t,
                               uint8_t, uint8_t, uint8_t, int32_t, uint8_t,
                               const MethodInfo*);

NativeRefs g_refs{};
const MethodInfo* g_costumeAddBuff=nullptr;
bool g_costumeReady=false;
struct CostumePlayerCycle {
    Il2CppObject* player=nullptr;
    int previousType=-1,currentType=-1;
    bool power=false;
};
std::array<CostumePlayerCycle,256> g_costumeCycles{};

FieldInfo* g_mainNpcArrayField = nullptr;
FieldInfo* g_mainProjectileArrayField = nullptr;
std::map<int, NativeWeaponDefaults> g_weapons;
std::map<int, NativeProjectileDefaults> g_projectiles;
struct ProjectileDefinition {
    NativeProjectileDefaults defaults{};
    ProjectileGameplayFields fields{};
    SummonMovement movement{};
    std::string key{};
};
using ProjectileDefinitions=std::map<int,ProjectileDefinition>;
std::shared_ptr<const ProjectileDefinitions> g_projectileView=std::make_shared<const ProjectileDefinitions>();
std::shared_ptr<const std::map<int,NativeWeaponDefaults>> g_weaponView=std::make_shared<const std::map<int,NativeWeaponDefaults>>();
void publishWeaponDefaults() {
    std::atomic_store_explicit(&g_weaponView,std::make_shared<const std::map<int,NativeWeaponDefaults>>(g_weapons),std::memory_order_release);
}
// Summon staves can create a stationary projectile at the cursor. Zero is a
// valid initial velocity; AI owns subsequent movement. Keep the same contract
// during registration and both managed-default application paths.
bool validWeaponShootSpeed(float speed) {
    return std::isfinite(speed) && speed >= 0.f && speed <= 40.f;
}

// Called after registerNativeContent has checked runtime, identity and texture.
// Keep reservation and publication together so rejection never consumes an ID.
int registerNativeWeapon(NativeRegisterContext& ctx, const NativeContentSpec& spec,
                         const std::string& path, const std::string& display) {
    auto reject = [&](const char* reason, const char* expected, const std::string& actual) {
        ctx.lastError = "[CSharp][ERROR][Register] Mod=" + ctx.modId +
            " Class=" + spec.name + " File=" + ctx.sourceFile +
            " Stage=NativeWeapon Expected=" + expected + " Actual=" + actual +
            " Result=Rejected Reason=" + reason;
        BL_ERROR("%s", ctx.lastError.c_str());
        return -1;
    };
    if (spec.damage < 1 || spec.damage > 1000)
        return reject("WeaponDamageOutOfRange", "Damage:1..1000", std::to_string(spec.damage));
    if (spec.useTime < 2 || spec.useTime > 120)
        return reject("WeaponUseTimeOutOfRange", "UseTime:2..120", std::to_string(spec.useTime));
    if (!validNativeWeaponShootSpeed(spec.shootSpeed, spec.summon))
        return reject("WeaponShootSpeedOutOfRange", "FiniteShootSpeed:0..40", std::to_string(spec.shootSpeed));
    if (!bl::runtime::modProjectileBelongsTo(spec.shootType, ctx.modId))
        return reject("WeaponProjectileOwnershipMismatch", "ProjectileOwnedByMod", std::to_string(spec.shootType));
    bl::runtime::ModItemDef def{};
    def.mod = ctx.modId;
    def.name = spec.name;
    def.texture = path;
    def.names = ctx.sourceNames;
    def.names.push_back({"", display});
    const int type = bl::runtime::registerModItem(std::move(def));
    if (type < 0)
        return reject("NativeItemReservationRejected", "NonNegativeContentId", std::to_string(type));
    g_weapons[type] = {spec.damage, spec.useTime, spec.shootSpeed, spec.shootType};
    publishWeaponDefaults();
    ++ctx.items;
    return type;
}
void publishProjectileDefinition(int type,const NativeProjectileDefaults& defaults,
                                 const std::string& key) {
    auto next=std::make_shared<ProjectileDefinitions>(*std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire));
    auto& record=(*next)[type];record.defaults=defaults;record.key=key;
    std::atomic_store_explicit(&g_projectileView,std::shared_ptr<const ProjectileDefinitions>(std::move(next)),std::memory_order_release);
}
std::map<int, NativeNpcDefaults> g_npcs;
std::map<int, NativeAccessoryDefaults> g_accessories;
std::mutex g_nativeEquipDescriptorsMutex;
std::map<int, ManagedNativeEquipTexture> g_nativeEquipDescriptors;
std::map<std::string, std::string> g_globalWorldMessages;
std::map<std::string, std::string> g_respawnMessages;
std::map<std::string, std::string> g_deadMessages;
bool g_preUpdateEnabled = false;
bool g_postUpdateEnabled = false;
bool g_deadEventDelivered = false;
bool g_anyLifeRegen = false;
bool g_anyManaRegen = false;
std::map<int, NativeBuffDefaults> g_buffs;
std::shared_ptr<const std::map<int,NativeBuffDefaults>> g_buffView=std::make_shared<const std::map<int,NativeBuffDefaults>>();
void publishBuffDefaults(){std::atomic_store_explicit(&g_buffView,std::make_shared<const std::map<int,NativeBuffDefaults>>(g_buffs),std::memory_order_release);}

std::map<int, NativePotionDefaults> g_potions;
ItemDefaultsFn g_originalItem = nullptr;
ProjectileDefaultsFn g_originalProjectile = nullptr;
ProjectileDamageFn g_originalProjectileDamage = nullptr;
FieldInfo* g_nativeMinionProjPet = nullptr;
FieldInfo* g_nativeMinionSacrificable = nullptr;
FieldInfo* g_nativeMinionTargeting = nullptr;
std::atomic<int> g_nativeMinionContactSamples{0};
int g_originalDamageField=-1;
using ApplyProjectileStatsFn=void(*)(Il2CppObject*,Il2CppObject*,const MethodInfo*);
ApplyProjectileStatsFn g_originalApplyProjectileStats=nullptr;
struct ProjectileSourceItem {int field=-1;const MethodInfo* getter=nullptr;};
std::map<Il2CppClass*,ProjectileSourceItem> g_projectileSourceItems;
NpcDefaultsPtrFn g_originalNpc = nullptr;
ApplyEquipFn g_originalEquip = nullptr;
ApplyEquipFn g_originalEquipVanity = nullptr;
const MethodInfo* g_cancelBootVisuals = nullptr;
const MethodInfo* g_placeBootFlames = nullptr;
const MethodInfo* g_mountActive = nullptr;
UpdateBuffsFn g_originalUpdateBuffs = nullptr;
HorizontalMovementFn g_originalHorizontalMovement = nullptr;
SetArmorEffectVisualsFn g_originalSetArmorEffectVisuals = nullptr;
PlayerSpawnFn g_originalPlayerSpawn = nullptr;
PlayerUpdateFn g_originalPlayerUpdate = nullptr;
PlayerStageFn g_originalUpdateDead = nullptr;
PlayerStageFn g_originalLifeRegen = nullptr;
PlayerStageFn g_originalManaRegen = nullptr;
PlayerHurtFn g_originalPlayerHurt = nullptr;
constexpr int g_reviveSpawnContext = 0;
bool g_ready = false;
bool g_npcReady = false;
bool g_effectsReady = false;
bool g_playerPoliciesReady = false;
Il2CppObject* g_dodgePendingPlayer = nullptr;
int g_dodgePendingLife = 0;
int g_dodgeLifeSamples = 0;

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

bool enableManagedPrefixEligibility(int type) {
    using namespace bl::il2cpp;
    static FieldInfo* prefixSet = nullptr;
    static Il2CppClass* prefixSetsClass = nullptr;
    if (!prefixSet) {
        Il2CppClass* itemId = findClass({"Terraria.ID", "ItemID", {}});
        Il2CppClass* sets = itemId ? findNested(itemId, "Sets") : nullptr;
        prefixSetsClass = sets;
        prefixSet = sets ? findField(sets, "CanGetPrefixes") : nullptr;
    }
    auto& runtime = api();
    if (!prefixSet || type < 0 || type >= bl::runtime::itemTypeCount() ||
        !runtime.field_static_get_value || !runtime.field_static_set_value)
        return false;
    if (prefixSetsClass && runtime.runtime_class_init)
        runtime.runtime_class_init(prefixSetsClass);
    Il2CppArray* entries = nullptr;
    runtime.field_static_get_value(prefixSet, &entries);
    if (!entries || static_cast<uintptr_t>(type) >= entries->length) {
        if (!entries) {
            BL_WARN("csharp prefix: tabela CanGetPrefixes ainda indisponivel para item %d", type);
            return false;
        }
        const uintptr_t required = static_cast<uintptr_t>(bl::runtime::itemTypeCount());
        auto* expanded = bl::runtime::TypeTables::resizedCopy(entries, required);
        if (!expanded) {
            BL_WARN("csharp prefix: falha ao ampliar CanGetPrefixes para item %d", type);
            return false;
        }
        runtime.field_static_set_value(prefixSet, expanded);
        entries = expanded;
        BL_INFO("csharp prefix: CanGetPrefixes ampliado para %zu itens", required);
    }
    auto* flags = static_cast<uint8_t*>(arrayData(entries));
    if (flags[type]) return true;
    flags[type] = 1;
    BL_INFO("csharp prefix: item %d habilitado para reforja", type);
    return true;
}

void hkItemDefaults(Il2CppObject* self, int32_t type, int32_t variant, const MethodInfo* method) {
    NativeDefaultsAfter nativeDefaults{self,false,type};
    if (self && isBasicItem(type)) {
        bl::runtime::prepareModItem(self,type);
        if (!applyBasicItemToObject(self,type)) {
            BL_ERROR("csharp item: defaults indisponiveis para type=%d",type);
            return;
        }
        if (basicItemHasDamage(type)||basicItemIsAccessory(type)) enableManagedPrefixEligibility(type);
        bl::runtime::finishModItem(self,type);
        return;
    }
    const auto weapons=std::atomic_load_explicit(&g_weaponView,std::memory_order_acquire);
    auto it = weapons->find(type);
    auto accessory = g_accessories.find(type);
    auto potion = g_potions.find(type);
    if ((it == weapons->end() && accessory == g_accessories.end() &&
         potion == g_potions.end()) || !self) {
        if (g_originalItem) g_originalItem(self, type, variant, method);
        return;
    }
    bl::runtime::prepareModItem(self, type);
    const auto& r = g_refs;
    if (accessory != g_accessories.end()) {
        bl::runtime::field<int32_t>(self, r.itemMaxStack) = 1;
        bl::runtime::field<int32_t>(self, r.itemRare) = 2;
        bl::runtime::field<uint8_t>(self, r.itemAccessory) = 1;
        const int slot = accessory->second.nativeEquipSlot;
        if (slot >= 0 && slot < 128) {
            const int offset = accessory->second.visualMode == 1
                ? r.itemShieldSlot : accessory->second.visualMode == 3
                ? r.itemShoeSlot : -1;
            if (offset >= 0)
                bl::runtime::field<int8_t>(self, offset) = static_cast<int8_t>(slot);
        }
        enableManagedPrefixEligibility(type);
        bl::runtime::finishModItem(self, type);
        return;
    }
    if (potion != g_potions.end()) {
        bl::runtime::field<int32_t>(self, r.itemMaxStack) = 30;
        bl::runtime::field<int32_t>(self, r.itemRare) = 2;
        bl::runtime::field<int32_t>(self, r.itemUseTime) = 15;
        bl::runtime::field<int32_t>(self, r.itemUseAnimation) = 15;
        bl::runtime::field<uint8_t>(self, r.itemUseStyle) = 2;
        bl::runtime::field<uint8_t>(self, r.itemConsumable) = 1;
        bl::runtime::field<int32_t>(self, r.itemBuffType) = potion->second.buffType;
        bl::runtime::field<int32_t>(self, r.itemBuffTime) = potion->second.duration;
        bl::runtime::finishModItem(self, type);
        return;
    }
    const auto& d = it->second;
    bl::runtime::field<int32_t>(self, r.itemDamage) = d.damage;
    bl::runtime::field<int32_t>(self, r.itemUseTime) = d.useTime;
    bl::runtime::field<int32_t>(self, r.itemUseAnimation) = d.useTime;
    bl::runtime::field<uint8_t>(self, r.itemUseStyle) = 5;
    bl::runtime::field<int32_t>(self, r.itemShoot) = d.shootType;
    bl::runtime::field<float>(self, r.itemShootSpeed) = d.shootSpeed;
    bl::runtime::field<float>(self, r.itemKnockBack) = 2.0f;
    bl::runtime::field<uint8_t>(self, r.itemRanged) = d.summon ? 0 : 1;
    if (d.summon) {
        bl::runtime::field<uint8_t>(self, r.itemSummon) = 1;
        bl::runtime::field<int32_t>(self, r.itemBuffType) = d.minionBuffType;
        bl::runtime::field<int32_t>(self, r.itemBuffTime) = d.minionBuffTime;
    }
    bl::runtime::field<uint8_t>(self, r.itemNoMelee) = 1;
    bl::runtime::field<uint8_t>(self, r.itemAutoReuse) = 1;
    bl::runtime::field<int32_t>(self, r.itemRare) = 2;
    bl::runtime::field<int32_t>(self, r.itemMaxStack) = 1;
    enableManagedPrefixEligibility(type);
    bl::runtime::finishModItem(self, type);
}

void hkProjectileStatsFromSource(Il2CppObject* projectile,Il2CppObject* source,const MethodInfo* method) {
    if(g_originalApplyProjectileStats)g_originalApplyProjectileStats(projectile,source,method);
    if(!projectile||g_originalDamageField<0)return;
    const int type=runtime::field<int32_t>(projectile,g_refs.projectileType);
    if(!std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire)->count(type))return;
    auto& baseline=runtime::field<int32_t>(projectile,g_originalDamageField);
    if(baseline>0)return; // Preserve native/other-mod initialization and the incoming scaled damage.
    int itemDamage=-1;
    if(source) {
        auto found=g_projectileSourceItems.find(source->klass);
        if(found==g_projectileSourceItems.end()) {
            ProjectileSourceItem binding;std::string reason;
            if(il2cpp::findField(source->klass,"Item"))
                il2cpp::nativeFieldContract(source->klass,"Item","Terraria.Item",false,sizeof(void*),nullptr,&binding.field,&reason);
            binding.getter=il2cpp::findMethodBySignature(source->klass,il2cpp::parseSignature("Terraria.Item get_Item()"));
            if(binding.getter&&!il2cpp::api().method_is_instance(binding.getter))binding.getter=nullptr;
            // Cache metadata only, never retain an IL2CPP object across frames/GC.
            found=g_projectileSourceItems.emplace(source->klass,binding).first;
            if(!reason.empty())BL_WARN("[CSharp][WARN][ProjectileSource] ContentId=%d Stage=BindSourceItem Result=TryTypedPropertyOrIncomingDamage Detail=%s",type,reason.c_str());
        }
        Il2CppObject* item=nullptr;
        if(found->second.field>=0)item=runtime::field<Il2CppObject*>(source,found->second.field);
        else if(found->second.getter) {
            Il2CppObject* error=nullptr;item=il2cpp::api().runtime_invoke(found->second.getter,source,nullptr,&error);
            if(error) {static unsigned failures=0;if(failures++<4)BL_ERROR("[CSharp][ERROR][ProjectileSource] ContentId=%d Method=get_Item Stage=Invoke Result=UseIncomingDamage Reason=NativeManagedException Exception=%s",type,il2cpp::describeException(error).c_str());item=nullptr;}
        }
        if(item)itemDamage=runtime::field<int32_t>(item,g_refs.itemDamage);
    }
    baseline=summonSourceDamage(baseline,itemDamage,runtime::field<int32_t>(projectile,g_refs.projectileDamage));
    static unsigned samples=0;
    if(samples++<8)BL_INFO("[CSharp][ProjectileSource] ContentId=%d Stage=ApplyStatsFromSource OriginalDamage=%d IncomingDamage=%d ItemDamage=%d Result=BaselineInitialized",
        type,baseline,runtime::field<int32_t>(projectile,g_refs.projectileDamage),itemDamage);
}

void hkNativeMinionDamage(Il2CppObject* projectile, const MethodInfo* method) {
    ProjectileDamageFn original = g_originalProjectileDamage;
    if (!original) return;
    if (!projectile || g_refs.projectileType < 0 || !g_nativeMinionProjPet) {
        original(projectile, method);
        return;
    }
    const int type = bl::runtime::field<int32_t>(projectile, g_refs.projectileType);
    if (!nativeManagedMinionContactEnabled(type)) {
        original(projectile, method);
        return;
    }
    Il2CppArray* flags = nullptr;
    il2cpp::api().field_static_get_value(g_nativeMinionProjPet, &flags);
    if (!il2cpp::nativeArrayContract(flags, "Boolean", 1, 65536) ||
        type < 0 || static_cast<uintptr_t>(type) >= flags->length) {
        original(projectile, method);
        return;
    }
    const int damage = runtime::field<int32_t>(projectile,g_refs.projectileDamage);
    auto* values = static_cast<uint8_t*>(arrayData(flags));
    const uint8_t originalPet = values[type];
    values[type] = 0;
    const int sample = g_nativeMinionContactSamples.fetch_add(1, std::memory_order_relaxed);
    if (sample < 5)
        BL_INFO("csharp summon: native contact invoked type=%d damage=%d pet=%d",
                type, damage, originalPet);
    original(projectile, method);
    values[type] = originalPet;
}

void hkProjectileDefaults(Il2CppObject* self, int32_t type, const MethodInfo* method) {
    notifyManagedSourceProjectileDefaults(self);
    applyNativeStaticDefaults(true,type);
    if (g_originalProjectile) g_originalProjectile(self, type, method);
    const auto definitions=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    auto it = definitions->find(type);
    if (it == definitions->end() || !self) return;
    const auto& r = g_refs;
    const auto& d = it->second.defaults;
    bl::runtime::field<int32_t>(self, r.projAiStyle) = d.aiStyle;
    bl::runtime::field<uint8_t>(self, r.projFriendly) = d.friendly ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projHostile) = d.hostile ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projRanged) = d.minion ? 0 : 1;
    if (d.minion && r.projMinion >= 0 && r.projMinionSlots >= 0) {
        bl::runtime::field<uint8_t>(self, r.projMinion) = 1;
        bl::runtime::field<float>(self, r.projMinionSlots) = d.minionSlots;
    }
    bl::runtime::field<int32_t>(self, r.projPenetrate) = d.penetrate;
    bl::runtime::field<int32_t>(self, r.projTimeLeft) = d.timeLeft;
    bl::runtime::field<uint8_t>(self, r.projIgnoreWater) = d.ignoreWater ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projTileCollide) = d.tileCollide ? 1 : 0;
    bl::runtime::field<int32_t>(self, r.projWidth) = d.width;
    bl::runtime::field<int32_t>(self, r.projHeight) = d.height;
    if (d.minion) {
        FieldInfo* const tables[]={g_nativeMinionProjPet,g_nativeMinionSacrificable,g_nativeMinionTargeting};
        for(auto* table:tables) {
            Il2CppArray* flags=nullptr;if(table)il2cpp::api().field_static_get_value(table,&flags);
            if(il2cpp::nativeArrayContract(flags,"Boolean",1,65536)&&type>=0&&static_cast<uintptr_t>(type)<flags->length)
                static_cast<uint8_t*>(arrayData(flags))[type]=1;
        }
    }
    applyNativeDefaults(self,true,type);
    bl::runtime::finishModProjectile(self, type);
}

void hkNpcDefaults(Il2CppObject* self, int32_t type, const void* params, const MethodInfo* method) {
    if (g_originalNpc) g_originalNpc(self, type, params, method);
    auto it = g_npcs.find(type);
    if (it == g_npcs.end() || !self) return;
    const auto& d = it->second;
    const auto& r = g_refs;
    bl::runtime::field<int32_t>(self, r.npcWidth) = d.width;
    bl::runtime::field<int32_t>(self, r.npcHeight) = d.height;
    bl::runtime::field<int32_t>(self, r.npcDamage) = d.damage;
    bl::runtime::field<int32_t>(self, r.npcDefense) = d.defense;
    bl::runtime::field<int32_t>(self, r.npcLifeMax) = d.lifeMax;
    bl::runtime::field<int32_t>(self, r.npcAiStyle) = d.aiStyle;
    bl::runtime::finishModNpc(self, type);
}

void refreshNativeEquipSlot(Il2CppObject* item) {
    if (!item || g_refs.itemType < 0) return;
    const auto found = g_accessories.find(bl::runtime::field<int32_t>(item, g_refs.itemType));
    if (found == g_accessories.end()) return;
    const auto& visual = found->second;
    const int offset = visual.visualMode == 1 ? g_refs.itemShieldSlot :
                       visual.visualMode == 3 ? g_refs.itemShoeSlot : -1;
    if (offset >= 0 && visual.nativeEquipSlot > 0 && visual.nativeEquipSlot < 128)
        bl::runtime::field<int8_t>(item, offset) = static_cast<int8_t>(visual.nativeEquipSlot);
}

void applyNativeBootVisuals(Il2CppObject* self, int32_t slot,
                           Il2CppObject* item, bool vanity) {
    if (!self || !item || g_refs.playerHellfireTreads < 0) return;
    const auto found = g_accessories.find(bl::runtime::field<int32_t>(item, g_refs.itemType));
    if (found == g_accessories.end() || found->second.visualMode != 3) return;
    if (!vanity) {
        if (g_refs.playerHideVisibleAccessory < 0) return;
        auto* hidden = bl::runtime::field<Il2CppArray*>(self, g_refs.playerHideVisibleAccessory);
        if (!hidden || slot < 0 || static_cast<uintptr_t>(slot) >= hidden->length ||
            static_cast<uint8_t*>(arrayData(hidden))[slot]) return;
    }
    if (g_cancelBootVisuals && bl::il2cpp::methodPointer(g_cancelBootVisuals)) {
        using Fn = void (*)(Il2CppObject*, const MethodInfo*);
        reinterpret_cast<Fn>(bl::il2cpp::methodPointer(g_cancelBootVisuals))(self, g_cancelBootVisuals);
    }
    bl::runtime::field<uint8_t>(self, g_refs.playerHellfireTreads) = 1;
    if (!g_placeBootFlames || !bl::il2cpp::methodPointer(g_placeBootFlames) ||
        g_refs.playerVelocity < 0 || g_refs.playerPosition < 0 ||
        g_refs.playerWidth < 0 || g_refs.playerHeight < 0 || g_refs.playerDead < 0 ||
        g_refs.playerMount < 0 || !g_mountActive || !bl::il2cpp::methodPointer(g_mountActive)) return;
    auto* mount = bl::runtime::field<Il2CppObject*>(self, g_refs.playerMount);
    using ActiveFn = bool (*)(Il2CppObject*, const MethodInfo*);
    const bool mounted = mount && reinterpret_cast<ActiveFn>(bl::il2cpp::methodPointer(g_mountActive))(mount, g_mountActive);
    struct Vec { float x, y; };
    const auto pos = bl::runtime::field<Vec>(self, g_refs.playerPosition);
    const auto velocity = bl::runtime::field<Vec>(self, g_refs.playerVelocity);
    int32_t x = 0, y = 0;
    if (!nativeBootFlameTile(pos.x, pos.y,
            bl::runtime::field<int32_t>(self, g_refs.playerWidth),
            bl::runtime::field<int32_t>(self, g_refs.playerHeight),
            velocity.x, velocity.y, bl::runtime::field<uint8_t>(self, g_refs.playerDead),
            mounted, &x, &y)) return;
    using FlameFn = bool (*)(Il2CppObject*, int32_t, int32_t, const MethodInfo*);
    const bool placed = reinterpret_cast<FlameFn>(bl::il2cpp::methodPointer(g_placeBootFlames))(self, x, y, g_placeBootFlames);
    static unsigned logs = 0;
    if (logs++ < 3)
        BL_INFO("csharp boots: rotina nativa tile=%d,%d chama=%d", x, y, placed);
}


CostumePlayerCycle* costumeCycle(Il2CppObject* player) {
    if(!g_costumeReady||!player||g_refs.playerWhoAmI<0)return nullptr;
    const int index=bl::runtime::field<int32_t>(player,g_refs.playerWhoAmI);
    if(index<0||index>=static_cast<int>(g_costumeCycles.size()))return nullptr;
    auto& cycle=g_costumeCycles[static_cast<size_t>(index)];
    if(cycle.player!=player)cycle={player,-1,-1,false};
    return &cycle;
}
bool costumeEquipped(Il2CppObject* player,int type,bool vanity) {
    if(!player||type<0||g_refs.playerArmor<0||g_refs.itemType<0)return false;
    auto* armor=bl::runtime::field<Il2CppArray*>(player,g_refs.playerArmor);
    if(!armor||armor->length<10||armor->length>40)return false;
    auto** items=static_cast<Il2CppObject**>(arrayData(armor));
    const uintptr_t begin=vanity?13u:3u,end=std::min<uintptr_t>(begin+7u,armor->length);
    for(uintptr_t slot=begin;slot<end;++slot) {
        auto* item=items[slot];
        if(item&&bl::runtime::field<int32_t>(item,g_refs.itemType)==type)return true;
    }
    return false;
}
bool costumeHidden(Il2CppObject* player,int type) {
    if(g_refs.playerArmor<0||g_refs.playerHideVisibleAccessory<0)return true;
    auto* armor=bl::runtime::field<Il2CppArray*>(player,g_refs.playerArmor);
    auto* hidden=bl::runtime::field<Il2CppArray*>(player,g_refs.playerHideVisibleAccessory);
    if(!armor||armor->length<10||armor->length>40||
       !nativeArrayContract(hidden,"Boolean",1,60))return true;
    auto** items=static_cast<Il2CppObject**>(arrayData(armor));
    auto* flags=static_cast<uint8_t*>(arrayData(hidden));
    for(uintptr_t slot=3;slot<10;++slot)
        if(items[slot]&&bl::runtime::field<int32_t>(items[slot],g_refs.itemType)==type)
            return slot<hidden->length&&flags[slot]!=0;
    return true;
}
int costumeTownCount(Il2CppObject* player) {
    if(!player||g_refs.playerTownNpcs<0)return 0;
    const float count=bl::runtime::field<float>(player,g_refs.playerTownNpcs);
    return std::isfinite(count)&&count>0?static_cast<int>(count):0;
}

void hkApplyEquip(Il2CppObject* self, int32_t slot, Il2CppObject* item, const MethodInfo* method) {
    // Itens salvos nasceram antes do registro de texturas. Atualize ANTES
    // de o Terraria selecionar shield/shoe para o desenho deste tick.
    refreshNativeEquipSlot(item);
    if (g_originalEquip) g_originalEquip(self, slot, item, method);
    if (!self || !item) return;
    const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
    NativeCostumeEffect costume;
    if(nativeCostumesActive()&&nativeCostumeEffect(type,&costume)) {
        if(auto* cycle=costumeCycle(self))cycle->currentType=type;
        if(costumeTownCount(self)>=costume.requiredTownNpcs&&g_costumeAddBuff) {
            int32_t duration=60,buff=costume.buffType;uint8_t fromNetPvP=1;
            void* args[]={&buff,&duration,&fromNetPvP};
            Il2CppObject* error=nullptr;
            il2cpp::api().runtime_invoke(g_costumeAddBuff,self,args,&error);
            if(error){static unsigned failures=0;if(failures++<3)
                BL_ERROR("[CSharp][Costume] Stage=UpdateEquips Result=AddBuffFailed Detail=%s",
                    il2cpp::describeException(error).c_str());}
        }
    }
    const auto it = g_accessories.find(type);
    if (it == g_accessories.end()) return;
    auto& defense = bl::runtime::field<int32_t>(self, g_refs.playerStatDefense);
    defense += it->second.defenseBonus;
    applyNativeBootVisuals(self, slot, item, false);
}

void hkApplyEquipVanity(Il2CppObject* self, int32_t slot, Il2CppObject* item, const MethodInfo* method) {
    refreshNativeEquipSlot(item);
    if (g_originalEquipVanity) g_originalEquipVanity(self, slot, item, method);
    applyNativeBootVisuals(self, slot, item, true);
}

void hkHorizontalMovement(Il2CppObject* self, const MethodInfo* method) {
    if (!self) {
        if (g_originalHorizontalMovement)
            g_originalHorizontalMovement(self, method);
        return;
    }
    int movementPercent = 0;
    int movementAcceleration = 0;
    if (g_refs.playerArmor >= 0) {
        auto* armor = bl::runtime::field<Il2CppArray*>(self, g_refs.playerArmor);
        if (armor && armor->length >= 10 && armor->length <= 40) {
            auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
            for (uintptr_t i = 3; i < 10; ++i) {
                Il2CppObject* item = equipped[i];
                if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
                    continue;
                const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
                const auto found = g_accessories.find(type);
                if (found != g_accessories.end()) {
                    movementPercent += found->second.moveSpeedPercent;
                    movementAcceleration += found->second.movementAccelerationPercent;
                }
            }
        }
    }
    if (movementPercent > 0 && movementPercent <= 100) {
        const float factor = 1.f + static_cast<float>(movementPercent) / 100.f;
        auto& moveSpeed = bl::runtime::field<float>(self, g_refs.playerMoveSpeed);
        auto& maxSpeed = bl::runtime::field<float>(self, g_refs.playerMaxRunSpeed);
        auto& acceleration = bl::runtime::field<float>(self, g_refs.playerRunAcceleration);
        const float before = maxSpeed;
        moveSpeed += static_cast<float>(movementPercent) / 100.f;
        maxSpeed *= factor;
        acceleration *= factor;
        static int logged = 0;
        if (logged < 4) {
            ++logged;
            BL_INFO("csharp player: velocidade nativa +%d%% aplicada ao correr maxRunSpeed=%.3f->%.3f",
                    movementPercent, before, maxSpeed);
        }
    }
    if (movementAcceleration > 0 && movementAcceleration <= 60) {
        auto& acceleration = bl::runtime::field<float>(self, g_refs.playerRunAcceleration);
        acceleration *= 1.f + static_cast<float>(movementAcceleration) / 100.f;
        static int accelerationLogs = 0;
        if (accelerationLogs++ < 3)
            BL_INFO("csharp BunnyPlayer UpdateMovement: +%d%% aceleracao nativa",
                    movementAcceleration);
    }
    if (g_originalHorizontalMovement)
        g_originalHorizontalMovement(self, method);
}

bool managedShadowEquipped(Il2CppObject* player) {
    if (!player || g_refs.playerArmor < 0) return false;
    auto* armor = bl::runtime::field<Il2CppArray*>(player, g_refs.playerArmor);
    if (!armor || armor->length < 10 || armor->length > 40) return false;
    auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
    for (uintptr_t slot = 3; slot < 10; ++slot) {
        Il2CppObject* item = equipped[slot];
        if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
            continue;
        const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
        const auto found = g_accessories.find(type);
        if (found != g_accessories.end() && found->second.frameShadow)
            return true;
    }
    return false;
}

void hkSetArmorEffectVisuals(Il2CppObject* self, Il2CppObject* drawPlayer,
                             const MethodInfo* method) {
    if (g_originalSetArmorEffectVisuals)
        g_originalSetArmorEffectVisuals(self, drawPlayer, method);
    if (!self) return;
    Il2CppObject* actor = drawPlayer ? drawPlayer : self;
    if (managedShadowEquipped(actor)) {
        if (g_refs.playerFrameShadow >= 0)
            bl::runtime::field<uint8_t>(self, g_refs.playerFrameShadow) = 1;
        if (g_refs.playerFrameOutline >= 0)
            bl::runtime::field<uint8_t>(self, g_refs.playerFrameOutline) = 1;
        static int visualLogs = 0;
        if (visualLogs++ < 3)
            BL_INFO("csharp BunnyPlayer FrameEffects: sombra de desenho e contorno ativos");
    }
    if(g_costumeReady&&nativeCostumesActive()&&g_refs.playerArmor>=0&&g_refs.itemType>=0) {
        auto* cycle=costumeCycle(actor);
        auto* armor=bl::runtime::field<Il2CppArray*>(actor,g_refs.playerArmor);
        if(cycle&&armor&&armor->length>=20&&armor->length<=40) {
            auto** items=static_cast<Il2CppObject**>(arrayData(armor));
            for(uintptr_t slot=3;slot<20;++slot) {
                if(slot>=10&&slot<13)continue;
                auto* item=items[slot];
                if(!item)continue;
                NativeCostumeEffect effect;
                const int type=bl::runtime::field<int32_t>(item,g_refs.itemType);
                if(!nativeCostumeEffect(type,&effect))continue;
                const bool forced=costumeEquipped(actor,type,true);
                if(!forced&&!(cycle->power&&!costumeHidden(actor,type)))continue;
                const bool wet=bl::runtime::field<uint8_t>(actor,g_refs.playerWet)!=0;
                const int* slots=wet?effect.alternate:effect.normal;
                bl::runtime::field<int32_t>(actor,g_refs.playerHead)=slots[0];
                bl::runtime::field<int32_t>(actor,g_refs.playerBody)=slots[1];
                bl::runtime::field<int32_t>(actor,g_refs.playerLegs)=slots[2];
                break;
            }
        }
    }
    if (g_refs.playerWhoAmI < 0 || g_refs.playerArmor < 0 || g_refs.itemType < 0)
        return;
    const int playerIndex = bl::runtime::field<int32_t>(actor, g_refs.playerWhoAmI);
    if (playerIndex < 0 || playerIndex >= 256) return;
    static std::array<uint32_t, 256> visualDrawCalls{};
    if ((++visualDrawCalls[static_cast<size_t>(playerIndex)] & 7u) != 1u)
        return;
    auto* armor = bl::runtime::field<Il2CppArray*>(actor, g_refs.playerArmor);
    if (!armor || armor->length < 10 || armor->length > 40) return;
    auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
    for (uintptr_t slot = 3; slot < 10; ++slot) {
        Il2CppObject* item = equipped[slot];
        if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
            continue;
        const int itemType = bl::runtime::field<int32_t>(item, g_refs.itemType);
        const auto found = g_accessories.find(itemType);
        if (found == g_accessories.end() || found->second.visualMode == 0)
            continue;
        const auto& visual = found->second;
        if ((visual.visualMode == 1 || visual.visualMode == 3) &&
            visual.nativeEquipSlot > 0 && visual.nativeEquipSlot < 128) {
            const int slotOffset = visual.visualMode == 1
                ? g_refs.itemShieldSlot : g_refs.itemShoeSlot;
            if (slotOffset >= 0)
                bl::runtime::field<int8_t>(item, slotOffset) =
                    static_cast<int8_t>(visual.nativeEquipSlot);
        }
        if (visual.visualMode != 2) continue;
        ManagedVisualCommand command;
        command.kind = visual.visualMode == 2
            ? ManagedVisualKind::PlayerOverlay
            : ManagedVisualKind::EquipmentSprite;
        command.playerIndex = playerIndex;
        command.effectId = itemType;
        command.texture = visual.visualTexture;
        command.scale = visual.visualScale;
        command.opacity = visual.visualOpacity;
        command.color = visual.visualColor;
        command.durationTicks = 24;
        command.style = visual.visualMode;
        postManagedVisualCommand(std::move(command));
    }
}

bool sendPlayerLifecycleMessage(const std::string& message) {
    if (message.empty()) return false;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* say = main ? findMethodBySignature(
        main, parseSignature(
            "void NewText(string newText, byte R, byte G, byte B, bool onlyCurrentPlayer)"))
        : nullptr;
    if (!say || !runtime.string_new || !runtime.runtime_invoke) return false;
    Il2CppString* line = runtime.string_new(message.c_str());
    if (!line) return false;
    uint8_t r = 255, g = 200, b = 0, only = 1;
    void* args[5] = {line, &r, &g, &b, &only};
    Il2CppObject* error = nullptr;
    runtime.runtime_invoke(say, nullptr, args, &error);
    return !error;
}

void hkPlayerSpawn(Il2CppObject* self, int32_t context, const MethodInfo* method) {
    if (g_originalPlayerSpawn) g_originalPlayerSpawn(self, context, method);
    if (!self) return;
    static int spawnDiagnostics = 0;
    if (spawnDiagnostics < 6) {
        ++spawnDiagnostics;
        BL_INFO("csharp BunnyPlayer Spawn: context=%d revive=%d policies=%d callbacks=%zu",
                context, g_reviveSpawnContext, g_playerPoliciesReady ? 1 : 0,
                g_respawnMessages.size());
    }
    if (!g_playerPoliciesReady || context != g_reviveSpawnContext ||
        g_respawnMessages.empty()) return;
    using namespace bl::il2cpp;
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    auto& runtime = api();
    const MethodInfo* getter = main && runtime.class_get_method_from_name
        ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0)
        : nullptr;
    if (!getter || !runtime.runtime_invoke) {
        BL_WARN("csharp BunnyPlayer OnRespawn: getter Main.LocalPlayer indisponivel");
        return;
    }
    Il2CppObject* error = nullptr;
    Il2CppObject* localPlayer = runtime.runtime_invoke(getter, nullptr, nullptr, &error);
    if (error || !localPlayer) {
        BL_WARN("csharp BunnyPlayer OnRespawn: Main.LocalPlayer indisponivel no Spawn");
        return;
    }
    const int32_t who = g_refs.playerWhoAmI >= 0
        ? bl::runtime::field<int32_t>(self, g_refs.playerWhoAmI) : -1;
    const int32_t localWho = g_refs.playerWhoAmI >= 0
        ? bl::runtime::field<int32_t>(localPlayer, g_refs.playerWhoAmI) : -1;
    const bool localMatch = self == localPlayer ||
        (who >= 0 && localWho >= 0 && who == localWho);
    BL_INFO("csharp BunnyPlayer OnRespawn: jogador=%d local=%d corresponde=%d",
            who, localWho, localMatch ? 1 : 0);
    if (!localMatch) return;
    int count = 0;
    for (const auto& [name, message] : g_respawnMessages)
        if (sendPlayerLifecycleMessage(message)) ++count;
    BL_INFO("csharp BunnyPlayer OnRespawn: %d mensagem(ns) enviadas(s)", count);
}

void hkPlayerUpdate(Il2CppObject* self, int32_t index, const MethodInfo* method) {
    if(nativeCostumesActive()) {
        if(auto* cycle=costumeCycle(self)) {
            cycle->previousType=cycle->currentType;
            cycle->currentType=-1;
            cycle->power=false;
        }
    }
    static bool loggedPre = false;
    static bool loggedPost = false;
    if (self && g_playerPoliciesReady && g_preUpdateEnabled && !loggedPre) {
        loggedPre = true;
        BL_INFO("csharp BunnyPlayer PreUpdate: politica nativa executada");
    }
    if (g_originalPlayerUpdate)
        g_originalPlayerUpdate(self, index, method);
    if (self && self == g_dodgePendingPlayer && g_refs.playerLife >= 0) {
        if (g_dodgeLifeSamples < 12) {
            ++g_dodgeLifeSamples;
            BL_INFO("csharp BunnyPlayer FreeDodge: vida apos Update=%d antes da esquiva=%d imune=%d tempo=%d",
                    bl::runtime::field<int32_t>(self, g_refs.playerLife),
                    g_dodgePendingLife,
                    g_refs.playerImmune >= 0
                        ? static_cast<int>(bl::runtime::field<uint8_t>(self, g_refs.playerImmune)) : -1,
                    g_refs.playerImmuneTime >= 0
                        ? bl::runtime::field<int32_t>(self, g_refs.playerImmuneTime) : -1);
        }
        g_dodgePendingPlayer = nullptr;
    }
    if (self && g_deadEventDelivered && g_refs.playerDead >= 0 &&
        !bl::runtime::field<uint8_t>(self, g_refs.playerDead)) {
        using namespace bl::il2cpp;
        auto& runtime = api();
        Il2CppClass* main = findClass({"Terraria", "Main", {}});
        const MethodInfo* getter = main && runtime.class_get_method_from_name
            ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0)
            : nullptr;
        if (getter && runtime.runtime_invoke) {
            Il2CppObject* error = nullptr;
            Il2CppObject* local = runtime.runtime_invoke(getter, nullptr, nullptr, &error);
            if (!error && local == self) {
                g_deadEventDelivered = false;
                BL_INFO("csharp BunnyPlayer UpdateDead: ciclo encerrado; nova morte habilitada");
            }
        }
    }
    if (self && g_playerPoliciesReady && g_postUpdateEnabled && !loggedPost) {
        loggedPost = true;
        BL_INFO("csharp BunnyPlayer PostUpdate: politica nativa executada");
    }
}

void hkPlayerUpdateDead(Il2CppObject* self, const MethodInfo* method) {
    const bool wasDead = self && g_refs.playerDead >= 0 &&
        bl::runtime::field<uint8_t>(self, g_refs.playerDead) != 0;
    if (g_originalUpdateDead) g_originalUpdateDead(self, method);
    if (!wasDead || !g_playerPoliciesReady || g_deadEventDelivered ||
        g_deadMessages.empty()) return;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* getter = main && runtime.class_get_method_from_name
        ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0)
        : nullptr;
    if (!getter || !runtime.runtime_invoke) return;
    Il2CppObject* error = nullptr;
    Il2CppObject* local = runtime.runtime_invoke(getter, nullptr, nullptr, &error);
    if (error || local != self) return;
    int delivered = 0;
    for (const auto& [key, message] : g_deadMessages)
        if (sendPlayerLifecycleMessage(message)) ++delivered;
    if (delivered > 0) g_deadEventDelivered = true;
    BL_INFO("csharp BunnyPlayer UpdateDead: %d mensagem(ns) enviadas(s)", delivered);
}

void snapshotHurtSource(const void* source, int* npcIndex, int* projectileIndex,
                        int* rawNpc, int* rawProjectile) {
    if (!npcIndex || !projectileIndex || !rawNpc || !rawProjectile) return;
    *npcIndex = -1;
    *projectileIndex = -1;
    *rawNpc = -1;
    *rawProjectile = -1;
    if (!source || g_refs.sourceSize <= 0) return;
    const auto* bytes = static_cast<const uint8_t*>(source);
    if (g_refs.sourceNpcOffset >= 0)
        std::memcpy(rawNpc, bytes + g_refs.sourceNpcOffset, sizeof(*rawNpc));
    if (g_refs.sourceProjectileOffset >= 0)
        std::memcpy(rawProjectile, bytes + g_refs.sourceProjectileOffset,
                    sizeof(*rawProjectile));
    if (*rawNpc >= 0 && *rawNpc < 200)
        *npcIndex = *rawNpc;
    else if (*rawProjectile >= 0 && *rawProjectile < 1000)
        *projectileIndex = *rawProjectile;
}

struct HurtVector2 {
    float x = 0.f;
    float y = 0.f;
};
static_assert(sizeof(HurtVector2) == 8);

struct HurtAttackerSnapshot {
    float attackerX = -1.f;
    float attackerY = -1.f;
    float attackerVelocityX = -1.f;
    float attackerVelocityY = -1.f;
    float attackerAngle = -1.f;
    float attackerDistance = -1.f;
    int npcType = -1;
    int npcDamage = -1;
    int npcLife = -1;
    int projectileType = -1;
    int projectileDamage = -1;
    int projectileOwner = -1;
};

bool readHurtMotion(Il2CppObject* attacker, int positionOffset, int velocityOffset,
                    int widthOffset, int heightOffset, HurtAttackerSnapshot* snapshot) {
    if (!attacker || !snapshot || positionOffset < 0 || velocityOffset < 0 ||
        widthOffset < 0 || heightOffset < 0)
        return false;
    const HurtVector2 pos = bl::runtime::field<HurtVector2>(attacker, positionOffset);
    const HurtVector2 speed = bl::runtime::field<HurtVector2>(attacker, velocityOffset);
    const int width = bl::runtime::field<int32_t>(attacker, widthOffset);
    const int height = bl::runtime::field<int32_t>(attacker, heightOffset);
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) ||
        !std::isfinite(speed.x) || !std::isfinite(speed.y) ||
        width < 1 || width > 8192 || height < 1 || height > 8192)
        return false;
    snapshot->attackerX = pos.x + static_cast<float>(width) * 0.5f;
    snapshot->attackerY = pos.y + static_cast<float>(height) * 0.5f;
    snapshot->attackerVelocityX = speed.x;
    snapshot->attackerVelocityY = speed.y;
    return true;
}

HurtAttackerSnapshot snapshotHurtAttacker(int npcIndex, int projectileIndex,
                                         Il2CppObject* player) {
    HurtAttackerSnapshot snapshot;
    auto& runtime = bl::il2cpp::api();
    if (!runtime.field_static_get_value) return snapshot;
    if (npcIndex >= 0 && npcIndex < 200 && g_mainNpcArrayField) {
        Il2CppArray* array = nullptr;
        runtime.field_static_get_value(g_mainNpcArrayField, &array);
        static bool reportedNpcArray = false;
        if (array && !reportedNpcArray) {
            reportedNpcArray = true;
            BL_INFO("csharp hit context: Main.npc tamanho=%zu", static_cast<size_t>(array->length));
        }
        if (array && array->length <= 201 &&
            static_cast<uintptr_t>(npcIndex) < array->length) {
            auto** entries = static_cast<Il2CppObject**>(arrayData(array));
            if (Il2CppObject* attacker = entries[npcIndex]) {
                if (g_refs.npcType >= 0)
                    snapshot.npcType = bl::runtime::field<int32_t>(attacker, g_refs.npcType);
                if (g_refs.npcDamage >= 0)
                    snapshot.npcDamage = bl::runtime::field<int32_t>(attacker, g_refs.npcDamage);
                if (g_refs.npcLife >= 0)
                    snapshot.npcLife = bl::runtime::field<int32_t>(attacker, g_refs.npcLife);
                readHurtMotion(attacker, g_refs.npcPosition, g_refs.npcVelocity,
                               g_refs.npcWidth, g_refs.npcHeight, &snapshot);
            }
        }
    } else if (projectileIndex >= 0 && projectileIndex < 1000 &&
               g_mainProjectileArrayField) {
        Il2CppArray* array = nullptr;
        runtime.field_static_get_value(g_mainProjectileArrayField, &array);
        static bool reportedProjectileArray = false;
        if (array && !reportedProjectileArray) {
            reportedProjectileArray = true;
            BL_INFO("csharp hit context: Main.projectile tamanho=%zu", static_cast<size_t>(array->length));
        }
        if (array && array->length <= 1001 &&
            static_cast<uintptr_t>(projectileIndex) < array->length) {
            auto** entries = static_cast<Il2CppObject**>(arrayData(array));
            if (Il2CppObject* attacker = entries[projectileIndex]) {
                if (g_refs.projectileType >= 0)
                    snapshot.projectileType = bl::runtime::field<int32_t>(
                        attacker, g_refs.projectileType);
                if (g_refs.projectileDamage >= 0)
                    snapshot.projectileDamage = bl::runtime::field<int32_t>(
                        attacker, g_refs.projectileDamage);
                if (g_refs.projectileOwner >= 0)
                    snapshot.projectileOwner = bl::runtime::field<int32_t>(
                        attacker, g_refs.projectileOwner);
                readHurtMotion(attacker, g_refs.projectilePosition,
                               g_refs.projectileVelocity, g_refs.projWidth,
                               g_refs.projHeight, &snapshot);
            }
        }
    }
    if (player && g_refs.playerPosition >= 0 &&
        g_refs.playerWidth >= 0 && g_refs.playerHeight >= 0 &&
        snapshot.attackerX >= 0.f && snapshot.attackerY >= 0.f) {
        const HurtVector2 pos = bl::runtime::field<HurtVector2>(
            player, g_refs.playerPosition);
        const int width = bl::runtime::field<int32_t>(player, g_refs.playerWidth);
        const int height = bl::runtime::field<int32_t>(player, g_refs.playerHeight);
        if (std::isfinite(pos.x) && std::isfinite(pos.y) &&
            width > 0 && width <= 8192 && height > 0 && height <= 8192) {
            const float dx = pos.x + static_cast<float>(width) * 0.5f -
                             snapshot.attackerX;
            const float dy = pos.y + static_cast<float>(height) * 0.5f -
                             snapshot.attackerY;
            const float distance = std::hypot(dx, dy);
            if (std::isfinite(distance) && distance <= 1000000.f) {
                snapshot.attackerDistance = distance;
                snapshot.attackerAngle = distance > 0.001f
                    ? std::atan2(dy, dx) : 0.f;
            }
        }
    }
    return snapshot;
}

double hkPlayerHurt(Il2CppObject* self, const void* source, int32_t damage,
                    int32_t direction, uint8_t pvp, uint8_t quiet, uint8_t crit,
                    int32_t cooldown, uint8_t dodgeable, const MethodInfo* method) {
    if (!g_originalPlayerHurt) return 0.0;
    static thread_local bool dispatching = false;
    if (dispatching || !self || !g_playerPoliciesReady ||
        g_refs.playerLife < 0 || g_refs.playerLifeMax < 0)
        return g_originalPlayerHurt(self, source, damage, direction,
                                    pvp, quiet, crit, cooldown, dodgeable, method);
    std::vector<ManagedHurtBinding> mods;
    {
        std::lock_guard<std::mutex> lock(g_hurtBindingsMutex);
        for (const auto& [id, binding] : g_hurtBindings)
            if (binding.active && !binding.accessoryTypes.empty())
                mods.push_back(binding);
    }
    if (mods.empty())
        return g_originalPlayerHurt(self, source, damage, direction,
                                    pvp, quiet, crit, cooldown, dodgeable, method);
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* localGetter = main && runtime.class_get_method_from_name
        ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0) : nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* local = localGetter && runtime.runtime_invoke
        ? runtime.runtime_invoke(localGetter, nullptr, nullptr, &error) : nullptr;
    if (error || local != self)
        return g_originalPlayerHurt(self, source, damage, direction,
                                    pvp, quiet, crit, cooldown, dodgeable, method);
    dispatching = true;
    struct DispatchGuard {
        bool& running;
        ~DispatchGuard() { running = false; }
    } guard{dispatching};
    struct HurtParticipant {
        std::string modId;
        int equipmentMask;
    };
    std::vector<HurtParticipant> participants;
    auto* equipped = g_refs.playerArmor >= 0
        ? bl::runtime::field<Il2CppArray*>(self, g_refs.playerArmor) : nullptr;
    auto** items = equipped && equipped->length >= 10 && equipped->length <= 40
        ? static_cast<Il2CppObject**>(arrayData(equipped)) : nullptr;
    int flags = (pvp ? 1 : 0) | (quiet ? 2 : 0) |
                (crit ? 4 : 0) | (dodgeable ? 8 : 0);
    const int sourceDamage = damage;
    const int life = bl::runtime::field<int32_t>(self, g_refs.playerLife);
    const int lifeMax = bl::runtime::field<int32_t>(self, g_refs.playerLifeMax);
    int npcSource = -1;
    int projectileSource = -1;
    int rawNpc = -1;
    int rawProjectile = -1;
    snapshotHurtSource(source, &npcSource, &projectileSource,
                       &rawNpc, &rawProjectile);
    const HurtAttackerSnapshot attacker = snapshotHurtAttacker(npcSource, projectileSource, self);
    static int contextNpcLogs = 0;
    static int contextProjectileLogs = 0;
    if (npcSource >= 0 && contextNpcLogs++ < 8)
        BL_INFO("csharp hit context NPC: index=%d tipo=%d dano=%d vida=%d",
                npcSource, attacker.npcType, attacker.npcDamage, attacker.npcLife);
    else if (projectileSource >= 0 && contextProjectileLogs++ < 8)
        BL_INFO("csharp hit context projectile: index=%d tipo=%d dano=%d dono=%d",
                projectileSource, attacker.projectileType,
                attacker.projectileDamage, attacker.projectileOwner);
    static int spatialNpcLogs = 0;
    static int spatialProjectileLogs = 0;
    if (npcSource >= 0 && spatialNpcLogs++ < 8)
        BL_INFO("csharp hit spatial NPC: x=%.1f y=%.1f vx=%.1f vy=%.1f ang=%.3f distancia=%.1f",
                attacker.attackerX, attacker.attackerY,
                attacker.attackerVelocityX, attacker.attackerVelocityY,
                attacker.attackerAngle, attacker.attackerDistance);
    else if (projectileSource >= 0 && spatialProjectileLogs++ < 8)
        BL_INFO("csharp hit spatial Projectile: x=%.1f y=%.1f vx=%.1f vy=%.1f ang=%.3f distancia=%.1f",
                attacker.attackerX, attacker.attackerY,
                attacker.attackerVelocityX, attacker.attackerVelocityY,
                attacker.attackerAngle, attacker.attackerDistance);
    static int npcCaptures = 0;
    static int projectileCaptures = 0;
    static int unknownCaptures = 0;
    int& sourceLogs = npcSource >= 0 ? npcCaptures :
                      projectileSource >= 0 ? projectileCaptures : unknownCaptures;
    const bool logSource = sourceLogs++ < 8;
    if (logSource)
        BL_INFO("csharp BunnyPlayer hit diag: Player.Hurt capturou rawNpc=%d rawProjectile=%d npc=%d projectile=%d origem=%s dano=%d",
                rawNpc, rawProjectile, npcSource, projectileSource,
                npcSource >= 0 ? "NPC" : projectileSource >= 0 ? "Projectile" : "Unknown",
                sourceDamage);
    int changed = 0;
    for (const auto& binding : mods) {
        int mask = 0;
        for (size_t i = 0; i < binding.accessoryTypes.size() && i < 16; ++i) {
            const int target = binding.accessoryTypes[i];
            if (target < 0) continue;
            if (!items) continue;
            for (uintptr_t slot = 3; slot < 10; ++slot) {
                Il2CppObject* item = items[slot];
                if (item && bl::runtime::field<int32_t>(item, g_refs.itemType) == target &&
                    bl::runtime::field<uint8_t>(item, g_refs.itemAccessory)) {
                    mask |= 1 << i;
                    break;
                }
            }
        }
        participants.push_back({binding.modId, mask});
        ManagedHurtResult result;
        if (!modifyManagedPlayerHurt(binding.modId, damage, direction, flags,
                                     life, lifeMax, mask, npcSource, projectileSource,
                                     attacker.npcType, attacker.npcDamage, attacker.npcLife,
                                     attacker.projectileType, attacker.projectileDamage,
                                     attacker.projectileOwner,
                                     attacker.attackerX, attacker.attackerY,
                                     attacker.attackerVelocityX, attacker.attackerVelocityY,
                                     attacker.attackerAngle, attacker.attackerDistance,
                                     &result)) continue;
        ++changed;
        damage = result.damage;
        direction = result.hitDirection;
        flags = (flags & 1) | (result.flags & 14);
        if (result.cancelled) {
            if (result.dodged) {
                const int lifeBeforeDodge = bl::runtime::field<int32_t>(self, g_refs.playerLife);
                int immunityFrames = 0;
                if (g_refs.playerImmune >= 0 && g_refs.playerImmuneTime >= 0) {
                    bl::runtime::field<uint8_t>(self, g_refs.playerImmune) = 1;
                    auto& immunity = bl::runtime::field<int32_t>(self, g_refs.playerImmuneTime);
                    immunity = std::max(immunity, 40);
                    immunityFrames = immunity;
                    if (g_refs.playerHurtCooldowns >= 0) {
                        auto* cooldowns = bl::runtime::field<Il2CppArray*>(
                            self, g_refs.playerHurtCooldowns);
                        if (cooldowns && cooldowns->length >= 2 && cooldowns->length <= 8) {
                            auto* timers = static_cast<int32_t*>(arrayData(cooldowns));
                            for (uintptr_t i = 0; i < cooldowns->length; ++i)
                                timers[i] = std::max(timers[i], 40);
                        }
                    }
                } else {
                    BL_WARN("csharp BunnyPlayer FreeDodge: imunidade nao disponivel no Player; esquiva sem invulnerabilidade");
                }
                g_dodgePendingPlayer = self;
                g_dodgePendingLife = lifeBeforeDodge;
                BL_INFO("csharp BunnyPlayer FreeDodge: esquiva C# entrada=%d apos ModifyHurt=%d vida=%d->%d imuneTicks=%d",
                        sourceDamage, damage, lifeBeforeDodge,
                        bl::runtime::field<int32_t>(self, g_refs.playerLife),
                        immunityFrames);
            } else {
                BL_INFO("csharp BunnyPlayer ModifyHurt: golpe cancelado");
            }
            return 0.0;
        }
    }
    static int diagnosed = 0;
    if (changed && diagnosed++ < 12)
        BL_INFO("csharp BunnyPlayer ModifyHurt: entrada=%d final=%d callbacks=%d",
                sourceDamage, damage, changed);
    static int npcModifiedLogs = 0;
    static int projectileModifiedLogs = 0;
    if (changed && npcSource >= 0 && npcModifiedLogs++ < 8)
        BL_INFO("csharp BunnyPlayer ModifyHitByNPC: npc=%d entrada=%d final=%d callbacks=%d",
                npcSource, sourceDamage, damage, changed);
    else if (changed && projectileSource >= 0 && projectileModifiedLogs++ < 8)
        BL_INFO("csharp BunnyPlayer ModifyHitByProjectile: projectile=%d entrada=%d final=%d callbacks=%d",
                projectileSource, sourceDamage, damage, changed);
    const double applied = g_originalPlayerHurt(
        self, source, damage, direction, pvp,
        (flags & 2) != 0, (flags & 4) != 0, cooldown, (flags & 8) != 0, method);
    if (applied > 0.0 && applied <= 1000000.0) {
        const int actual = static_cast<int>(applied);
        const int remaining = std::max(
            0, bl::runtime::field<int32_t>(self, g_refs.playerLife));
        for (const auto& participant : participants) {
            const bool queued = notifyManagedPlayerHurt(
                participant.modId, sourceDamage, actual, direction, flags,
                remaining, participant.equipmentMask, npcSource, projectileSource,
                attacker.npcType, attacker.npcDamage, attacker.npcLife,
                attacker.projectileType, attacker.projectileDamage,
                attacker.projectileOwner,
                attacker.attackerX, attacker.attackerY,
                attacker.attackerVelocityX, attacker.attackerVelocityY,
                attacker.attackerAngle, attacker.attackerDistance);
            if (logSource)
                BL_INFO("csharp BunnyPlayer hit diag: evento OnHurt fila=%s npc=%d projectile=%d dano=%d vida=%d",
                        queued ? "aceito" : "recusado",
                        npcSource, projectileSource, actual, remaining);
        }
    }
    return applied;
}

int equippedRegeneration(Il2CppObject* self, bool mana) {
    if (!self || g_refs.playerArmor < 0 || g_refs.itemAccessory < 0 ||
        g_refs.itemType < 0) return 0;
    auto* armor = bl::runtime::field<Il2CppArray*>(self, g_refs.playerArmor);
    if (!armor || armor->length < 10 || armor->length > 40) return 0;
    auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
    int sum = 0;
    for (uintptr_t slot = 3; slot < 10; ++slot) {
        Il2CppObject* item = equipped[slot];
        if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
            continue;
        const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
        auto it = g_accessories.find(type);
        if (it == g_accessories.end()) continue;
        sum += mana ? it->second.manaRegenBonus : it->second.lifeRegenBonus;
    }
    return sum < 0 ? 0 : (sum > 100 ? 100 : sum);
}

void hkPlayerLifeRegen(Il2CppObject* self, const MethodInfo* method) {
    if (g_originalLifeRegen) g_originalLifeRegen(self, method);
    if (!self || !g_playerPoliciesReady || !g_anyLifeRegen ||
        g_refs.playerLifeRegen < 0) return;
    const int bonus = equippedRegeneration(self, false);
    if (!bonus) return;
    bl::runtime::field<int32_t>(self, g_refs.playerLifeRegen) += bonus;
    static int logged = 0;
    if (logged++ < 2)
        BL_INFO("csharp BunnyPlayer UpdateLifeRegen: bonus nativo +%d", bonus);
}

void hkPlayerManaRegen(Il2CppObject* self, const MethodInfo* method) {
    if (g_originalManaRegen) g_originalManaRegen(self, method);
    if (!self || !g_playerPoliciesReady || !g_anyManaRegen ||
        g_refs.playerManaRegen < 0) return;
    const int bonus = equippedRegeneration(self, true);
    if (!bonus) return;
    bl::runtime::field<int32_t>(self, g_refs.playerManaRegen) += bonus;
    static int logged = 0;
    if (logged++ < 2)
        BL_INFO("csharp BunnyPlayer UpdateManaRegen: bonus nativo +%d", bonus);
}

void hkBuffEquipment(Il2CppObject* self,int32_t playerIndex,const MethodInfo* method) {
    if(g_originalBuffEquip)g_originalBuffEquip(self,playerIndex,method);
    if(!self)return;
    const auto buffs=std::atomic_load_explicit(&g_buffView,std::memory_order_acquire);
    if(buffs->empty())return;
    auto* types=runtime::field<Il2CppArray*>(self,g_refs.playerBuffType);
    auto* times=runtime::field<Il2CppArray*>(self,g_refs.playerBuffTime);
    if(!nativeArrayContract(types,"Int32",4,60)||!nativeArrayContract(times,"Int32",4,60))return;
    const auto* ids=static_cast<const int32_t*>(arrayData(types));const auto* ticks=static_cast<const int32_t*>(arrayData(times));
    auto& defense=runtime::field<int32_t>(self,g_refs.playerStatDefense);
    for(size_t n=0;n<std::min(types->length,times->length);++n) {
        if(ticks[n]<=0)continue;
        const auto it=buffs->find(ids[n]);
        if(it!=buffs->end()&&it->second.defenseMultiplierPercent!=100)
            defense=static_cast<int32_t>((static_cast<int64_t>(defense)*it->second.defenseMultiplierPercent)/100);
    }
}
void hkUpdateBuffs(Il2CppObject* self, int32_t playerIndex, const MethodInfo* method) {
    if (g_originalUpdateBuffs) g_originalUpdateBuffs(self, playerIndex, method);
    if (!self) return;
    const auto buffs=std::atomic_load_explicit(&g_buffView,std::memory_order_acquire);
    if(buffs->empty())return;
    auto* types=bl::runtime::field<Il2CppArray*>(self,g_refs.playerBuffType);
    auto* times=bl::runtime::field<Il2CppArray*>(self,g_refs.playerBuffTime);
    if(!nativeArrayContract(types,"Int32",4,60)||!nativeArrayContract(times,"Int32",4,60))return;
    const auto count=std::min(types->length,times->length);
    auto* typeValues=static_cast<int32_t*>(arrayData(types));auto* timeValues=static_cast<int32_t*>(arrayData(times));
    // i is a PLAYER index, not a buff slot. Reverse traversal tolerates DelBuff shifting entries left.
    visitManagedBuffSlots(typeValues,timeValues,count,[&](size_t slot) {
        const auto it=buffs->find(typeValues[slot]);if(it==buffs->end())return;
        bl::runtime::field<int32_t>(self,g_refs.playerStatDefense)+=it->second.defenseBonus;
        if(it->second.minionProjectileType<0||g_refs.playerOwnedProjectileCounts<0)return;
        int32_t owned=-1;
        auto* counts=bl::runtime::field<Il2CppArray*>(self,g_refs.playerOwnedProjectileCounts);
        const auto type=static_cast<uintptr_t>(it->second.minionProjectileType);
        if(nativeArrayContract(counts,"Int32",4,65536)&&type<counts->length)owned=static_cast<int32_t*>(arrayData(counts))[type];
        switch(decideMinionBuff(owned,timeValues[slot])) {
        case MinionBuffAction::Refresh:timeValues[slot]=kMinionBuffTicks;break;
        case MinionBuffAction::Remove:
            if(g_removeMinionBuff){int32_t removeIndex=static_cast<int32_t>(slot);void* args[]={&removeIndex};Il2CppObject* error=nullptr;
                il2cpp::api().runtime_invoke(g_removeMinionBuff,self,args,&error);
                if(error){static unsigned failures=0;if(failures++<3)BL_ERROR("[CSharp][ERROR][Buff] ContentId=%d Key=%s Stage=RemoveMinionBuff Method=Player.DelBuff Slot=%d Reason=NativeManagedException Result=NotRemoved Exception=%s",typeValues[slot],runtime::modBuffKey(typeValues[slot]).c_str(),removeIndex,il2cpp::describeException(error).c_str());}}
            break;
        case MinionBuffAction::Keep:break;
        }
    });
    if(g_costumeReady&&nativeCostumesActive()) {
        auto* cycle=costumeCycle(self);
        if(cycle&&cycle->previousType>=0) {
            NativeCostumeEffect costume;
            if(nativeCostumeEffect(cycle->previousType,&costume)) {
                const bool enabled=costumeTownCount(self)>=costume.requiredTownNpcs &&
                    costumeEquipped(self,cycle->previousType,false);
                visitManagedBuffSlots(typeValues,timeValues,count,[&](size_t slot) {
                    if(typeValues[slot]!=costume.buffType)return;
                    if(enabled) {
                        bl::runtime::field<float>(self,g_refs.playerJumpSpeedBoost)+=costume.jumpSpeedBoost;
                        bl::runtime::field<int32_t>(self,g_refs.playerExtraFall)+=costume.extraFall;
                        cycle->power=true;
                    } else if(g_removeMinionBuff) {
                        int32_t index=static_cast<int32_t>(slot);
                        void* args[]={&index};Il2CppObject* error=nullptr;
                        il2cpp::api().runtime_invoke(g_removeMinionBuff,self,args,&error);
                        if(error){static unsigned failures=0;if(failures++<3)
                            BL_ERROR("[CSharp][Costume] Stage=DelBuff Result=Failed Detail=%s",
                                il2cpp::describeException(error).c_str());}
                    }
                });
            }
        }
    }
}

}

bool nativeCostumeRuntimeReady() {return g_costumeReady;}

std::string nativeManagedProjectileKey(int type) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto it=view->find(type);return it==view->end()?std::string{}:it->second.key;
}
SummonMovement nativeManagedMinionMovement(int type) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto it=view->find(type);return it==view->end()?SummonMovement{}:it->second.movement;
}
ProjectilePolicy nativeManagedProjectilePolicy(int type) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto found=view->find(type);if(found==view->end())return {};
    const auto& d=found->second.defaults;
    return {d.frames,d.animationFrameTicks,d.minionScanTicks,d.minionTargetRange,d.minion};
}
bool nativeManagedProjectileFields(int type,ProjectileGameplayFields* fields) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto found=view->find(type);if(found==view->end())return false;
    *fields=found->second.fields;return true;
}

ManagedTileCollisionSettings nativeManagedProjectileTileCollisionSettings(int type) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto found=view->find(type);
    if(found==view->end())return {};
    const auto& defaults=found->second.defaults;
    return {defaults.tileCollisionMode,defaults.bounceRestitution,
            defaults.bounceMinimumSpeed,defaults.tileCollisionSurvivalLimit,
            defaults.tilePlatformCollision,
            defaults.tileCollisionBoxWidth,defaults.tileCollisionBoxHeight,
            defaults.tileCollisionAnchorOffsetX,defaults.tileCollisionAnchorOffsetY};
}
int nativeManagedProjectileFrames(int type) {return nativeManagedProjectilePolicy(type).frames;}

int nativeManagedMinionBuff(int projectileType) {
    const auto buffs=std::atomic_load_explicit(&g_buffView,std::memory_order_acquire);
    for(const auto& [type,buff]:*buffs)if(buff.minionProjectileType==projectileType)return type;
    const auto weapons=std::atomic_load_explicit(&g_weaponView,std::memory_order_acquire);
    for (const auto& [type, weapon] : *weapons) {
        if (weapon.summon && weapon.shootType == projectileType)
            return weapon.minionBuffType;
    }
    return -1;
}

bool nativeManagedMinionContactEnabled(int projectileType) {
    const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
    const auto it = view->find(projectileType);
    return it != view->end() && it->second.defaults.minion && it->second.defaults.minionContactDamage;
}


bool prepareNativeContent() {
    if (g_ready) return true;
    using namespace bl::il2cpp;
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    if (!item || !projectile || !npc || !player) return false;

    NativeRefs r{};
    r.itemDamage = fieldOffset(item, "damage");
    r.itemUseTime = fieldOffset(item, "useTime");
    r.itemUseAnimation = fieldOffset(item, "useAnimation");
    r.itemUseStyle = fieldOffset(item, "useStyle");
    r.itemShoot = fieldOffset(item, "shoot");
    r.itemShootSpeed = fieldOffset(item, "shootSpeed");
    r.itemKnockBack = fieldOffset(item, "knockBack");
    r.itemRanged = fieldOffset(item, "ranged");
    r.itemSummon = fieldOffset(item, "summon");
    r.itemNoMelee = fieldOffset(item, "noMelee");
    r.itemAutoReuse = fieldOffset(item, "autoReuse");
    r.itemRare = fieldOffset(item, "rare");
    r.itemMaxStack = fieldOffset(item, "maxStack");
    r.projAiStyle = fieldOffset(projectile, "aiStyle");
    r.projFriendly = fieldOffset(projectile, "friendly");
    r.projHostile = fieldOffset(projectile, "hostile");
    r.projRanged = fieldOffset(projectile, "ranged");
    r.projMinion = fieldOffset(projectile, "minion");
    r.projMinionSlots = fieldOffset(projectile, "minionSlots");
    r.projPenetrate = fieldOffset(projectile, "penetrate");
    r.projTimeLeft = fieldOffset(projectile, "timeLeft");
    r.projIgnoreWater = fieldOffset(projectile, "ignoreWater");
    r.projTileCollide = fieldOffset(projectile, "tileCollide");
    r.projWidth = fieldOffset(projectile, "width");
    r.projHeight = fieldOffset(projectile, "height");
    r.itemAccessory = fieldOffset(item, "accessory");
    r.itemShieldSlot = fieldOffset(item, "shieldSlot");
    r.itemShoeSlot = fieldOffset(item, "shoeSlot");
    r.playerHellfireTreads = fieldOffset(player, "hellfireTreads");
    r.playerHideVisibleAccessory = fieldOffset(player, "hideVisibleAccessory");
    r.playerVelocity = fieldOffset(player, "velocity");
    r.playerMount = fieldOffset(player, "mount");
    r.itemBuffType = fieldOffset(item, "buffType");
    r.itemBuffTime = fieldOffset(item, "buffTime");
    r.itemConsumable = fieldOffset(item, "consumable");
    r.itemType = fieldOffset(item, "type");
    r.npcWidth = fieldOffset(npc, "width");
    r.npcHeight = fieldOffset(npc, "height");
    r.npcDamage = fieldOffset(npc, "damage");
    r.npcDefense = fieldOffset(npc, "defense");
    r.npcLifeMax = fieldOffset(npc, "lifeMax");
    r.npcAiStyle = fieldOffset(npc, "aiStyle");
    r.npcType = fieldOffset(npc, "type");
    r.npcLife = fieldOffset(npc, "life");
    r.npcPosition = fieldOffset(npc, "position");
    r.npcVelocity = fieldOffset(npc, "velocity");
    r.projectilePosition = fieldOffset(projectile, "position");
    r.projectileVelocity = fieldOffset(projectile, "velocity");
    r.playerPosition = fieldOffset(player, "position");
    r.playerWidth = fieldOffset(player, "width");
    r.playerHeight = fieldOffset(player, "height");
    r.projectileType = fieldOffset(projectile, "type");
    r.projectileDamage = fieldOffset(projectile, "damage");
    r.projectileOwner = fieldOffset(projectile, "owner");
    r.playerStatDefense = fieldOffset(player, "statDefense");
    r.playerMoveSpeed = fieldOffset(player, "moveSpeed");
    r.playerArmor = fieldOffset(player, "armor");
    r.playerMaxRunSpeed = fieldOffset(player, "maxRunSpeed");
    r.playerRunAcceleration = fieldOffset(player, "runAcceleration");
    r.playerFrameShadow = fieldOffset(player, "armorEffectDrawShadow");
    r.playerFrameOutline = fieldOffset(player, "armorEffectDrawOutlines");
    r.playerWhoAmI = fieldOffset(player, "whoAmI");
    r.playerLifeRegen = fieldOffset(player, "lifeRegen");
    r.playerManaRegen = fieldOffset(player, "manaRegen");
    r.playerDead = fieldOffset(player, "dead");
    r.playerLife = fieldOffset(player, "statLife");
    r.playerImmune = fieldOffset(player, "immune");
    r.playerImmuneTime = fieldOffset(player, "immuneTime");
    r.playerHurtCooldowns = fieldOffset(player, "hurtCooldowns");
    r.playerLifeMax = fieldOffset(player, "statLifeMax2");
    if (r.playerLifeMax < 0) r.playerLifeMax = fieldOffset(player, "statLifeMax");
    std::string buffFieldError;
    if(!nativeFieldContract(player,"buffType","System.Int32[]",false,sizeof(void*),nullptr,&r.playerBuffType,&buffFieldError)||
       !nativeFieldContract(player,"buffTime","System.Int32[]",false,sizeof(void*),nullptr,&r.playerBuffTime,&buffFieldError)||
       !nativeFieldContract(player,"statDefense","System.Int32",false,4,nullptr,&r.playerStatDefense,&buffFieldError)) {
        BL_ERROR("[CSharp][ERROR][Buff] API=BunnyBuff Stage=BindPlayerFields Result=Rejected Detail=%s",buffFieldError.c_str());return false;
    }
    g_removeMinionBuff=il2cpp::findMethodBySignature(player,il2cpp::parseSignature("void DelBuff(int b)"));
    if(g_removeMinionBuff&&!il2cpp::api().method_is_instance(g_removeMinionBuff))g_removeMinionBuff=nullptr;
    if(!nativeFieldContract(player,"ownedProjectileCounts","System.Int32[]",false,sizeof(void*),nullptr,&r.playerOwnedProjectileCounts,&g_minionBindError))
        r.playerOwnedProjectileCounts=-1;

    const int* all = &r.itemDamage;
    for (size_t i = 0; i < offsetof(NativeRefs, itemAccessory) / sizeof(int); ++i) {
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
    auto* mainClass = findClass({"Terraria", "Main", {}});
    auto* projectileSets = findClass({"Terraria.ID", "ProjectileID", "Sets"});
    std::string flagContract;
    if(!nativeFieldContract(mainClass,"projPet","System.Boolean[]",true,sizeof(void*),&g_nativeMinionProjPet,nullptr,&flagContract)||
       !nativeFieldContract(projectileSets,"MinionSacrificable","System.Boolean[]",true,sizeof(void*),&g_nativeMinionSacrificable,nullptr,&flagContract)||
       !nativeFieldContract(projectileSets,"MinionTargetingFeature","System.Boolean[]",true,sizeof(void*),&g_nativeMinionTargeting,nullptr,&flagContract)) {
        BL_ERROR("[CSharp][ERROR][Summon] API=BunnyProjectile.Minion Stage=BindNativeSets Result=Rejected Detail=%s",flagContract.c_str());return false;
    }
    const MethodInfo* damageMethod = findMethodBySignature(
        projectile, parseSignature("void Damage()"));
    if (!g_nativeMinionProjPet || !damageMethod || r.projectileType < 0 ||
        r.projectileDamage < 0) {
        BL_ERROR("csharp summon: native Projectile.Damage/projPet contract unavailable");
        return false;
    }

    auto* statsMethod=findMethodBySignature(projectile,parseSignature("void ApplyStatsFromSource(IEntitySource spawnSource)"));
    std::string damageContract;FieldInfo* originalDamageField=nullptr;
    if(!statsMethod||!api().method_is_instance(statsMethod)||
       !nativeFieldContract(projectile,"originalDamage","System.Int32",false,4,&originalDamageField,&g_originalDamageField,&damageContract)||
       !nativeFieldContract(item,"damage","System.Int32",false,4,nullptr,&r.itemDamage,&damageContract)) {
        BL_ERROR("[CSharp][ERROR][ProjectileSource] API=BunnyProjectile Stage=BindStatsSource Expected=InstanceApplyStatsFromSourceAndInt32DamageFields Result=Rejected Detail=%s",damageContract.c_str());return false;
    }
    if(api().field_get_flags(originalDamageField)&0x20u) {
        BL_ERROR("[CSharp][ERROR][ProjectileSource] Field=originalDamage Stage=BindStatsSource Expected=WritableInstanceInt32 Actual=InitOnly Result=Rejected");return false;
    }
    g_refs = r;
    if(!g_originalApplyProjectileStats&&!bl::hook::install(statsMethod,hkProjectileStatsFromSource,&g_originalApplyProjectileStats))return false;
    if (!g_originalItem&&!bl::hook::install(itemMethod, hkItemDefaults, &g_originalItem)) return false;
    if (!g_originalProjectile&&!bl::hook::install(projectileMethod, hkProjectileDefaults, &g_originalProjectile)) return false;
    if (!g_originalProjectileDamage&&!bl::hook::install(damageMethod, hkNativeMinionDamage, &g_originalProjectileDamage))
        return false;

    g_ready = true;
    BL_INFO("csharp native content: hooks C++ de item e projetil instalados");
    return true;
}

bool prepareNativeNpc() {
    if (g_npcReady) return true;
    using namespace bl::il2cpp;
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    if (!npc || g_refs.npcWidth < 0 || g_refs.npcHeight < 0 ||
        g_refs.npcDamage < 0 || g_refs.npcDefense < 0 ||
        g_refs.npcLifeMax < 0 || g_refs.npcAiStyle < 0) return false;
    const MethodInfo* method = findMethodBySignature(
        npc, parseSignature("void SetDefaults(int Type, NPCSpawnParams spawnparams)"));
    if (!method) return false;
    const Il2CppType* p = api().method_get_param(method, 1);
    Il2CppClass* cls = p ? api().class_from_il2cpp_type(p) : nullptr;
    uint32_t align = 0;
    const int size = cls ? api().class_value_size(cls, &align) : 0;
    if (size <= 16 || size > 1024) {
        BL_ERROR("csharp native: NPCSpawnParams ABI nao verificada (size=%d)", size);
        return false;
    }
    if (!bl::hook::install(method, hkNpcDefaults, &g_originalNpc)) return false;
    g_npcReady = true;
    BL_INFO("csharp native: NPC defaults registrado com ABI por referencia (size=%d)", size);
    return true;
}

void hurtDiagnosticToEditor(const char* line) {
    if (line && *line) BL_INFO("%s", line);
}

bool prepareNativePlayerEffects() {
    if (g_effectsReady) return true;
    setManagedHurtDiagnosticLogger(hurtDiagnosticToEditor);
    using namespace bl::il2cpp;
    const auto& r = g_refs;
    if (r.playerStatDefense < 0 || r.playerMoveSpeed < 0 ||
        r.playerArmor < 0 || r.playerMaxRunSpeed < 0 ||
        r.playerRunAcceleration < 0 || r.playerFrameShadow < 0 ||
        r.playerFrameOutline < 0 ||
        r.playerBuffType < 0 ||
        r.playerBuffTime < 0 || r.itemType < 0 ||
        r.itemAccessory < 0 || r.itemBuffType < 0 ||
        r.itemBuffTime < 0 || r.itemConsumable < 0) return false;
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    if (!player) return false;
    const MethodInfo* equip = findMethodBySignature(
        player, parseSignature("void ApplyEquipFunctional(int itemSlot, Item currentItem)"));
    const MethodInfo* buffs = findMethodBySignature(
        player, parseSignature("void UpdateBuffs(int i)"));
    const MethodInfo* runSpeeds = findMethodBySignature(
        player, parseSignature("void HorizontalMovement()"));
    const MethodInfo* armorVisuals = findMethodBySignature(
        player, parseSignature("void SetArmorEffectVisuals(Player drawPlayer)"));
    if (!equip || !buffs || !runSpeeds || !armorVisuals) return false;

    const auto costumeField=[&](const char* name,const char* type,size_t size,int* offset) {
        std::string detail;
        return nativeFieldContract(player,name,type,false,size,nullptr,offset,&detail);
    };
    g_costumeAddBuff=findMethodBySignature(
        player,parseSignature("void AddBuff(int type, int time, bool fromNetPvP)"));
    g_costumeReady=g_costumeAddBuff&&api().method_is_instance&&
        api().method_is_instance(g_costumeAddBuff)&&
        g_removeMinionBuff&&api().runtime_invoke&&
        g_refs.playerHideVisibleAccessory>=0&&
        costumeField("townNPCs","System.Single",4,&g_refs.playerTownNpcs)&&
        costumeField("jumpSpeedBoost","System.Single",4,&g_refs.playerJumpSpeedBoost)&&
        costumeField("extraFall","System.Int32",4,&g_refs.playerExtraFall)&&
        costumeField("wet","System.Boolean",1,&g_refs.playerWet)&&
        costumeField("head","System.Int32",4,&g_refs.playerHead)&&
        costumeField("body","System.Int32",4,&g_refs.playerBody)&&
        costumeField("legs","System.Int32",4,&g_refs.playerLegs);
    if (!bl::hook::install(equip, hkApplyEquip, &g_originalEquip)) return false;
    const MethodInfo* vanity = findMethodBySignature(
        player, parseSignature("void ApplyEquipVanity(int itemSlot, Item currentItem)"));
    if (!vanity || !bl::hook::install(vanity, hkApplyEquipVanity, &g_originalEquipVanity))
        BL_WARN("csharp equip: hook de vaidade indisponivel");
    g_cancelBootVisuals = findMethodBySignature(
        player, parseSignature("void CancelAllBootRunVisualEffects()"));
    g_placeBootFlames = findMethodBySignature(
        player, parseSignature("bool DoBootsEffect_PlaceFlamesOnTile(int X, int Y)"));
    auto* mountClass = findClass({"Terraria", "Mount", {}});
    if (mountClass) g_mountActive = findMethodBySignature(mountClass, parseSignature("bool get_Active()"));
    if (!g_cancelBootVisuals || !g_placeBootFlames || !g_mountActive ||
        g_refs.playerHideVisibleAccessory < 0 || g_refs.playerVelocity < 0 || g_refs.playerMount < 0)
        BL_WARN("csharp boots: contrato nativo incompleto cancel=%d flames=%d mount=%d hide=%d velocity=%d",
            !!g_cancelBootVisuals, !!g_placeBootFlames, !!g_mountActive,
            g_refs.playerHideVisibleAccessory, g_refs.playerVelocity);
    else BL_INFO("csharp boots: cancelamento e chamas nativas prontos");
    if (!bl::hook::install(buffs, hkUpdateBuffs, &g_originalUpdateBuffs)) return false;
    if (!bl::hook::install(runSpeeds, hkHorizontalMovement,
                           &g_originalHorizontalMovement)) return false;
    if (!bl::hook::install(armorVisuals, hkSetArmorEffectVisuals,
                           &g_originalSetArmorEffectVisuals)) return false;
    const MethodInfo* spawn = findMethodBySignature(
        player, parseSignature("void Spawn(PlayerSpawnContext context)"));
    if (!spawn)
        spawn = findMethodBySignature(
            player, parseSignature("void Spawn(PlayerSpawnContext)"));
    if (spawn) {
        if (!bl::hook::install(spawn, hkPlayerSpawn, &g_originalPlayerSpawn))
            BL_WARN("csharp BunnyPlayer OnRespawn: hook nao instalado");
        else BL_INFO("csharp BunnyPlayer OnRespawn: hook instalado; reviveContext=%d",
                     g_reviveSpawnContext);
    } else {
        BL_WARN("csharp BunnyPlayer OnRespawn: assinatura de Player.Spawn indisponivel");
    }
    const MethodInfo* update = findMethodBySignature(
        player, parseSignature("void Update(int i)"));
    const MethodInfo* dead = findMethodBySignature(
        player, parseSignature("void UpdateDead()"));
    const MethodInfo* lifeRegen = findMethodBySignature(
        player, parseSignature("void UpdateLifeRegen()"));
    const MethodInfo* manaRegen = findMethodBySignature(
        player, parseSignature("void UpdateManaRegen()"));
    if (update && bl::hook::install(update, hkPlayerUpdate, &g_originalPlayerUpdate))
        BL_INFO("csharp BunnyPlayer PreUpdate/PostUpdate: hooks instalados");
    else BL_WARN("csharp BunnyPlayer PreUpdate/PostUpdate: hook indisponivel");
    if (dead && bl::hook::install(dead, hkPlayerUpdateDead, &g_originalUpdateDead))
        BL_INFO("csharp BunnyPlayer UpdateDead: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateDead: hook indisponivel");
    if (r.playerLifeRegen >= 0 && lifeRegen &&
        bl::hook::install(lifeRegen, hkPlayerLifeRegen, &g_originalLifeRegen))
        BL_INFO("csharp BunnyPlayer UpdateLifeRegen: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateLifeRegen: hook indisponivel");
    if (r.playerManaRegen >= 0 && manaRegen &&
        bl::hook::install(manaRegen, hkPlayerManaRegen, &g_originalManaRegen))
        BL_INFO("csharp BunnyPlayer UpdateManaRegen: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateManaRegen: hook indisponivel");
    Il2CppClass* attackerMain = findClass({"Terraria", "Main", {}});
    if (attackerMain && api().field_static_get_value) {
        if (api().runtime_class_init) api().runtime_class_init(attackerMain);
        g_mainNpcArrayField = findField(attackerMain, "npc");
        g_mainProjectileArrayField = findField(attackerMain, "projectile");
        BL_INFO("csharp hit context: Main.npc=%s Main.projectile=%s",
                g_mainNpcArrayField ? "ok" : "indisponivel",
                g_mainProjectileArrayField ? "ok" : "indisponivel");
    }
    const MethodInfo* hurt = findMethodBySignature(player, parseSignature(
        "double Hurt(PlayerDeathReason damageSource, int Damage, int hitDirection, bool pvp, bool quiet, bool Crit, int cooldownCounter, bool dodgeable)"));
    if (hurt && api().method_get_param && api().class_from_il2cpp_type &&
        api().field_get_offset && api().class_value_size &&
        api().class_is_valuetype) {
        const Il2CppType* reasonType = api().method_get_param(hurt, 0);
        Il2CppClass* reason = reasonType
            ? api().class_from_il2cpp_type(reasonType) : nullptr;
        FieldInfo* npc = reason ? findField(reason, "_sourceNPCIndex") : nullptr;
        FieldInfo* projectile = reason
            ? findField(reason, "_sourceProjectileLocalIndex") : nullptr;
        uint32_t alignment = 0;
        const int bytes = reason ? api().class_value_size(reason, &alignment) : 0;
        const bool valueType = reason && api().class_is_valuetype(reason);
        if (npc && projectile && bytes >= 16 && bytes <= 128) {
            const size_t rawNpcOffset = api().field_get_offset(npc);
            const size_t rawProjectileOffset = api().field_get_offset(projectile);
            size_t npcOffset=rawNpcOffset,projectileOffset=rawProjectileOffset;
            bl::il2cpp::ValueLayout layout;std::string layoutError;
            const bool validLayout=!valueType||(bl::il2cpp::resolveValueLayout(reason,&layout,&layoutError)&&
                layout.offset(rawNpcOffset,sizeof(int32_t),&npcOffset)&&layout.offset(rawProjectileOffset,sizeof(int32_t),&projectileOffset));
            if (validLayout && npcOffset + sizeof(int32_t) <= static_cast<size_t>(bytes) &&
                projectileOffset + sizeof(int32_t) <= static_cast<size_t>(bytes) &&
                npcOffset != projectileOffset) {
                g_refs.sourceNpcOffset = static_cast<int>(npcOffset);
                g_refs.sourceProjectileOffset = static_cast<int>(projectileOffset);
                g_refs.sourceSize = bytes;
                BL_INFO("csharp BunnyPlayer fonte do dano: PlayerDeathReason layout=%s size=%d npcOffset=%zu projectileOffset=%zu offsetsOriginais=%zu,%zu",
                        valueType ? "valor" : "referencia", bytes,
                        npcOffset, projectileOffset,
                        rawNpcOffset, rawProjectileOffset);
            }
        }
        if (g_refs.sourceSize == 0)
            BL_WARN("csharp BunnyPlayer fonte do dano: metadata PlayerDeathReason indisponivel");
    }
    if (hurt && r.playerLife >= 0 && r.playerLifeMax >= 0 &&
        r.playerImmune >= 0 && r.playerImmuneTime >= 0 &&
        bl::hook::install(hurt, hkPlayerHurt, &g_originalPlayerHurt))
        BL_INFO("csharp BunnyPlayer ModifyHurt/OnHurt: hook Player.Hurt instalado");
    else
        BL_WARN("csharp BunnyPlayer ModifyHurt/OnHurt: hook Player.Hurt indisponivel");
    g_effectsReady = true;
    BL_INFO("csharp native: hooks de acessorio e buff instalados");
    return true;
}


bool applyManagedNativeDefaults(
    const std::string& modId, const std::string& projectileName, int projectileType,
    const ManagedProjectileDefaults& projectile, const std::string& weaponName,
    int weaponType, const ManagedWeaponDefaults& weapon) {
    if (!g_ready ||
        bl::runtime::modProjectileTypeByName(modId, projectileName) != projectileType ||
        bl::runtime::modItemTypeByKey(modId + "/" + weaponName) != weaponType ||
        g_projectiles.count(projectileType) == 0 || g_weapons.count(weaponType) == 0 ||
        projectile.width < 1 || projectile.width > 512 ||
        projectile.height < 1 || projectile.height > 512 ||
        projectile.aiStyle < 0 || projectile.aiStyle > 255 ||
        projectile.penetrate < -1 || projectile.penetrate > 100 ||
        projectile.timeLeft < 1 || projectile.timeLeft > 36000 ||
        !projectile.friendly ||
        weapon.damage < 1 || weapon.damage > 1000 ||
        weapon.useTime < 2 || weapon.useTime > 120 ||
        !validWeaponShootSpeed(weapon.shootSpeed))
        return false;

    g_projectiles[projectileType] = {
        projectile.width, projectile.height, projectile.aiStyle,
        projectile.penetrate, projectile.timeLeft, true, false
    };
    g_weapons[weaponType] = {
        weapon.damage, weapon.useTime, weapon.shootSpeed, projectileType
    };
    publishProjectileDefinition(projectileType,g_projectiles.at(projectileType),modId+"/"+projectileName);
    publishWeaponDefaults();
    BL_INFO("csharp native: managed SetDefaults applied for %s/%s and %s/%s",
            modId.c_str(), projectileName.c_str(), modId.c_str(), weaponName.c_str());
    return true;
}

bool applyManagedNativeDefaultsBatch(const std::string& modId,
    const std::vector<ManagedProjectileEntry>& projectiles,const std::vector<ManagedWeaponEntry>& weapons) {
    return applyManagedNativeDefaultsBatch(modId,projectiles,weapons,nullptr);
}

bool applyManagedNativeDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedProjectileEntry>& projectiles,
    const std::vector<ManagedWeaponEntry>& weapons,std::string* error) {
    if(error)error->clear();
    if (!g_ready ||
        projectiles.size() > kSourceMaxCategory || weapons.size() > kSourceMaxCategory) return false;

    std::map<int, NativeProjectileDefaults> stagedProjectiles;
    std::map<int,ProjectileGameplayFields> stagedFields;
    std::map<int, NativeWeaponDefaults> stagedWeapons;
    for (const auto& entry : projectiles) {
        const auto& data = entry.values;
        ProjectilePolicy policy{data.frames,data.animationFrameTicks,data.minionScanTicks,data.minionTargetRange,data.minion};
        ProjectileGameplayFields fields;std::string detail;
        if(!validateProjectilePolicy(policy,&detail)||
           !bindProjectileGameplayFields(il2cpp::findClass({"Terraria","Projectile",{}}),data.minion,data.frames>1,&fields,&detail)) {
            const auto diagnostic="[CSharp][ERROR][ProjectilePolicy] Mod="+modId+" Class="+entry.name+" ContentId="+std::to_string(entry.type)+" Stage=BindGameplayFields Result=Rejected Detail="+detail;
            if(error)*error=diagnostic;
            BL_ERROR("%s",diagnostic.c_str());return false;
        }
        if(!bindProjectileFrameTable(il2cpp::findClass({"Terraria","Main",{}}),entry.type,&detail)) {
            const auto diagnostic="[CSharp][ERROR][ProjectilePolicy] Mod="+modId+" Class="+entry.name+" Stage=BindFrames Result=Rejected Detail="+detail;
            if(error)*error=diagnostic;
            BL_ERROR("%s",diagnostic.c_str());return false;
        }
        if(data.minion&&!prepareManagedSummonRuntime(&detail)) {
            const auto diagnostic="[CSharp][ERROR][Summon] Mod="+modId+" Class="+entry.name+" Stage=BindLifecycle Result=Rejected "+detail;
            if(error)*error=diagnostic;
            BL_ERROR("%s",diagnostic.c_str());return false;
        }
        stagedFields.emplace(entry.type,fields);
        if (entry.type < 0 ||
            bl::runtime::modProjectileTypeByName(modId, entry.name) != entry.type ||
            g_projectiles.count(entry.type) != 1 ||
            (data.minion && (g_refs.projMinion < 0 || g_refs.projMinionSlots < 0)) ||
            !std::isfinite(data.minionSlots) ||
            (data.minion && (data.minionSlots <= 0.f || data.minionSlots > 10.f)) ||
            (!data.minion && data.minionSlots != 0.f) ||
            (data.movement.enabled&&(!data.minion||data.aiStyle!=0)) ||
            (data.minionContactDamage && (!data.minion || !g_originalProjectileDamage)) ||
            data.tileCollisionMode < 0 || data.tileCollisionMode > 3 ||
            (data.tileCollisionMode != 0 && !data.tileCollide) ||
            !std::isfinite(data.bounceRestitution) ||
            data.bounceRestitution < 0.f || data.bounceRestitution > 2.f ||
            !std::isfinite(data.bounceMinimumSpeed) ||
            data.bounceMinimumSpeed < 0.f || data.bounceMinimumSpeed > 64.f ||
            (data.tileCollisionMode != 2 &&
             (data.bounceRestitution != 1.f || data.bounceMinimumSpeed != 0.f)) ||
            data.tileCollisionSurvivalLimit < 0 || data.tileCollisionSurvivalLimit > 64 ||
            (data.tileCollisionMode == 0 && data.tileCollisionSurvivalLimit != 0) ||
            data.tilePlatformCollision < 0 || data.tilePlatformCollision > 2 ||
            (data.tilePlatformCollision != 0 && !data.tileCollide) ||
            data.tileCollisionBoxWidth < 0 || data.tileCollisionBoxWidth > 512 ||
            data.tileCollisionBoxHeight < 0 || data.tileCollisionBoxHeight > 512 ||
            !std::isfinite(data.tileCollisionAnchorOffsetX) ||
            !std::isfinite(data.tileCollisionAnchorOffsetY) ||
            data.tileCollisionAnchorOffsetX < -256.f ||
            data.tileCollisionAnchorOffsetX > 256.f ||
            data.tileCollisionAnchorOffsetY < -256.f ||
            data.tileCollisionAnchorOffsetY > 256.f ||
            (!data.tileCollide && (data.tileCollisionBoxWidth != 0 ||
              data.tileCollisionBoxHeight != 0 ||
              data.tileCollisionAnchorOffsetX != 0.f ||
              data.tileCollisionAnchorOffsetY != 0.f)) ||
            data.frames < 1 || data.frames > 32 ||
            !stagedProjectiles.emplace(entry.type, NativeProjectileDefaults{
                data.width, data.height, data.aiStyle, data.penetrate,
                data.timeLeft, true, false,
                data.minion, data.minionContactDamage, data.minionSlots,
                data.tileCollide, data.ignoreWater, data.tileCollisionMode,
                data.bounceRestitution, data.bounceMinimumSpeed,
                data.tileCollisionSurvivalLimit,data.tilePlatformCollision,
                data.tileCollisionBoxWidth,data.tileCollisionBoxHeight,
                data.tileCollisionAnchorOffsetX,data.tileCollisionAnchorOffsetY,
                data.frames,data.animationFrameTicks,data.minionScanTicks,data.minionTargetRange}).second ||
            data.width < 1 || data.width > 512 ||
            data.height < 1 || data.height > 512 ||
            data.aiStyle < 0 || data.aiStyle > 255 ||
            data.penetrate < -1 || data.penetrate > 100 ||
            data.timeLeft < 1 || data.timeLeft > 36000 ||
            !data.friendly)
            return false;
    }
    for (const auto& entry : weapons) {
        const auto& data = entry.values;
        if(entry.summon) {
            int offset=-1;std::string detail;
            if(!nativeFieldContract(il2cpp::findClass({"Terraria","Item",{}}),"summon","System.Boolean",false,1,nullptr,&offset,&detail)||offset!=g_refs.itemSummon) {
                const auto diagnostic="[CSharp][ERROR][NativeWeapon] Mod="+modId+" Class="+entry.name+" Field=summon Stage=BindFields Result=Rejected Detail="+detail;
                if(error)*error=diagnostic;
            BL_ERROR("%s",diagnostic.c_str());return false;
            }
        }
        if (entry.type < 0 ||
            bl::runtime::modItemTypeByKey(modId + "/" + entry.name) != entry.type ||
            g_weapons.count(entry.type) != 1 ||
            stagedProjectiles.count(entry.shootType) != 1 ||
            (entry.summon && (g_refs.itemSummon < 0 ||
                              entry.minionBuffType < 0 ||
                              !stagedProjectiles.at(entry.shootType).minion ||
                              entry.minionBuffTime < 1 || entry.minionBuffTime > 36000)) ||
            !stagedWeapons.emplace(entry.type, NativeWeaponDefaults{
                data.damage, data.useTime, data.shootSpeed, entry.shootType,
                entry.summon, entry.minionBuffType, entry.minionBuffTime}).second ||
            data.damage < 1 || data.damage > 1000 ||
            data.useTime < 2 || data.useTime > 120 ||
            !validNativeWeaponShootSpeed(data.shootSpeed, entry.summon))
            return false;
    }
    const bool needsTileCollisionHook = std::any_of(projectiles.begin(),projectiles.end(),
        [](const ManagedProjectileEntry& entry) {
            const auto& v=entry.values;
            return v.tileCollisionMode != 0 || v.tilePlatformCollision != 0 ||
                v.tileCollisionBoxWidth != 0 || v.tileCollisionBoxHeight != 0 ||
                v.tileCollisionAnchorOffsetX != 0.f || v.tileCollisionAnchorOffsetY != 0.f;
        });
    std::string tileCollisionError;
    const bool needsPlatformPolicy = std::any_of(projectiles.begin(),projectiles.end(),
        [](const ManagedProjectileEntry& entry) {return entry.values.tilePlatformCollision != 0;});
    const bool needsCollisionBox = std::any_of(projectiles.begin(),projectiles.end(),
        [](const ManagedProjectileEntry& entry) {
            const auto& v=entry.values;
            return v.tileCollisionBoxWidth != 0 || v.tileCollisionBoxHeight != 0 ||
                   v.tileCollisionAnchorOffsetX != 0.f || v.tileCollisionAnchorOffsetY != 0.f;
        });
    if (needsTileCollisionHook &&
        !installManagedProjectileTileCollisionHooks(needsPlatformPolicy,needsCollisionBox,error ? error : &tileCollisionError))
        return false;
    auto nextView=std::make_shared<ProjectileDefinitions>(*std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire));
    for (const auto& item : stagedProjectiles) {
        g_projectiles[item.first] = item.second;
        // Update the shared texture/frame registry too: finishModProjectile restores
        // geometry from it, and table growth must preserve the same frame count.
        bl::runtime::setModProjectileFrames(item.first,item.second.frames);
    }
    for(const auto& item:projectiles)(*nextView)[item.type]={stagedProjectiles.at(item.type),stagedFields.at(item.type),item.values.movement,modId+"/"+item.name};
    std::atomic_store_explicit(&g_projectileView,std::shared_ptr<const ProjectileDefinitions>(std::move(nextView)),std::memory_order_release);
    for (const auto& item : stagedWeapons) {
        g_weapons[item.first] = item.second;
        enableManagedPrefixEligibility(item.first);
    }
    publishWeaponDefaults();
    BL_INFO("csharp native: managed batch applied for %s (%zu projectile, %zu weapon)",
            modId.c_str(), stagedProjectiles.size(), stagedWeapons.size());
    return true;
}

bool applyManagedNativeBuffDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedBuffEntry>& buffs,
    const std::vector<ManagedPotionEntry>& potions) {
    if (buffs.empty() && potions.empty()) return true;
    if (!g_effectsReady) return false;
    for (const auto& buff : buffs) {
        if(buff.minionProjectileType>=0&&(g_refs.playerOwnedProjectileCounts<0||!g_removeMinionBuff)) {
            BL_ERROR("[CSharp][ERROR][Buff] Mod=%s Class=%s ContentId=%d ProjectileId=%d API=BunnyBuff.MinionProjectile Stage=BindPlayerState Expected=Int32ArrayAndInstanceDelBuff Result=Rejected Detail=%s DelBuff=%s",
                modId.c_str(),buff.name.c_str(),buff.type,buff.minionProjectileType,g_minionBindError.c_str(),g_removeMinionBuff?"Present":"MissingOrWrongSignature");return false;
        }
        const auto it = g_buffs.find(buff.type);
        if (it == g_buffs.end() ||
            bl::runtime::modBuffTypeByName(modId, buff.name) != buff.type ||
            buff.defenseBonus < -50 || buff.defenseBonus > 100 ||
            (buff.minionProjectileType >= 0 &&
             (!bl::runtime::isModProjectile(buff.minionProjectileType)||(g_refs.playerOwnedProjectileCounts<0||!g_removeMinionBuff))))
            return false;
    }
    for (const auto& potion : potions) {
        const auto it = g_potions.find(potion.type);
        if (it == g_potions.end() ||
            bl::runtime::modItemTypeByKey(modId + "/" + potion.name) != potion.type ||
            it->second.buffType != potion.buffType ||
            potion.duration < 1 || potion.duration > 216000)
            return false;
    }
    bool needsMultiplier=false;
    for(const auto& buff:buffs){
        if(buff.defenseMultiplierPercent<0||buff.defenseMultiplierPercent>100)return false;
        needsMultiplier=needsMultiplier||buff.defenseMultiplierPercent!=100;
    }
    if(needsMultiplier&&!g_buffMultipliersInstalled){
        auto* player=il2cpp::findClass({"Terraria","Player",{}});
        auto* equip=il2cpp::findMethodBySignature(player,il2cpp::parseSignature("void UpdateEquips(int i)"));
        if(!equip||!il2cpp::api().method_is_instance(equip)||!hook::install(equip,hkBuffEquipment,&g_originalBuffEquip)) {
            BL_ERROR("[CSharp][ERROR][Buff] Mod=%s API=BunnyBuff.DefenseMultiplierPercent Method=Player.UpdateEquips Stage=InstallHook Result=Rejected Reason=NativeSignatureOrHookMismatch",modId.c_str());return false;
        }
        g_buffMultipliersInstalled=true;
    }
    for (const auto& buff : buffs) {
        if(buff.flags>=0) {
            std::string error;
            if(!runtime::updateModBuffDefaults(modId,buff.type,buff.flags,buff.description,&error)) {
                BL_ERROR("[CSharp][ERROR][Buff] Mod=%s Class=%s ContentId=%d API=BunnyBuff Stage=ApplyManagedDefaults Result=Rejected Detail=%s",
                    modId.c_str(),buff.name.c_str(),buff.type,error.c_str());return false;
            }
        }
        g_buffs[buff.type].defenseBonus = buff.defenseBonus;
        g_buffs[buff.type].minionProjectileType = buff.minionProjectileType;
        g_buffs[buff.type].defenseMultiplierPercent=buff.defenseMultiplierPercent;
    }
    for (const auto& potion : potions)
        g_potions[potion.type].duration = potion.duration;
    publishBuffDefaults();
    BL_INFO("csharp native: managed buffs applied for %s (%zu buffs, %zu potions)",
            modId.c_str(), buffs.size(), potions.size());
    return true;
}

bool applyManagedNativePlayerDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedAccessoryEntry>& accessories,
    const std::vector<ManagedPlayerEntry>& players) {
    if (accessories.empty() && players.empty()) return true;
    if (!g_effectsReady || accessories.size() > kSourceMaxCategory || players.size() > kSourceMaxCategory)
        return false;
    std::map<int, NativeAccessoryDefaults> values;
    for (const auto& accessory : accessories) {
        if (accessory.type < 0 || accessory.defenseBonus < 0 ||
            accessory.defenseBonus > 100 || accessory.tooltip.size() > 180 ||
            bl::runtime::modItemTypeByKey(modId + "/" + accessory.name) != accessory.type ||
            g_accessories.count(accessory.type) != 1 ||
            !values.emplace(accessory.type,
                NativeAccessoryDefaults{accessory.defenseBonus, 0, 0, false, ""}).second)
            return false;
        auto& config = values.at(accessory.type);
        config.visualMode = accessory.visualMode;
        config.visualTexture = accessory.visualMode > 0 ? modId + ":" + accessory.visualTexture : "";
        config.visualScale = accessory.visualScale;
        config.visualOpacity = accessory.visualOpacity;
        config.visualColor = accessory.visualColor;
    }
    std::map<int, bool> linked;
    std::map<std::string, std::string> globalMessages;
    std::map<std::string, std::string> respawnMessages;
    std::map<std::string, std::string> deadMessages;
    bool newPreUpdate = false;
    bool newPostUpdate = false;
    for (const auto& player : players) {
        const std::string key = modId + "/" + player.name;
        if (player.respawnMessage.size() > 100 || player.deadMessage.size() > 100 ||
            player.lifeRegenBonus < 0 || player.lifeRegenBonus > 30 ||
            player.manaRegenBonus < 0 || player.manaRegenBonus > 30) return false;
        if (player.preUpdate) newPreUpdate = true;
        if (player.postUpdate) newPostUpdate = true;
        if (!player.deadMessage.empty()) deadMessages[key] = player.deadMessage;
        if (player.accessoryType < 0) {
            if (player.accessoryName != "-" || player.equippedDefenseBonus != 0 ||
                player.moveSpeedPercent != 0 || player.runAccelerationPercent != 0 ||
                player.frameShadow || player.worldJoinMessage.size() > 100 ||
                player.lifeRegenBonus != 0 || player.manaRegenBonus != 0) return false;
            if (!player.worldJoinMessage.empty())
                globalMessages[key] = player.worldJoinMessage;
            if (!player.respawnMessage.empty())
                respawnMessages[key] = player.respawnMessage;
            continue;
        }
        if (
            player.equippedDefenseBonus < 0 || player.equippedDefenseBonus > 100 ||
            player.moveSpeedPercent < 0 || player.moveSpeedPercent > 50 ||
            player.runAccelerationPercent < 0 || player.runAccelerationPercent > 30 ||
            player.worldJoinMessage.size() > 100 ||
            bl::runtime::modItemTypeByKey(modId + "/" + player.accessoryName) != player.accessoryType ||
            !values.contains(player.accessoryType) ||
            !linked.emplace(player.accessoryType, true).second)
            return false;
        auto& bonus = values[player.accessoryType];
        if (bonus.defenseBonus + player.equippedDefenseBonus > 100) return false;
        bonus.defenseBonus += player.equippedDefenseBonus;
        bonus.moveSpeedPercent = player.moveSpeedPercent;
        bonus.movementAccelerationPercent = player.runAccelerationPercent;
        bonus.frameShadow = player.frameShadow;
        bonus.worldJoinMessage = player.worldJoinMessage;
        bonus.lifeRegenBonus = player.lifeRegenBonus;
        bonus.manaRegenBonus = player.manaRegenBonus;
        if (!player.respawnMessage.empty())
            respawnMessages[key] = player.respawnMessage;
    }
    for (const auto& accessory : accessories) {
        if (!accessory.tooltip.empty())
            bl::runtime::setModItemTooltip(accessory.type, {{"", accessory.tooltip}});
    }
    for (const auto& [type, effects] : values) {
        g_accessories[type] = effects;
        if(effects.visualMode==2)requestNativeVisualRenderer(effects.visualTexture);
        enableManagedPrefixEligibility(type);
    }
    {
        std::lock_guard lock(g_nativeEquipDescriptorsMutex);
        for(const auto& [type,effects]:values) {
            if((effects.visualMode==1||effects.visualMode==3)&&!effects.visualTexture.empty())
                g_nativeEquipDescriptors[type]={type,effects.visualMode,effects.visualTexture};
            else g_nativeEquipDescriptors.erase(type);
        }
    }
    for (const auto& [key, message] : globalMessages)
        g_globalWorldMessages[key] = message;
    for (const auto& [key, message] : respawnMessages)
        g_respawnMessages[key] = message;
    for (const auto& [key, message] : deadMessages)
        g_deadMessages[key] = message;
    g_preUpdateEnabled = g_preUpdateEnabled || newPreUpdate;
    g_postUpdateEnabled = g_postUpdateEnabled || newPostUpdate;
    for (const auto& [type, effect] : g_accessories) {
        g_anyLifeRegen = g_anyLifeRegen || effect.lifeRegenBonus > 0;
        g_anyManaRegen = g_anyManaRegen || effect.manaRegenBonus > 0;
    }
    if (!players.empty()) {
        std::vector<ManagedPlayerEntry> sorted = players;
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.name < b.name; });
        ManagedHurtBinding binding;
        binding.modId = modId;
        for (const auto& player : sorted)
            binding.accessoryTypes.push_back(player.accessoryType);
        std::lock_guard<std::mutex> lock(g_hurtBindingsMutex);
        g_hurtBindings[modId] = std::move(binding);
    }
    g_playerPoliciesReady = true;
    BL_INFO("csharp native: managed player equipment applied for %s (%zu accessory, %zu player)",
            modId.c_str(), accessories.size(), players.size());
    return true;
}

bool applyManagedNativeNpcDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedNpcEntry>& npcs) {
    if (npcs.empty()) return true;
    if (!g_npcReady || npcs.size() > kSourceMaxCategory) return false;
    std::map<int, NativeNpcDefaults> staged;
    for (const auto& npc : npcs) {
        if (npc.type < 0 ||
            bl::runtime::modNpcTypeByName(modId, npc.name) != npc.type ||
            g_npcs.count(npc.type) != 1 ||
            npc.width < 8 || npc.width > 256 ||
            npc.height < 8 || npc.height > 256 ||
            npc.damage < 0 || npc.damage > 500 ||
            npc.defense < 0 || npc.defense > 200 ||
            npc.lifeMax < 1 || npc.lifeMax > 100000 ||
            npc.aiStyle != 1 || npc.animationType != 1 ||
            npc.frames != 2 ||
            !staged.emplace(npc.type, NativeNpcDefaults{
                npc.width, npc.height, npc.damage, npc.defense,
                npc.lifeMax, npc.aiStyle}).second) return false;
    }
    for (const auto& entry : staged)
        g_npcs[entry.first] = entry.second;
    BL_INFO("csharp native: managed NPC defaults applied for %s (%zu NPC)",
            modId.c_str(), staged.size());
    return true;
}

bool bindManagedPlayerHurtMod(const std::string& modId,
                              const std::string& assemblyPath) {
    {
        std::lock_guard<std::mutex> lock(g_hurtBindingsMutex);
        if (!g_hurtBindings.contains(modId)) return false;
    }
    if (!registerManagedPlayerHurtMod(modId, assemblyPath)) return false;
    std::lock_guard<std::mutex> lock(g_hurtBindingsMutex);
    auto it = g_hurtBindings.find(modId);
    if (it == g_hurtBindings.end()) return false;
    it->second.assemblyPath = assemblyPath;
    it->second.active = true;
    return true;
}

void pauseManagedPlayerHurtBindings() {
    {
        std::lock_guard<std::mutex> lock(g_hurtBindingsMutex);
        for (auto& [id, binding] : g_hurtBindings) binding.active = false;
    }
    pauseManagedPlayerHurtMods();
}

bool showManagedPlayerWorldJoinMessages() {
    if (!g_playerPoliciesReady) return false;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* say = main ? findMethodBySignature(
        main, parseSignature(
            "void NewText(string newText, byte R, byte G, byte B, bool onlyCurrentPlayer)"))
        : nullptr;
    if (!say || !runtime.string_new) {
        BL_WARN("csharp BunnyPlayer OnEnterWorld: Terraria.Main.NewText indisponivel");
        return true;
    }
    int shown = 0;
    for (const auto& [key, message] : g_globalWorldMessages)
        if (sendPlayerLifecycleMessage(message)) ++shown;
    for (const auto& [type, effects] : g_accessories) {
        if (effects.worldJoinMessage.empty()) continue;
        Il2CppString* line = runtime.string_new(effects.worldJoinMessage.c_str());
        if (!line) continue;
        uint8_t r = 255, g = 200, b = 0, only = 1;
        void* arguments[5] = {line, &r, &g, &b, &only};
        Il2CppObject* error = nullptr;
        runtime.runtime_invoke(say, nullptr, arguments, &error);
        if (!error) ++shown;
    }
    BL_INFO("csharp BunnyPlayer OnEnterWorld: %d mensagem(ns) enviada(s) ao entrar", shown);
    return true;
}

std::vector<ManagedNativeEquipTexture> managedNativeEquipTextures() {
    std::lock_guard lock(g_nativeEquipDescriptorsMutex);
    static int last=-1;
    std::vector<ManagedNativeEquipTexture> entries;
    auto it=g_nativeEquipDescriptors.upper_bound(last);
    for(size_t scanned=0;scanned<g_nativeEquipDescriptors.size()&&entries.size()<8;++scanned) {
        if(it==g_nativeEquipDescriptors.end())it=g_nativeEquipDescriptors.begin();
        entries.push_back(it->second);last=it->first;++it;
    }
    return entries;
}
size_t pendingManagedNativeEquipTextures() {
    std::lock_guard lock(g_nativeEquipDescriptorsMutex);return g_nativeEquipDescriptors.size();
}

bool setManagedNativeEquipSlot(int itemType, int visualMode, int equipSlot) {
    auto it = g_accessories.find(itemType);
    if (it == g_accessories.end() ||
        it->second.visualMode != visualMode ||
        (visualMode != 1 && visualMode != 3) ||
        equipSlot <= 0 || equipSlot >= 128)
        return false;
    if (it->second.nativeEquipSlot >= 0)
        return it->second.nativeEquipSlot == equipSlot;
    const int offset = visualMode == 1 ? g_refs.itemShieldSlot : g_refs.itemShoeSlot;
    if (offset < 0) return false;
    it->second.nativeEquipSlot = equipSlot;
    {std::lock_guard lock(g_nativeEquipDescriptorsMutex);g_nativeEquipDescriptors.erase(itemType);}
    BL_INFO("csharp equip: item=%d tipo=%s slot=%d", itemType,
            visualMode == 1 ? "Shield" : "Shoes", equipSlot);
    return true;
}

int managedNativeAccessoryDefense(int type) {
    const auto it = g_accessories.find(type);
    return it == g_accessories.end() ? 0 : it->second.defenseBonus;
}

int managedNativeAccessoryMoveSpeedPercent(int type) {
    const auto it = g_accessories.find(type);
    return it == g_accessories.end() ? 0 : it->second.moveSpeedPercent;
}

int managedNativePrefixItemKind(int type) {
    if (basicItemIsAccessory(type)) return 2;
    if (basicItemHasDamage(type)) return 1;
    if (std::atomic_load_explicit(&g_weaponView,std::memory_order_acquire)->contains(type)) return 1;
    if (g_accessories.contains(type)) return 2;
    return 0;
}

void refreshManagedPrefixEligibilityOnWorldReady() {
    int corrected = 0;
    for (int type : basicItemTypes())
        if (basicItemHasDamage(type) && enableManagedPrefixEligibility(type)) ++corrected;
    const auto weapons=std::atomic_load_explicit(&g_weaponView,std::memory_order_acquire);
    for (const auto& weapon : *weapons)
        if (enableManagedPrefixEligibility(weapon.first)) ++corrected;
    for (const auto& accessory : g_accessories)
        if (enableManagedPrefixEligibility(accessory.first)) ++corrected;
    BL_INFO("csharp prefix: mundo pronto; elegibilidade C# revista para %zu item(ns), %d corrigido(s)",
            weapons->size() + g_accessories.size(), corrected);
}

namespace {

std::mutex g_tileDefaultsMutex;
std::map<int, std::pair<std::string, ManagedTileEntry>> g_tileDefaults;

Il2CppObject* tileStaticObject(Il2CppClass* cls, const char* name) {
    auto& a = il2cpp::api();
    if (!cls) return nullptr;
    if (FieldInfo* field = il2cpp::findField(cls, name)) {
        Il2CppObject* result = nullptr;
        a.field_static_get_value(field, &result);
        if (result) return result;
    }
    const std::string getter = std::string("get_") + name;
    const MethodInfo* method = a.class_get_method_from_name(cls, getter.c_str(), 0);
    Il2CppObject* exc = nullptr;
    return method ? a.runtime_invoke(method, nullptr, nullptr, &exc) : nullptr;
}

#include "experiments/csharp/NativeTileSaplingShape.h"

bool applyTileShape(const std::string& mod,int type,const ManagedTileEntry& e) {
    if(e.shape==4)return applyNativeSaplingShape(mod,type,e);
    if (e.shape == 0) return true;
    if (!tile_policy::validShape(e.shape)) return false;
    auto& a=il2cpp::api();
    auto* cls=il2cpp::findClass({"Terraria.ObjectData","TileObjectData",{}});
    auto* tile=tileStaticObject(cls,"newTile");
    if (!tile || !cls || !a.object_get_class || !a.method_get_param_count ||
        !a.method_get_param || !a.method_get_return_type || !a.method_is_instance)
        return false;
    auto* tileClass=a.object_get_class(tile);
    const auto typeIs=[&](const Il2CppType* type,const char* expected) {
        if(!type||!a.type_get_name||!a.il2cpp_free)return false;
        char* text=a.type_get_name(type);
        const bool match=text&&std::strcmp(text,expected)==0;
        if(text)a.il2cpp_free(text);
        return match;
    };
    const auto* copy=a.class_get_method_from_name(tileClass,"CopyFrom",1);
    const auto* addTile=a.class_get_method_from_name(cls,"addTile",1);
    if(!copy||!addTile||!il2cpp::methodPointer(copy)||!il2cpp::methodPointer(addTile))
        return false;
    Il2CppObject* source=nullptr;
    if(e.shape==2) {
        const auto* get=a.class_get_method_from_name(cls,"GetTileData",3);
        if(!get||!il2cpp::methodPointer(get)||a.method_is_instance(get)||
           a.method_get_param_count(get)!=3||
           !typeIs(a.method_get_return_type(get),"Terraria.ObjectData.TileObjectData"))
            return false;
        for(unsigned i=0;i<3;++i)
            if(!typeIs(a.method_get_param(get,i),"System.Int32"))return false;
        int32_t campfire=215,style=0,alternate=0;
        void* args[]={&campfire,&style,&alternate};
        Il2CppObject* exc=nullptr;
        source=a.runtime_invoke(get,nullptr,args,&exc);
        if(exc||!source||a.object_get_class(source)!=tileClass)return false;
    } else {
        source=tileStaticObject(cls,e.shape==1?"Style2x1":"Style1xX");
    }
    if(!source)return false;
    struct NativeProperty {
        const MethodInfo* setter=nullptr;
        int offset=-1;
        size_t size=0;
    };
    const auto resolve=[&](const char* fieldName,const char* type,size_t size,NativeProperty* out) {
        const std::string setterName=std::string("set_")+fieldName;
        const auto* setter=a.class_get_method_from_name(tileClass,setterName.c_str(),1);
        if(setter) {
            if(!il2cpp::methodPointer(setter)||a.method_is_instance(setter)==false||
               a.method_get_param_count(setter)!=1||
               !typeIs(a.method_get_param(setter,0),type)||
               !typeIs(a.method_get_return_type(setter),"System.Void"))return false;
            *out={setter,-1,size};
            return true;
        }
        std::string detail;
        FieldInfo* field=nullptr;
        int offset=-1;
        if(!il2cpp::nativeFieldContract(tileClass,fieldName,type,false,size,
                                        &field,&offset,&detail))return false;
        *out={nullptr,offset,size};
        return true;
    };
    const auto assign=[&](const NativeProperty& slot,const void* value) {
        if(slot.setter) {
            void* args[]={const_cast<void*>(value)};
            Il2CppObject* exc=nullptr;
            a.runtime_invoke(slot.setter,tile,args,&exc);
            return exc==nullptr;
        }
        if(slot.offset<0)return false;
        std::memcpy(reinterpret_cast<char*>(tile)+slot.offset,value,slot.size);
        return true;
    };
    NativeProperty line{},yoff{},flip{};
    if((e.shape!=1&&!resolve("StyleLineSkip","System.Int32",4,&line))||
       (e.shape==3&&(!resolve("DrawYOffset","System.Int32",4,&yoff)||
                     !resolve("DrawFlipHorizontal","System.Boolean",1,&flip))))return false;
    const auto* setHeights=e.shape==1?a.class_get_method_from_name(tileClass,"set_CoordinateHeights",1):nullptr;
    if(e.shape==1&&(!setHeights||!il2cpp::methodPointer(setHeights)))return false;
    void* copyArgs[]={source};
    Il2CppObject* exc=nullptr;
    a.runtime_invoke(copy,tile,copyArgs,&exc);
    if(exc)return false;
    if(e.shape==1) {
        auto* intClass=il2cpp::findClass({"System","Int32",{}});
        if(!intClass||!a.array_new)return false;
        auto* heights=a.array_new(intClass,1);
        if(!heights)return false;
        *static_cast<int32_t*>(arrayData(heights))=e.coordinateHeight;
        void* args[]={heights};
        Il2CppObject* error=nullptr;
        a.runtime_invoke(setHeights,tile,args,&error);
        if(error)return false;
    } else {
        int32_t skip=e.shape==2?9:2,offset=2;
        bool drawFlip=true;
        if(!assign(line,&skip)||
           (e.shape==3&&(!assign(yoff,&offset)||!assign(flip,&drawFlip))))
            return false;
    }
    int32_t id=type;
    void* addArgs[]={&id};
    a.runtime_invoke(addTile,nullptr,addArgs,&exc);
    return !exc;
}

bool applyTileFlags(const std::string& mod, int type, const ManagedTileEntry& entry) {
    std::string error;
    if (tile_flags::apply(type, entry.flags, &error)) return true;
    BL_ERROR("[CSharp][ERROR][TileRegistration] Mod=%s Class=%s API=BunnyTile.Flags ContentId=%d Result=Rejected %s",
             mod.c_str(), entry.name.c_str(), type, error.c_str());
    return false;
}

using NativeAdjFn = void (*)(Il2CppObject*, const MethodInfo*);
NativeAdjFn g_previousAdj = nullptr;
bool g_adjReady = false;
int g_adjPosition = -1, g_adjWidth = -1, g_adjHeight = -1, g_adjArray = -1;
constexpr size_t kMaxNativeAdjacencies = 1024;
std::array<std::pair<int, int>, kMaxNativeAdjacencies> g_adjPairs{};
size_t g_adjCount = 0;

bool addNativeAdjacency(int type, int vanilla) {
    return tile_adjacency::add(g_adjPairs, g_adjCount, type, vanilla);
}

void nativePlayerAdjTiles(Il2CppObject* player, const MethodInfo* method) {
    if (g_previousAdj) g_previousAdj(player, method);
    if (!g_adjReady || !player) return;
    const auto* bytes = reinterpret_cast<const unsigned char*>(player);
    float pos[2] = {};
    int32_t width = 0, height = 0;
    Il2CppArray* adjacent = nullptr;
    std::memcpy(pos, bytes + g_adjPosition, sizeof(pos));
    std::memcpy(&width, bytes + g_adjWidth, sizeof(width));
    std::memcpy(&height, bytes + g_adjHeight, sizeof(height));
    std::memcpy(&adjacent, bytes + g_adjArray, sizeof(adjacent));
    if (!adjacent || adjacent->length < runtime::kVanillaTileCount ||
        !std::isfinite(pos[0]) || !std::isfinite(pos[1]) ||
        width < 1 || width > 256 || height < 1 || height > 256) return;
    if (g_adjCount == 0) return;
    const int x = static_cast<int>(std::floor((pos[0] + width / 2.f) / 16.f));
    const int y = static_cast<int>(std::floor((pos[1] + height) / 16.f));
    runtime::TileArrays world{};
    if (!runtime::tileArrays(&world)) return;
    auto* result = static_cast<uint8_t*>(arrayData(adjacent));
    for (int i = std::max(0, x - 4); i <= std::min(world.width - 1, x + 4); ++i)
        for (int j = std::max(0, y - 3); j <= std::min(world.height - 1, y + 2); ++j) {
            const int64_t offset = static_cast<int64_t>(j) * world.width + i;
            const uint32_t definition = world.lookup[offset];
            if ((world.sHeader[definition] & runtime::kTileActiveBit) == 0) continue;
            const int type = world.type[definition];
            if (type < runtime::kVanillaTileCount ||
                static_cast<uintptr_t>(type) >= adjacent->length) continue;
            if (tile_adjacency::forEach(g_adjPairs, g_adjCount, type,
                    [&](int alias) { result[alias] = 1; }))
                result[type] = 1;
        }
}

bool ensureNativeAdjTiles() {
    if (g_adjReady) return true;
    auto& a = il2cpp::api();
    Il2CppClass* cls = il2cpp::findClass({"Terraria", "Player", {}});
    const MethodInfo* target = cls ? a.class_get_method_from_name(cls, "AdjTiles", 0) : nullptr;
    if (!target || !a.class_instance_size) return false;
    const int position = il2cpp::fieldOffset(cls, "position");
    const int width = il2cpp::fieldOffset(cls, "width");
    const int height = il2cpp::fieldOffset(cls, "height");
    const int adjacent = il2cpp::fieldOffset(cls, "adjTile");
    const unsigned size = a.class_instance_size(cls);
    const auto fits = [&](int offset, unsigned count) {
        return offset >= static_cast<int>(sizeof(Il2CppObject)) &&
               static_cast<unsigned>(offset) <= size &&
               count <= size - static_cast<unsigned>(offset);
    };
    if (!fits(position, 8) || !fits(width, 4) ||
        !fits(height, 4) || !fits(adjacent, sizeof(void*))) return false;
    if (!hook::install(target, nativePlayerAdjTiles, &g_previousAdj)) return false;
    g_adjPosition = position; g_adjWidth = width;
    g_adjHeight = height; g_adjArray = adjacent;
    g_adjReady = true;
    return true;
}

bool appendNativeHousingTable(const std::string& mod, const ManagedTileEntry& entry) {
    Il2CppClass* sets = il2cpp::findClass({"Terraria.ID", "TileID", "Sets"});
    Il2CppClass* room = sets ? il2cpp::findNested(sets, "RoomNeeds") : nullptr;
    std::string error;
    if (tile_flags::appendHousing(entry.type, room,
            [](Il2CppArray* array, uintptr_t size) {
                return runtime::TypeTables::resizedCopy(array, size);
            }, &error)) return true;
    BL_ERROR("[CSharp][ERROR][TileRegistration] Mod=%s Class=%s API=BunnyTile.Flags.CountsAsTable ContentId=%d Result=Rejected %s",
             mod.c_str(), entry.name.c_str(), entry.type, error.c_str());
    return false;
}

bool installCraftingStationIcon(const std::string& modId,
                                const ManagedTileEntry& e) {
    auto& a = il2cpp::api();
    const int item = basicItemForTile(modId + "/" + e.name);
    if (item < 0) return false;
    Il2CppClass* sets = il2cpp::findClass({"Terraria.ID", "TileID", "Sets"});
    FieldInfo* field = sets ? il2cpp::findField(sets, "CraftingStationItemId") : nullptr;
    if (!field) return false;
    Il2CppArray* arr = nullptr;
    a.field_static_get_value(field, &arr);
    if (!arr) return false;
    if (static_cast<uintptr_t>(e.type) >= arr->length) {
        Il2CppArray* grown = runtime::TypeTables::resizedCopy(arr, e.type + 1);
        if (!grown) return false;
        arr = grown;
        a.field_static_set_value(field, arr);
    }
    auto* values = static_cast<int32_t*>(arrayData(arr));
    if (values[e.type] == 0 || values[e.type] == item)
        values[e.type] = item;
    return values[e.type] == item;
}

void onManagedTilesInstalled(int first, int last) {
    std::vector<std::pair<std::string, ManagedTileEntry>> records;
    {
        std::lock_guard<std::mutex> lock(g_tileDefaultsMutex);
        for (const auto& [type, entry] : g_tileDefaults)
            if (type >= first && type <= last) records.push_back(entry);
    }
    for (const auto& [mod, entry] : records) {
        const bool flags = applyTileFlags(mod, entry.type, entry);
        const bool shape = applyTileShape(mod,entry.type, entry);
        const bool housing = (entry.flags & 256) == 0 || appendNativeHousingTable(mod, entry);
        const bool guide = installCraftingStationIcon(mod, entry);
        bool adjacency = true;
        if (entry.adjacentTile >= 0 || !entry.adjacentTiles.empty()) {
            adjacency = ensureNativeAdjTiles();
            if (adjacency && entry.adjacentTile >= 0)
                adjacency = addNativeAdjacency(entry.type, entry.adjacentTile);
            if (adjacency)
                for (int alias : entry.adjacentTiles)
                    if (!addNativeAdjacency(entry.type, alias)) {
                        adjacency = false;
                        break;
                    }
        }
        const bool gameplay = flags && shape &&
            registerNativeTileGameplay({mod, entry.name, entry.dustName, entry.type,
                                        entry.shape, entry.failDust, entry.normalDust,
                                        entry.animationFrameHeight, entry.animationFrames,
                                        entry.animationFrameTicks,
                                        entry.allowBreaking, entry.allowDrops, entry.callbackMask,
                                        entry.cacheDrawData, {},entry.frameToggle,
                                        (entry.flags & 65536)!=0,entry.registeredDrops,
                                         entry.minPick,entry.mineResist,
                                         entry.saplingGroundTile.empty()?-1:
                                           runtime::modTileTypeByKey(mod+"/"+entry.saplingGroundTile),
                                         entry.saplingVanillaGroundTile,
                                         entry.saplingGrowthChance});
        if (gameplay && entry.animationFrameHeight > 0)
            runtime::setModTileAnimation(entry.type, entry.animationFrameHeight);
        if (!gameplay)
            BL_ERROR("[CSharp][ERROR][Tile] Mod=%s Class=%s Stage=NativeGameplay Result=Incomplete",
                     mod.c_str(), entry.name.c_str());
        if (!adjacency)
            BL_ERROR("[CSharp][ERROR][Tile] Mod=%s Class=%s Stage=NativeAdjacency Result=Unavailable",
                     mod.c_str(), entry.name.c_str());
        if (!flags || !shape || !housing)
            BL_ERROR("[CSharp][ERROR][Tile] Mod=%s Class=%s Type=%d Stage=SetStaticDefaults Flags=%d Shape=%d Housing=%d Result=Incomplete",
                     mod.c_str(), entry.name.c_str(), entry.type, flags, shape, housing);
        if (!guide)
            BL_DEBUG("[CSharp][Tile] Mod=%s Class=%s GuideIcon=Pending",
                     mod.c_str(), entry.name.c_str());
    }
}

}

bool applyManagedNativeTileDefaultsBatch(const std::string& modId,
                                         const std::vector<ManagedTileEntry>& tiles) {
    for (const auto& entry : tiles) {
        if (entry.type < runtime::kVanillaTileCount ||
            runtime::modTileTypeByName(modId, entry.name) != entry.type ||
            entry.flags < 0 || entry.flags > tile_flags::supportedMask ||
            entry.shape < 0 || entry.shape > 4 ||
            entry.frameToggle < 0 || entry.frameToggle > 2 ||
            (entry.frameToggle==1&&entry.shape!=2) ||
            (entry.frameToggle==2&&entry.shape!=3) || entry.coordinateHeight != 18 ||
            entry.animationFrameHeight < 0 || entry.animationFrameHeight > 4096 ||
            entry.animationFrames < 1 || entry.animationFrames > 64 ||
            entry.animationFrameTicks < 0 || entry.animationFrameTicks > 600 ||
            (entry.animationFrames > 1 &&
             (entry.animationFrameHeight == 0 || entry.animationFrameTicks == 0)) ||
            (entry.animationFrames == 1 && entry.animationFrameTicks != 0) ||
            entry.adjacentTile < -1 || entry.adjacentTile >= runtime::kVanillaTileCount ||
            entry.adjacentTiles.size() > 16 ||
            !tile_policy::validCallbackMask(entry.callbackMask) ||
             !tile_mining::valid(entry.minPick,entry.mineResist) ||
            !tile_growth::valid(entry.shape,entry.saplingGrowthChance) ||
            entry.mapR < 0 || entry.mapR > 255 ||
            entry.mapG < 0 || entry.mapG > 255 ||
            entry.mapB < 0 || entry.mapB > 255)
            return false;
    }
    for (const auto& entry : tiles) {
        for (size_t i = 0; i < entry.adjacentTiles.size(); ++i) {
            const int alias = entry.adjacentTiles[i];
            if (alias < 0 || alias >= runtime::kVanillaTileCount ||
                alias == entry.adjacentTile ||
                std::find(entry.adjacentTiles.begin(), entry.adjacentTiles.begin() + i, alias) !=
                    entry.adjacentTiles.begin() + i) return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_tileDefaultsMutex);
        for (const auto& entry : tiles)
            g_tileDefaults[entry.type] = {modId, entry};
    }
    for (const auto& entry : tiles) {
        if (!entry.mapName.empty() &&
            runtime::addModTileMapEntry(entry.type, entry.mapR, entry.mapG, entry.mapB,
                "Mods." + modId + ".MapObject." + entry.name) < 0)
            return false;
    }
    runtime::setNativeTilesInstalledHook(onManagedTilesInstalled);
    runtime::requestNativeTileDefaults();
    return true;
}


bool publishManagedNativeGores(const std::string& modId,const std::string& directory,
                               const std::vector<ManagedGoreEntry>& gores,
                               std::map<std::string,int>* ids,std::string* error) {
    if(!ids||!error)return false;
    if(gores.empty())return true;
    const auto fail=[&](const char* stage,const std::string& reason) {
        *error="[CSharp][GoreLoader] Mod="+modId+" Stage="+stage+
               " Reason="+reason+" Result=Rejected";
        BL_ERROR("%s",error->c_str());
        return false;
    };
    auto& a=il2cpp::api();
    table_publication::Roots roots(a);
    if(!a.field_static_get_value||!a.field_static_set_value||!a.object_get_class||
       !a.class_get_element_class||!a.class_get_name||!a.array_new||
       !a.field_get_flags||!a.class_get_rank||!a.class_is_valuetype||
       !a.runtime_invoke||!a.gchandle_new||!a.gchandle_get_target||!a.gchandle_free)
        return fail("ABI","UnavailableIL2CPPAccess");
    auto* textures=il2cpp::findClass({"Terraria.GameContent","TextureAssets",{}});
    auto* goreClass=il2cpp::findClass({"Terraria.ID","GoreID",{}});
    auto* sets=goreClass?il2cpp::findNested(goreClass,"Sets"):nullptr;
    auto* safety=il2cpp::findClass({"Terraria.GameContent","ChildSafety",{}});
    if(!textures||!sets||!safety)return fail("ABI","GoreClassesUnavailable");
    const std::array<Il2CppClass*,6> classes={textures,sets,sets,sets,sets,safety};
    const std::array<const char*,6> names={
        "Gore","SpecialAI","DisappearSpeed","DisappearSpeedAlpha","IsDrip","SafeGore"
    };
    std::array<FieldInfo*,6> fields{};
    std::array<Il2CppArray*,6> original{};
    std::array<Il2CppArray*,6> pending{};
    for(size_t i=0;i<fields.size();++i) {
        fields[i]=il2cpp::findField(classes[i],names[i]);
        if(!fields[i]||!(a.field_get_flags(fields[i])&0x10u))
            return fail("ABI",std::string("MissingStaticField:")+names[i]);
        a.field_static_get_value(fields[i],&original[i]);
        if(!original[i]||a.class_get_rank(original[i]->obj.klass)!=1||!roots.add(reinterpret_cast<Il2CppObject*>(original[i])))return fail("ABI",std::string("MissingArray:")+names[i]);
    }
    const auto first=original[0]->length;
    if(!gore_policy::capacity(first,gores.size()))
        return fail("Capacity","ExceededGoreIDs");
    for(size_t i=1;i<original.size();++i)
        if(original[i]->length>first || original[i]->length==0)
            return fail("ABI",std::string("GoreTableLengthMismatch:")+names[i]);
    const auto elemName=[&](Il2CppArray* arr)->std::string {
        auto* cls=a.class_get_element_class(
            a.object_get_class(reinterpret_cast<Il2CppObject*>(arr)));
        const char* name=cls?a.class_get_name(cls):nullptr;
        return name?name:"";
    };
    const std::string special=elemName(original[1]);
    const std::string speed=elemName(original[2]);
    const std::string alpha=elemName(original[3]);
    if((special!="Byte"&&special!="Int32")||
       (speed!="Int32"&&speed!="Single"&&speed!="Byte")||
       (alpha!="Int32"&&alpha!="Single"&&alpha!="Byte")||
       elemName(original[4])!="Boolean"||elemName(original[5])!="Boolean")
        return fail("ABI","UnexpectedGoreElementTypes");
    for(size_t i=1;i<original.size();++i) {
        const std::string type=elemName(original[i]);
        const size_t width=(type=="Byte"||type=="Boolean")?1:4;
        if(!il2cpp::nativeArrayContract(original[i],type.c_str(),width,32767))
            return fail("ABI","GoreArrayLayoutMismatch");
    }
    std::map<std::string,bool> namesUsed;
    std::vector<std::string> files;
    files.reserve(gores.size());
    for(const auto& gore:gores) {
        if(!gore_policy::valid(gore.name,gore.texture,gore.specialAI)||
           !namesUsed.emplace(gore.name,true).second)
            return fail("Preflight","InvalidOrDuplicateGore:"+gore.name);
        const auto file=gore_policy::filePath(directory,gore.texture);
        if(file.empty()||!fileExists(file))
            return fail("Files","MissingGorePng:"+gore.name);
        files.push_back(file);
    }
    std::vector<Il2CppObject*> assets;
    assets.reserve(gores.size());
    for(size_t i=0;i<gores.size();++i) {
        int width=0,height=0;
        auto* asset=runtime::content::loadTextureAsset(
            files[i],nullptr,0,modId+"/Gores/"+gores[i].name,&width,&height);
        if(!asset||width<1||height<1||width>4096||height>4096)
            return fail("Texture","InvalidGorePng:"+gores[i].name);
        if(!roots.add(asset))return fail("GC","GoreAssetRootFailed");
        assets.push_back(asset);
    }
    const auto total=first+gores.size();
    for(size_t i=0;i<pending.size();++i) {
        pending[i]=runtime::TypeTables::resizedCopy(original[i],total);
        if(!pending[i]||!roots.add(reinterpret_cast<Il2CppObject*>(pending[i])))return fail("Array","ResizeFailed:"+std::string(names[i]));
    }
    auto* arrays=il2cpp::findClass({"System","Array",{}});
    const auto* set=arrays?il2cpp::findMethodBySignature(arrays,
        il2cpp::parseSignature("void SetValue(object value, int index)")):nullptr;
    if(!set||!il2cpp::methodPointer(set))
        return fail("ABI","GoreAssetSetValueMissing");
    const auto putNumber=[&](size_t arrayIndex,int index,int number)->bool {
        auto* array=pending[arrayIndex];
        if(!array||static_cast<uintptr_t>(index)>=array->length)return false;
        const std::string type=elemName(array);
        auto* bytes=static_cast<char*>(arrayData(array));
        if(type=="Byte"||type=="Boolean") {
            if(number<0||number>255)return false;
            bytes[index]=static_cast<char>(number);
            return true;
        }
        if(type=="Int32") {
            int value=number;
            std::memcpy(bytes+static_cast<size_t>(index)*sizeof(value),&value,sizeof(value));
            return true;
        }
        if(type=="Single") {
            float value=static_cast<float>(number);
            std::memcpy(bytes+static_cast<size_t>(index)*sizeof(value),&value,sizeof(value));
            return true;
        }
        return false;
    };
    for(size_t i=0;i<gores.size();++i) {
        int index=static_cast<int>(first+i);
        Il2CppObject* exception=nullptr;
        void* args[]={assets[i],&index};
        a.runtime_invoke(set,reinterpret_cast<Il2CppObject*>(pending[0]),args,&exception);
        if(exception||!putNumber(1,index,gores[i].specialAI)||
           !putNumber(2,index,1)||!putNumber(3,index,1)||
           !putNumber(4,index,0)||!putNumber(5,index,gores[i].safeGore?1:0))
            return fail("Publish","GoreValuesFailed:"+gores[i].name);
    }
    if(!table_publication::publish(a,fields.data(),original.data(),pending.data(),
                                   fields.size(),roots))
        return fail("Publish","GoreTablesNotConfirmed");
    std::vector<gore::DeclaredGore> declared;
    for(size_t i=0;i<gores.size();++i)
        declared.push_back({gores[i].name,static_cast<int>(first+i),assets[i],
                            gores[i].specialAI,gores[i].safeGore});
    if(!gore::adoptDeclared(modId,declared,error))return false;
    for(size_t i=0;i<gores.size();++i)
        ids->emplace(gores[i].name,static_cast<int>(first+i));
    BL_INFO("[CSharp][GoreLoader] Mod=%s Registered=%zu First=%zu Result=Ready",
            modId.c_str(),gores.size(),static_cast<size_t>(first));
    return true;
}

bool publishManagedNativeTrees(const std::string& modId,const std::string& directory,
                               const std::vector<ManagedTreeEntry>& trees,std::string* error) {
    if(!error)return false;
    const auto fail=[&](const std::string& stage,const std::string& detail) {
        *error="[CSharp][TreeLoader] Mod="+modId+" Stage="+stage+
               " Result=Rejected Reason="+detail;
        BL_ERROR("%s",error->c_str());
        return false;
    };
    if(trees.empty())return true;
    if(trees.size()>255||runtime::modTreeCount()+static_cast<int>(trees.size())>255)
        return fail("Preflight","CapacityExceeded");
    auto& a=il2cpp::api();
    table_publication::Roots roots(a);
    if(!a.field_static_get_value||!a.field_static_set_value||
       !a.field_get_flags||!a.class_get_rank||!a.class_is_valuetype||
       !a.runtime_invoke||!a.object_get_class||!a.array_new||
       !a.gchandle_new||!a.gchandle_get_target||!a.gchandle_free)
        return fail("ABI","MissingIl2CppFunctions");
    auto* textures=il2cpp::findClass({"Terraria.GameContent","TextureAssets",{}});
    auto* fieldTop=textures?il2cpp::findField(textures,"TreeTop"):nullptr;
    auto* fieldBranch=textures?il2cpp::findField(textures,"TreeBranch"):nullptr;
    Il2CppArray* originalTop=nullptr;
    Il2CppArray* originalBranch=nullptr;
    if(!fieldTop||!fieldBranch||!(a.field_get_flags(fieldTop)&0x10u)||
       !(a.field_get_flags(fieldBranch)&0x10u))return fail("ABI","TextureTablesUnavailable");
    a.field_static_get_value(fieldTop,&originalTop);
    a.field_static_get_value(fieldBranch,&originalBranch);
    if(!originalTop||!originalBranch||
       a.class_get_rank(originalTop->obj.klass)!=1||
       a.class_get_rank(originalBranch->obj.klass)!=1||
       originalTop->length!=originalBranch->length||
       originalTop->length+trees.size()>32767)
        return fail("ABI","TextureTablesMismatch");
    if(!roots.add(reinterpret_cast<Il2CppObject*>(originalTop))||
       !roots.add(reinterpret_cast<Il2CppObject*>(originalBranch)))
        return fail("GC","OriginalTreeTableRootFailed");
    auto* sysArray=il2cpp::findClass({"System","Array",{}});
    const auto* arraySet=sysArray?il2cpp::findMethodBySignature(sysArray,
        il2cpp::parseSignature("void SetValue(object value, int index)")):nullptr;
    if(!arraySet||!il2cpp::methodPointer(arraySet))
        return fail("ABI","ArrayWriteUnavailable");
    if(!runtime::prepareModTreeRendering())
        return fail("BindRendering","MissingNativeTreeDrawOrGrowthHooks");
    if(!runtime::prepareModTreeDrops(true))
        return fail("BindTreeDrops","MissingNativeDropsAcornOrRandomABI");
    if(!runtime::prepareModTreePlanting())
        return fail("BindSaplingPlanting","MissingTileObjectCanPlaceABI");
    if(!runtime::prepareModTreeEffects())
        return fail("BindTreeEffects","MissingGetTreeLeafABI");
    const bool needsShake=std::any_of(trees.begin(),trees.end(),[](const ManagedTreeEntry& e) {
        return e.shakeItemType>=0;
    });
    if(needsShake&&!runtime::prepareModTreeShake())
        return fail("BindTreeShake","MissingNativeShakeABI");
    const bool needsShakeEffects=std::any_of(trees.begin(),trees.end(),[](const ManagedTreeEntry& e) {
        return e.shakeItemType>=0 && e.shakeLeaves && e.leafGoreType>=0;
    });
    if(needsShakeEffects&&!runtime::prepareModTreeShakeEffects())
        return fail("BindTreeShake","MissingNativeShakeEffectsABI");
    std::map<int,bool> uniqueGround;
    struct Ready {
        int ground=-1;
        int sapling=-1;
        std::array<std::string,3> paths;
        Il2CppObject* trunk=nullptr;
        Il2CppObject* top=nullptr;
        Il2CppObject* branch=nullptr;
    };
    std::vector<Ready> ready;
    ready.reserve(trees.size());
    for(const auto& tree:trees) {
        const int ground=runtime::modTileTypeByName(modId,tree.groundTile);
        const int sapling=runtime::modTileTypeByName(modId,tree.saplingTile);
        if(ground<runtime::kVanillaTileCount||sapling<runtime::kVanillaTileCount||
           !uniqueGround.emplace(ground,true).second||
           runtime::hasModTree(ground)||!tree_texture::validStem(tree.texture))
            return fail("Resolve","GroundOrSaplingNotOwnedOrDuplicate");
        const auto files=tree_texture::paths(directory,tree.texture);
        for(const auto& path:files) {
            if(path.empty()||!fileExists(path))
                return fail("Files","MissingTreeTexture:"+tree.name);
        }
        ready.push_back({ground,sapling,files,nullptr,nullptr,nullptr});
    }
    for(size_t i=0;i<ready.size();++i) {
        auto& tree=ready[i];
        int width[3]={},height[3]={};
        Il2CppObject* assets[3]={};
        for(int part=0;part<3;++part) {
            assets[part]=runtime::content::loadTextureAsset(
                tree.paths[part],nullptr,0,
                modId+"/"+trees[i].name+(part==1?"_Tops":part==2?"_Branches":""),
                &width[part],&height[part]);
            if(!assets[part]||!roots.add(assets[part]))return fail("Asset","LoadFailed:"+trees[i].name);
        }
        if(!tree_texture::validDimensions(width[0],height[0],width[1],height[1],
                                           width[2],height[2]))
            return fail("Asset","InvalidDimensions:"+trees[i].name);
        tree.trunk=runtime::content::textureAssetValue(assets[0]);
        tree.top=assets[1];
        tree.branch=assets[2];
        if(!tree.trunk)return fail("Asset","TrunkTextureNotLoaded:"+trees[i].name);
    }
    auto* newTop=runtime::TypeTables::resizedCopy(originalTop,
        originalTop->length+ready.size());
    if(!newTop||!roots.add(reinterpret_cast<Il2CppObject*>(newTop)))
        return fail("GC","TreeTopRootFailed");
    auto* newBranch=runtime::TypeTables::resizedCopy(originalBranch,
        originalBranch->length+ready.size());
    if(!newBranch||!roots.add(reinterpret_cast<Il2CppObject*>(newBranch)))
        return fail("GC","TreeBranchRootFailed");
    const auto set=[&](Il2CppArray* array,int index,Il2CppObject* asset) {
        Il2CppObject* exception=nullptr;
        void* args[]={asset,&index};
        a.runtime_invoke(arraySet,reinterpret_cast<Il2CppObject*>(array),args,&exception);
        return exception==nullptr;
    };
    for(size_t i=0;i<ready.size();++i) {
        const int index=static_cast<int>(originalTop->length+i);
        if(!set(newTop,index,ready[i].top)||
           !set(newBranch,index,ready[i].branch))
            return fail("Asset","ArrayWriteFailed");
    }
    FieldInfo* fields[]={fieldTop,fieldBranch};
    Il2CppArray* originals[]={originalTop,originalBranch};
    Il2CppArray* replacements[]={newTop,newBranch};
    if(!table_publication::publish(a,fields,originals,replacements,2,roots))
        return fail("Publish","TreeTablesNotConfirmed");
    for(size_t i=0;i<ready.size();++i) {
        const int style=static_cast<int>(originalTop->length+i);
        if(!runtime::registerModTreeChecked(ready[i].ground,ready[i].trunk,style)||
           !runtime::configureModTreeSapling(ready[i].ground,ready[i].sapling)||
           !runtime::configureModTreeDrops(ready[i].ground,trees[i].woodItemType,
                                            trees[i].canDropAcorn)||
           !runtime::configureModTreeEffects(ready[i].ground,trees[i].leafGoreType,
                                              trees[i].dustType)||
           (trees[i].shakeItemType>=0&&
            !runtime::configureModTreeShake(ready[i].ground,trees[i].shakeItemType,
                                             trees[i].shakeLeaves)))
            return fail("Publish","TreeRegistrationOrDropsFailed:"+trees[i].name);
    }
    BL_INFO("[CSharp][TreeLoader] Mod=%s Trees=%zu Stage=Publish Result=Ready",
            modId.c_str(),ready.size());
    return true;
}

int registerNativeContent(void* context, const NativeContentSpec* spec) {
    auto* ctxPtr = static_cast<NativeRegisterContext*>(context);
    if (ctxPtr) ctxPtr->lastError.clear();
    auto reject = [&](const std::string& reason, const std::string& expected,
                      const std::string& actual) {
        const std::string detail = "[CSharp][ERROR][Register] Mod=" +
            (ctxPtr ? ctxPtr->modId : "<unavailable>") + " Class=" +
            (spec && spec->name ? spec->name : "<unavailable>") + " File=" +
            (ctxPtr ? ctxPtr->sourceFile : "<unavailable>") +
            " Stage=NativePreflight Expected=" + expected + " Actual=" + actual +
            " Result=Rejected Reason=" + reason;
        if (ctxPtr) ctxPtr->lastError = detail;
        BL_ERROR("%s", detail.c_str());
        return -1;
    };
    if (!ctxPtr) return reject("RegistrationContextMissing", "Context", "Null");
    if (!spec) return reject("ContentSpecMissing", "ContentSpec", "Null");
    if (!g_ready) return reject("NativeRuntimeNotReady", "Ready", "NotReady");
    if (!validName(spec->name))
        return reject("ContentNameInvalid", "ValidContentName", spec->name ? spec->name : "Null");
    std::string texture, textureError;
    if (!normalizeContentTexture(spec->texture, &texture, &textureError))
        return reject(textureError, "RelativeContentPngOrStem", spec->texture ? spec->texture : "Null");
    auto& ctx = *ctxPtr;
    const std::string path = ctx.modDir + "/content/" + texture;
    if (!fileExists(path)) {
        ctx.lastError="[CSharp][ERROR][Asset] Mod="+ctx.modId+" Class="+spec->name+
            " Path="+path+" Stage=TextureLoad Expected=ExistingFile Actual=Missing Result=Rejected Reason=AssetMissing";
        BL_ERROR("%s",ctx.lastError.c_str());
        return -1;
    }
    const std::string display = spec->displayName && *spec->displayName
        ? std::string(spec->displayName).substr(0, 128)
        : std::string(spec->name);
    if (spec->kind == NativeWall) {
        std::string wallError;
        const int type=runtime::registerModWall({ctx.modId,spec->name,path},&wallError);
        if(type<0){ctx.lastError="[CSharp][ERROR][WallRegistration] Mod="+ctx.modId+" Class="+spec->name+" File="+ctx.sourceFile+" Stage=NativeRegister Result=Rejected Detail="+wallError;BL_ERROR("%s",ctx.lastError.c_str());return -1;}
        ++ctx.walls;
        return type;
    }
    if (spec->kind == NativeTile) {
        bl::runtime::ModTileDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        const int type = bl::runtime::registerModTile(std::move(def));
        if (type < 0)
            return reject("DuplicateNativeTile", "UniqueModTileKey", spec->name);
        ++ctx.tiles;
        return type;
    }
    if (spec->kind == NativeBasicItem) {
        std::string contractError;
        if(!checkBasicItemContract(&contractError)) {
            ctx.lastError="[CSharp][ERROR][NativeItem] Mod="+ctx.modId+" Class="+spec->name+
                " Stage=Register Result=Rejected Detail="+contractError;
            BL_ERROR("%s",ctx.lastError.c_str());return -1;
        }
        bl::runtime::ModItemDef def{};
        def.mod=ctx.modId;def.name=spec->name;def.texture=path;
        def.names=ctx.sourceNames;def.names.push_back({"",display});
        const int type=bl::runtime::registerModItem(std::move(def));
        std::string error;
        if (type < 0 || !registerBasicItemPlaceholder(type,spec->name,&error,ctx.modDir,ctx.sourceFile)) {
            ctx.lastError="[CSharp][ERROR][NativeItem] Mod="+ctx.modId+" Class="+spec->name+
                " Stage=Register Actual="+std::to_string(type)+" Result=Rejected Detail="+error;
            BL_ERROR("%s",ctx.lastError.c_str());return -1;
        }
        ++ctx.items;++ctx.basicItems;return type;
    }
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
        def.names=ctx.sourceNames;def.names.push_back({"", display});
        const int type = bl::runtime::registerModProjectile(std::move(def));
        if (type >= 0) {
            g_projectiles[type] = {spec->width, spec->height, spec->aiStyle,
                                   spec->penetrate, spec->timeLeft, true, false};
            publishProjectileDefinition(type,g_projectiles.at(type),ctx.modId+"/"+spec->name);
            ++ctx.projectiles;
        }
        return type;
    }
    if (spec->kind == NativeNpc) {
        if (!prepareNativeNpc()) return -1;
        if (spec->width < 8 || spec->width > 256 ||
            spec->height < 8 || spec->height > 256 ||
            spec->damage < 0 || spec->damage > 500 ||
            spec->useTime < 0 || spec->useTime > 200 ||
            spec->shootType < 1 || spec->shootType > 100000 ||
            spec->aiStyle < 0 || spec->aiStyle > 200 ||
            spec->penetrate < 0 || spec->penetrate >= bl::runtime::kVanillaNpcCount ||
            spec->timeLeft < 1 || spec->timeLeft > 30) return -1;
        bl::runtime::ModNpcDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names=ctx.sourceNames;def.names.push_back({"", display});
        def.frames = spec->timeLeft;
        def.animationType = spec->penetrate;
        const int type = bl::runtime::registerModNpc(std::move(def));
        if (type >= 0) {
            g_npcs[type] = {spec->width, spec->height, spec->damage,
                            spec->useTime, spec->shootType, spec->aiStyle};
            if (ctx.firstNpcType < 0) ctx.firstNpcType = type;
            ++ctx.npcs;
        }
        return type;
    }
    if (spec->kind == NativeBuff) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->damage < -50 || spec->damage > 100) return -1;
        bl::runtime::ModBuffDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names=ctx.sourceNames;def.names.push_back({"", display});
        def.managedFlags = true;
        def.debuff = (spec->flags & kNativeBuffDebuff) != 0;
        def.noSave = (spec->flags & kNativeBuffNoSave) != 0;
        def.noTimeDisplay = (spec->flags & kNativeBuffNoTimeDisplay) != 0;
        def.nurseCannotRemove=(spec->flags & kNativeBuffNurseCannotRemove)!=0;
        const int type = bl::runtime::registerModBuff(std::move(def));
        if (type >= 0) {
            g_buffs[type] = {spec->damage};
            publishBuffDefaults();
            if (ctx.firstBuffType < 0) ctx.firstBuffType = type;
            ++ctx.buffs;
        }
        return type;
    }
    if (spec->kind == NativeAccessory) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->damage < 0 || spec->damage > 100) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names=ctx.sourceNames;def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_accessories[type] = {spec->damage, 0, 0, false, ""};
            if (ctx.firstAccessoryType < 0) ctx.firstAccessoryType = type;
            ++ctx.items;
            ++ctx.accessories;
        }
        return type;
    }
    if (spec->kind == NativeBuffPotion) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->timeLeft < 1 || spec->timeLeft > 216000 ||
            bl::runtime::modBuffTypeByName(ctx.modId, spec->name) >= 0) return -1;
        if (g_buffs.count(spec->shootType) == 0) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names=ctx.sourceNames;def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_potions[type] = {spec->shootType, spec->timeLeft};
            ++ctx.items;
            ++ctx.potions;
        }
        return type;
    }
    if (spec->kind == NativeWeapon)
        return registerNativeWeapon(ctx, *spec, path, display);
    return -1;
}

}


