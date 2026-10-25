#pragma once

#include <cstdint>
#include "experiments/csharp/ProjectilePolicy.h"
#include "experiments/csharp/SummonPolicy.h"
#include "experiments/csharp/NativeTileDropPolicy.h"
#include <string>
#include <vector>
#include <map>

namespace bl::csharp {
bool nativeCostumeRuntimeReady();

enum NativeContentKind : int32_t {
    NativeProjectile = 1,
    NativeWeapon = 2,
    NativeNpc = 3,
    NativeAccessory = 4,
    NativeBuff = 5,
    NativeBuffPotion = 6,
    NativeBasicItem = 7,
    NativeTile = 8,
    NativeWall = 9,
    NativeMount = 10
};

struct NativeContentSpec {
    int32_t kind;
    const char* name;
    const char* texture;
    const char* displayName;
    int32_t width;
    int32_t height;
    int32_t damage;
    int32_t useTime;
    float shootSpeed;
    int32_t shootType;
    int32_t aiStyle;
    int32_t penetrate;
    int32_t timeLeft;
    int32_t flags;            // NativeBuff: kNativeBuffDebuff | kNativeBuffNoSave | kNativeBuffNoTimeDisplay
    bool summon = false;
};

constexpr int32_t kNativeBuffDebuff = 1;
constexpr int32_t kNativeBuffNoSave = 2;
constexpr int32_t kNativeBuffNoTimeDisplay = 4;
constexpr int32_t kNativeBuffNurseCannotRemove = 8;

struct NativeRegisterContext {
    std::string modId;
    std::string modDir;
    int items;
    int projectiles;
    int npcs = 0;
    int accessories = 0;
    int buffs = 0;
    int potions = 0;
    int basicItems = 0;
    int tiles = 0;
    int walls = 0;
    int firstNpcType = -1;
    int firstAccessoryType = -1;
    int firstBuffType = -1;
    std::string lastError = "";
    std::string sourceFile = "";
    std::vector<std::pair<std::string,std::string>> sourceNames{};
};

using NativeRegisterCallback = int (*)(void*, const NativeContentSpec*);

struct ManagedTileEntry {
    std::string name;
    int type = -1;
    int flags = 0;
    int shape = 0;
    int frameToggle = 0;
    int coordinateHeight = 18;
    int adjacentTile = -1;
    std::vector<int> adjacentTiles;
    std::string dustName;
    int mapR = 200, mapG = 200, mapB = 200;
    std::string mapName;
    int failDust = 1, normalDust = 3;
    int animationFrameHeight = 0;
    int animationFrames = 1;
    int animationFrameTicks = 0;
    bool allowBreaking = true;
    bool allowDrops = true;
    int callbackMask = 0;
    bool cacheDrawData = true;
    std::vector<tile_drop::Rule> registeredDrops;
    int minPick = 0;
    float mineResist = 1.f;
    std::string saplingGroundTile;
    int saplingVanillaGroundTile = -1;
    int saplingStyleRange = 3;
    int saplingGrowthChance = 0;
};

struct ManagedTreeEntry {
    std::string name;
    std::string groundTile;
    std::string saplingTile;
    std::string texture;
    std::string woodItem;
    bool canDropAcorn=true;
    int woodItemType=-1;
    int leafGoreType=-1;
    std::string leafGoreName;
    int dustType=7;
    std::string shakeItem;
    int shakeItemType=-1;
    bool shakeLeaves=true;
};
struct ManagedGoreEntry {
    std::string name;
    std::string texture;
    int specialAI=0;
    bool safeGore=false;
};
bool publishManagedNativeGores(const std::string& modId,const std::string& directory,
    const std::vector<ManagedGoreEntry>& gores,std::map<std::string,int>* ids,
    std::string* error);

bool publishManagedNativeTrees(const std::string& modId,const std::string& directory,
    const std::vector<ManagedTreeEntry>& trees,std::string* error);

bool applyManagedNativeTileDefaultsBatch(const std::string& modId,
                                         const std::vector<ManagedTileEntry>& tiles);

struct ManagedProjectileDefaults {
    int width = 0;
    int height = 0;
    int aiStyle = 0;
    int penetrate = 0;
    int timeLeft = 0;
    bool friendly = true;
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
    SummonMovement movement{};
    int animationFrameTicks=6, minionScanTicks=8;
    float minionTargetRange=700.f;
};

struct ManagedWeaponDefaults {
    int damage = 0;
    int useTime = 0;
    float shootSpeed = 0.f;
};

bool applyManagedNativeDefaults(
    const std::string& modId, const std::string& projectileName, int projectileType,
    const ManagedProjectileDefaults& projectile, const std::string& weaponName,
    int weaponType, const ManagedWeaponDefaults& weapon);

struct ManagedProjectileEntry {
    std::string name;
    int type = -1;
    ManagedProjectileDefaults values;
};

struct ManagedWeaponEntry {
    std::string name;
    int type = -1;
    int shootType = -1;
    bool summon = false;
    std::string minionBuffName;
    int minionBuffType = -1;
    int minionBuffTime = 0;
    ManagedWeaponDefaults values;
};

bool applyManagedNativeDefaultsBatch(const std::string& modId,
                                     const std::vector<ManagedProjectileEntry>& projectiles,
                                     const std::vector<ManagedWeaponEntry>& weapons);
bool applyManagedNativeDefaultsBatch(const std::string& modId,
                                     const std::vector<ManagedProjectileEntry>& projectiles,
                                     const std::vector<ManagedWeaponEntry>& weapons,std::string* error);
int nativeManagedProjectileFrames(int type);
struct ManagedTileCollisionSettings {
    int mode = 0;
    float restitution = 1.f;
    float minimumSpeed = 0.f;
    int survivalLimit = 0;
    int platformMode = 0;
    int boxWidth = 0;
    int boxHeight = 0;
    float anchorOffsetX = 0.f;
    float anchorOffsetY = 0.f;
};
ManagedTileCollisionSettings nativeManagedProjectileTileCollisionSettings(int type);
ProjectilePolicy nativeManagedProjectilePolicy(int type);
bool nativeManagedProjectileFields(int type,ProjectileGameplayFields* fields);
int nativeManagedMinionBuff(int projectileType);
SummonMovement nativeManagedMinionMovement(int projectileType);
std::string nativeManagedProjectileKey(int type);
bool nativeManagedMinionContactEnabled(int projectileType);

struct ManagedBuffEntry {
    std::string name;
    int type = -1;
    int defenseBonus = 0;
    int minionProjectileType = -1;   // >= 0: buff de minion (renovado/removido pelo host)
    int flags=-1; std::string description{}; int defenseMultiplierPercent=100;
};

struct ManagedPotionEntry {
    std::string name;
    int type = -1;
    int buffType = -1;
    int duration = 600;
};

bool applyManagedNativeBuffDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedBuffEntry>& buffs,
    const std::vector<ManagedPotionEntry>& potions);

struct ManagedAccessoryEntry {
    std::string name;
    int type = -1;
    int defenseBonus = 0;
    std::string tooltip;
    int visualMode = 0;
    std::string visualTexture;
    float visualScale = 1.f;
    float visualOpacity = 1.f;
    uint32_t visualColor = 0xffffffffu;
};

struct ManagedPlayerEntry {
    std::string name;
    std::string accessoryName;
    int accessoryType = -1;
    int equippedDefenseBonus = 0;
    int moveSpeedPercent = 0;
    int runAccelerationPercent = 0;
    bool frameShadow = false;
    std::string worldJoinMessage;
    std::string respawnMessage;
    bool preUpdate = false;
    bool postUpdate = false;
    std::string deadMessage;
    int lifeRegenBonus = 0;
    int manaRegenBonus = 0;
};

bool bindManagedPlayerHurtMod(const std::string& modId,
                              const std::string& assemblyPath);
void pauseManagedPlayerHurtBindings();

bool showManagedPlayerWorldJoinMessages();

bool applyManagedNativePlayerDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedAccessoryEntry>& accessories,
    const std::vector<ManagedPlayerEntry>& players);

struct ManagedNpcEntry {
    std::string name;
    int type = -1;
    int width = 32;
    int height = 24;
    int damage = 7;
    int defense = 2;
    int lifeMax = 25;
    int aiStyle = 1;
    int animationType = 1;
    int frames = 2;
};

bool applyManagedNativeNpcDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedNpcEntry>& npcs);


struct ManagedNativeEquipTexture {
    int itemType = -1;
    int visualMode = 0;
    std::string texture;
};

std::vector<ManagedNativeEquipTexture> managedNativeEquipTextures();
size_t pendingManagedNativeEquipTextures();
bool setManagedNativeEquipSlot(int itemType, int visualMode, int equipSlot);

int managedNativeAccessoryDefense(int type);
int managedNativeAccessoryMoveSpeedPercent(int type);
int managedNativePrefixItemKind(int type);
void refreshManagedPrefixEligibilityOnWorldReady();
bool prepareNativeContent();
int registerNativeContent(void* context, const NativeContentSpec* spec);

}

