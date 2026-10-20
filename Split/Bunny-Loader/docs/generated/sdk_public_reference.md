# Public C# SDK reference

Generated from the compiled BunnySourceSdk. Update with `dotnet run --project tools/sdk_reference -c Release -- --write`.

Execution terms are defined in [the SDK guide](../../../../docs/SDK-CSharp.md). Public methods do not imply Android validation, arbitrary per-frame AI, or multiplayer support. Only members declared on each type are listed; inherited members follow its base type.

## BunnyLoader.Source.BunnyAccessException

class; base: `System.InvalidOperationException`.

```csharp
BunnyAccessException(BunnyAccessStatus status, string operation)
BunnyAccessStatus Status { get; }
```

## BunnyLoader.Source.BunnyAccessStatus

enum; base: `System.Enum`.

Values: Ok, Invalid, WrongThread, Capacity, NativeException, TimedOut, Busy, Expired, ReadOnly, TypeMismatch, Ambiguous, NotFound, Unavailable, Unsupported.

## BunnyLoader.Source.BunnyAccessory

abstract class; base: `BunnyLoader.Source.BunnyRecipeSource`.

```csharp
BunnyAccessoryVisualMode VisualMode { get; set; }
BunnyNativeDefaults NativeDefaults { get; }
UInt32 VisualColor { get; set; }
float VisualOpacity { get; set; }
float VisualScale { get; set; }
int DefenseBonus { get; set; }
string DisplayName { get; set; }
string NativeDefaultsDefinition { get; }
string Texture { get; set; }
string Tooltip { get; set; }
string VisualTexture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyAccessoryVisualMode

enum; base: `System.Enum`.

Values: None, EquippedSprite, EnergyBarrier, RunningTrail.

## BunnyLoader.Source.BunnyAfterHurtHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyAfterHurtHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyHurtInfo info, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(IAsyncResult result)` | managed_helper |
| `void Invoke(BunnyHurtInfo info)` | managed_helper |

## BunnyLoader.Source.BunnyArmorEffects

class; base: `System.Object`.

```csharp
BunnyArmorEffects()
```

| Method | Execution |
| --- | --- |
| `void Add(BunnyArmorInteger field, int value)` | managed_helper |
| `void Add(BunnyArmorStat field, float value)` | managed_helper |
| `void ImmuneTo(string vanillaBuffName)` | managed_helper |
| `void Multiply(BunnyArmorStat field, float value)` | managed_helper |

## BunnyLoader.Source.BunnyArmorInteger

enum; base: `System.Enum`.

Values: StatManaMax2, MaxMinions, StatDefense, LifeRegen.

## BunnyLoader.Source.BunnyArmorSet

class; base: `System.Object`.

```csharp
BunnyArmorSet(string head, string body, string legs, string text)
BunnyArmorEffects Effects { get; }
```

## BunnyLoader.Source.BunnyArmorStat

enum; base: `System.Enum`.

Values: MeleeDamage, RangedDamage, MagicDamage, MinionDamage, MoveSpeed.

## BunnyLoader.Source.BunnyBeforeHurtHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyBeforeHurtHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(ref int damage, ref int hitDirection, ref bool cancel, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(ref int damage, ref int hitDirection, ref bool cancel, IAsyncResult result)` | managed_helper |
| `void Invoke(ref int damage, ref int hitDirection, ref bool cancel)` | managed_helper |

## BunnyLoader.Source.BunnyBuff

abstract class; base: `System.Object`.

```csharp
bool IsDebuff { get; set; }
bool NoSave { get; set; }
bool NoTimeDisplay { get; set; }
bool NurseCannotRemove { get; set; }
int DefenseBonus { get; set; }
int DefenseMultiplierPercent { get; set; }
string Description { get; set; }
string DisplayName { get; set; }
string MinionProjectile { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyBuffPotion

abstract class; base: `BunnyLoader.Source.BunnyRecipeSource`.

```csharp
BunnyNativeDefaults NativeDefaults { get; }
int Duration { get; set; }
string Buff { get; set; }
string DisplayName { get; set; }
string NativeDefaultsDefinition { get; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyColor

struct; base: `System.ValueType`.

```csharp
BunnyColor(Byte red, Byte green, Byte blue, Byte alpha)
UInt32 PackedRgba { get; }
```

| Method | Execution |
| --- | --- |
| `static BunnyColor FromPackedRgba(UInt32 value)` | managed_helper |

## BunnyLoader.Source.BunnyCombat

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static BunnyHurtSource SourceOf(int npcIndex, int projectileIndex)` | managed_helper |
| `static bool HasSpatialContext(BunnyHurtInfo hit)` | managed_helper |
| `static bool HasSpatialContext(BunnyHurtModifiers hit)` | managed_helper |
| `static bool IsInArc(float angle, float center, float width)` | managed_helper |
| `static bool IsLowLife(int life, int maxLife, int thresholdPercent)` | managed_helper |
| `static bool IsWithinDistance(float distance, float maximumPixels)` | managed_helper |
| `static float NormalizeAngle(float radians)` | managed_helper |
| `static float Speed(float velocityX, float velocityY)` | managed_helper |
| `static int CapDamage(int damage, int maximum)` | managed_helper |
| `static int LifePercent(int life, int maxLife)` | managed_helper |
| `static int ReduceDamage(int damage, int percent, int minimum)` | managed_helper |

## BunnyLoader.Source.BunnyContentAttribute

class; base: `System.Attribute`.

```csharp
BunnyContentAttribute(BunnyContentKind kind, string displayName, string texture)
BunnyContentKind Kind { get; }
string DisplayName { get; }
string Texture { get; }
```

## BunnyLoader.Source.BunnyContentKind

enum; base: `System.Enum`.

Values: Item, Tile, Weapon, Projectile, Buff, BuffPotion, Accessory, Player, NPC, Prefix, Wall.

## BunnyLoader.Source.BunnyCraftContext

class; base: `System.Object`.

```csharp
BunnyRecipeStage Stage { get; }
bool InDesert { get; }
bool InSnow { get; }
bool IsDay { get; }
bool NearWater { get; }
bool QuickCraft { get; }
int AvailableIngredientCount { get; }
int CurrentLife { get; }
int MaxLife { get; }
int ResultType { get; }
```

| Method | Execution |
| --- | --- |
| `void GiveItem(int vanillaItemId, int count)` | worker_craft_command_queue |
| `void GiveItem(string modItemName, int count)` | worker_craft_command_queue |
| `void GiveItem<TItem>(int count)` | worker_craft_command_queue |

## BunnyLoader.Source.BunnyDodgeHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyDodgeHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(int damage, out bool dodge, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(out bool dodge, IAsyncResult result)` | managed_helper |
| `void Invoke(int damage, out bool dodge)` | managed_helper |

## BunnyLoader.Source.BunnyEntitySourceDispatcher

abstract class; base: `System.Object`.

```csharp
string LastRegistrationError { get; }
```

| Method | Execution |
| --- | --- |
| `static Int64 TickNpc(int modSlot, int type, int slot, int generation, int life, int lifeMax, float x, float y, float vx, float vy, float playerX, float playerY)` | worker_async_entity |
| `static Int64 TickProjectile(int modSlot, int type, int slot, int generation, int owner, float x, float y, float vx, float vy, float playerX, float playerY)` | worker_async_entity |
| `static Int64 TickProjectile(int modSlot, int type, int slot, int generation, int owner, float x, float y, float vx, float vy, float playerX, float playerY, int targetNpc, float targetX, float targetY)` | worker_async_entity |
| `static bool Bind(int modSlot, int kind, int type, string name)` | worker_async_entity |
| `static bool HitNpc(int modSlot, int type, int slot, int generation, int damage, bool killed)` | worker_async_entity |
| `static bool HitProjectile(int modSlot, int type, int slot, int generation, int owner, int npcIndex, int damage)` | worker_async_entity |
| `static bool KillNpc(int modSlot, int type, int slot, int generation)` | worker_async_entity |
| `static bool KillProjectile(int modSlot, int type, int slot, int generation, int owner)` | worker_async_entity |
| `static int Register(string modId, string assemblyPath)` | worker_async_entity |
| `static string Status(int modSlot)` | worker_async_entity |
| `static void ResetWorld()` | worker_async_entity |

## BunnyLoader.Source.BunnyEquipDrawFlags

enum; base: `System.Enum`.

Values: None, UseHairColor, ShowHands.

## BunnyLoader.Source.BunnyEquipFlag

enum; base: `System.Enum`.

Values: StrongBees.

## BunnyLoader.Source.BunnyEquipFloat

enum; base: `System.Enum`.

Values: MeleeDamage, RangedDamage, MagicDamage, MinionDamage, MoveSpeed.

## BunnyLoader.Source.BunnyEquipKind

enum; base: `System.Enum`.

Values: Head, Body, Legs, Beard, Back, Wings.

## BunnyLoader.Source.BunnyHoldout

class; base: `System.Object`.

```csharp
BunnyHoldout(float x, float y)
float X { get; }
float Y { get; }
```

## BunnyLoader.Source.BunnyHookHandle

struct; base: `System.ValueType`.

```csharp
BunnyHookPhase Phase { get; }
bool IsValid { get; }
int Id { get; }
```

## BunnyLoader.Source.BunnyHookModule

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `string DiagnosticStatus()` | experimental_worker_sync_void0 |
| `void Load(BunnyHookScope hooks)` | experimental_worker_sync_void0 |
| `void Unload()` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyHookPhase

enum; base: `System.Enum`.

Values: BeforeHurt, Dodge, AfterHurt.

## BunnyLoader.Source.BunnyHookRegistry

class; base: `System.Object`.

```csharp
BunnyHookRegistry()
Int64 AfterCalls { get; }
Int64 BeforeCalls { get; }
Int64 CancelledByHook { get; }
Int64 DodgeChecks { get; }
Int64 FaultedHooks { get; }
Int64 SuccessfulDodges { get; }
Int64 UnhookCalls { get; }
bool HasLastDamage { get; }
bool LastEquipped { get; }
int Count { get; }
int LastDamageAfter { get; }
int LastDamageBefore { get; }
string LastHookError { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyHookHandle HookAfterHurt(BunnyAfterHurtHook handler)` | managed_helper |
| `BunnyHookHandle HookBeforeHurt(BunnyBeforeHurtHook handler)` | managed_helper |
| `BunnyHookHandle HookDodge(BunnyDodgeHook handler)` | managed_helper |
| `bool InvokeDodge(int damage)` | managed_helper |
| `bool Unhook(BunnyHookHandle handle)` | managed_helper |
| `void Clear()` | managed_helper |
| `void InvokeAfter(BunnyHurtInfo info)` | managed_helper |
| `void InvokeBefore(ref int damage, ref int hitDirection, ref bool cancel)` | managed_helper |

## BunnyLoader.Source.BunnyHookScope

class; base: `System.Object`.

```csharp
BunnyHookScope()
BunnyNativeScope Native { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyMethodHook HookInt32(string type, string method, BunnyInt32Hook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookInt32Pair(string type, string method, BunnyInt32PairHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookInt32Triple(string type, string method, BunnyInt32TripleHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookInt32Zero(string type, string method, BunnyInt32ZeroHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookVoid(string type, string method, BunnyVoidHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookVoidInt32(string type, string method, BunnyVoidInt32Hook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookVoidInt32Pair(string type, string method, BunnyVoidInt32PairHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `BunnyMethodHook HookVoidInt32Triple(string type, string method, BunnyVoidInt32TripleHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `void Dispose()` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyHooks

abstract class; base: `System.Object`.

```csharp
Int64 Calls { get; }
Int64 Faults { get; }
Int64 TransientRetries { get; }
string LastError { get; }
```

| Method | Execution |
| --- | --- |
| `static BunnyMethodHook Hook(BunnyHookScope scope, string type, string method, BunnyVoidHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookInt32(BunnyHookScope scope, string type, string method, BunnyInt32Hook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookInt32Pair(BunnyHookScope scope, string type, string method, BunnyInt32PairHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookInt32Triple(BunnyHookScope scope, string type, string method, BunnyInt32TripleHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookInt32Zero(BunnyHookScope scope, string type, string method, BunnyInt32ZeroHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookVoidInt32(BunnyHookScope scope, string type, string method, BunnyVoidInt32Hook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookVoidInt32Pair(BunnyHookScope scope, string type, string method, BunnyVoidInt32PairHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static BunnyMethodHook HookVoidInt32Triple(BunnyHookScope scope, string type, string method, BunnyVoidInt32TripleHook callback, bool isStatic)` | experimental_worker_sync_void0 |
| `static int Dispatch(int slot, Int64 ticket, IntPtr self)` | experimental_worker_sync_void0 |
| `static string GetStatus()` | experimental_worker_sync_void0 |
| `static void Invoke(int slot, Int64 ticket, IntPtr self, out int result)` | experimental_worker_sync_void0 |
| `static void InvokeExtended(int slot, Int64 ticket, IntPtr self, int first, int second, int third, int mode, out int success, out int returnedValue)` | experimental_worker_sync_void0 |
| `static void InvokeInt32(int slot, Int64 ticket, IntPtr self, int value, out int success, out int returnedValue)` | experimental_worker_sync_void0 |
| `static void InvokeInt32Pair(int slot, Int64 ticket, IntPtr self, int first, int second, out int success, out int returnedValue)` | experimental_worker_sync_void0 |
| `static void RemoveSlot(int slot)` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyHurtInfo

struct; base: `System.ValueType`.

```csharp
BunnyHurtInfo(int damage, int sourceDamage, int hitDirection, int lifeRemaining, bool pvp, bool crit, int npcIndex, int projectileIndex, int npcType, int npcDamage, int npcLife, int projectileType, int projectileDamage, int projectileOwner, float attackerX, float attackerY, float attackerVelocityX, float attackerVelocityY, float attackerAngle, float attackerDistance)
bool Crit { get; }
bool PvP { get; }
float AttackerAngle { get; }
float AttackerDistance { get; }
float AttackerVelocityX { get; }
float AttackerVelocityY { get; }
float AttackerX { get; }
float AttackerY { get; }
int Damage { get; }
int HitDirection { get; }
int LifeRemaining { get; }
int NpcDamage { get; }
int NpcIndex { get; }
int NpcLife { get; }
int NpcType { get; }
int ProjectileDamage { get; }
int ProjectileIndex { get; }
int ProjectileOwner { get; }
int ProjectileType { get; }
int SourceDamage { get; }
```

## BunnyLoader.Source.BunnyHurtModifiers

class; base: `System.Object`.

```csharp
BunnyHurtModifiers()
bool Cancelled { get; }
bool Crit { get; set; }
bool Dodgeable { get; set; }
bool PvP { get; set; }
bool Quiet { get; set; }
float AttackerAngle { get; set; }
float AttackerDistance { get; set; }
float AttackerVelocityX { get; set; }
float AttackerVelocityY { get; set; }
float AttackerX { get; set; }
float AttackerY { get; set; }
int CurrentLife { get; set; }
int Damage { get; set; }
int HitDirection { get; set; }
int MaxLife { get; set; }
int NpcDamage { get; set; }
int NpcIndex { get; set; }
int NpcLife { get; set; }
int NpcType { get; set; }
int ProjectileDamage { get; set; }
int ProjectileIndex { get; set; }
int ProjectileOwner { get; set; }
int ProjectileType { get; set; }
```

| Method | Execution |
| --- | --- |
| `void Cancel()` | managed_helper |

## BunnyLoader.Source.BunnyHurtSource

enum; base: `System.Enum`.

Values: Unknown, Npc, Projectile.

## BunnyLoader.Source.BunnyInt32Hook

class; base: `System.MulticastDelegate`.

```csharp
BunnyInt32Hook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalInt32 original, IntPtr self, int value, AsyncCallback callback, Object object)` | managed_helper |
| `int EndInvoke(IAsyncResult result)` | managed_helper |
| `int Invoke(BunnyOriginalInt32 original, IntPtr self, int value)` | managed_helper |

## BunnyLoader.Source.BunnyInt32PairHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyInt32PairHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalInt32 original, IntPtr self, int first, int second, AsyncCallback callback, Object object)` | managed_helper |
| `int EndInvoke(IAsyncResult result)` | managed_helper |
| `int Invoke(BunnyOriginalInt32 original, IntPtr self, int first, int second)` | managed_helper |

## BunnyLoader.Source.BunnyInt32TripleHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyInt32TripleHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalInt32 original, IntPtr self, int first, int second, int third, AsyncCallback callback, Object object)` | managed_helper |
| `int EndInvoke(IAsyncResult result)` | managed_helper |
| `int Invoke(BunnyOriginalInt32 original, IntPtr self, int first, int second, int third)` | managed_helper |

## BunnyLoader.Source.BunnyInt32ZeroHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyInt32ZeroHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalInt32 original, IntPtr self, AsyncCallback callback, Object object)` | managed_helper |
| `int EndInvoke(IAsyncResult result)` | managed_helper |
| `int Invoke(BunnyOriginalInt32 original, IntPtr self)` | managed_helper |

## BunnyLoader.Source.BunnyItem

abstract class; base: `BunnyLoader.Source.BunnyShootSource`.

```csharp
BunnyHoldout Holdout { get; set; }
BunnyItemBehavior Behavior { get; }
BunnyNativeDefaults NativeDefaults { get; }
BunnySoundStyle CustomUseSound { get; set; }
Dictionary<string, string> TooltipTranslations { get; }
bool Accessory { get; set; }
bool AutoReuse { get; set; }
bool Consumable { get; set; }
bool Melee { get; set; }
bool NoMelee { get; set; }
bool UseTurn { get; set; }
bool Vanity { get; set; }
float KnockBack { get; set; }
int Axe { get; set; }
int Crit { get; set; }
int Damage { get; set; }
int Defense { get; set; }
int Hammer { get; set; }
int Height { get; set; }
int MaxStack { get; set; }
int Pick { get; set; }
int Rare { get; set; }
int UseAnimation { get; set; }
int UseStyle { get; set; }
int UseTime { get; set; }
int Value { get; set; }
int Width { get; set; }
string BehaviorDefinition { get; }
string DisplayName { get; set; }
string NativeAccessoryEffectsDefinition { get; }
string NativeArmorDefinition { get; }
string NativeDefaultsDefinition { get; }
string PlacementDefinition { get; }
string Texture { get; set; }
string Tooltip { get; set; }
string UseSound { get; set; }
```

| Method | Execution |
| --- | --- |
| `static int BuyPrice(int platinum, int gold, int silver, int copper)` | managed_helper |
| `static int SellPrice(int platinum, int gold, int silver, int copper)` | managed_helper |
| `void DefaultToPlaceableTile(int vanillaTileId, int style)` | managed_helper |
| `void DefaultToPlaceableTile(string modTileName, int style)` | managed_helper |
| `void DefaultToPlaceableWall(int vanillaWallId)` | managed_helper |
| `void DefaultToPlaceableWall(string modWallName)` | managed_helper |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyItemBehavior

class; base: `System.Object`.

```csharp
BunnyItemBehavior()
BunnyArmorEffects Armor { get; }
bool ColorFromPlayerHair { get; set; }
bool Robe { get; set; }
int Defense { get; set; }
string NativeAccessoryEffectsDefinition { get; }
string NativeArmorDefinition { get; }
```

| Method | Execution |
| --- | --- |
| `void AddOnEquip(BunnyEquipFloat field, float value)` | managed_helper |
| `void Animate(int frames, int ticksPerFrame, bool animatesAsSoul, bool pulse, bool noGravity)` | managed_helper |
| `void CloneVanillaDefaults(string itemIdName)` | managed_helper |
| `void DefineArmorSet(BunnyArmorSet set)` | managed_helper |
| `void EquipTexture(BunnyEquipKind kind, string texture, BunnyEquipDrawFlags flags)` | managed_helper |
| `void MultiplyOnEquip(BunnyEquipFloat field, float multiplier)` | managed_helper |
| `void SetOnEquip(BunnyEquipFlag flag)` | managed_helper |
| `void SetWingStats(int flyTime, float flySpeed, float acceleration, float ascentWhenFalling, float ascentWhenRising, float maxCanAscendMultiplier, float maxAscentMultiplier, float constantAscend)` | managed_helper |
| `void WorldAlpha(Byte red, Byte green, Byte blue, Byte opacity)` | managed_helper |
| `void WorldLight(float red, float green, float blue, bool scaleByEssence)` | managed_helper |

## BunnyLoader.Source.BunnyMethodHook

class; base: `System.Object`.

```csharp
bool IsDisposed { get; }
```

| Method | Execution |
| --- | --- |
| `void Dispose()` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyMinionMovement

class; base: `System.Object`.

```csharp
BunnyMinionMovement(float followSpeed, float attackSpeed, float catchUpSpeed, float followInertia, float attackInertia, float followOffsetX, float followOffsetY, float catchUpDistance, float idleDistance, float idleDamping, float catchUpInertia, float teleportDistance, float formationSpacing, float attackStopDistance, float ownerLeashDistance, float rotationFactor, float separation)
BunnyMinionMovement Default { get; }
float AttackInertia { get; }
float AttackSpeed { get; }
float AttackStopDistance { get; }
float CatchUpDistance { get; }
float CatchUpInertia { get; }
float CatchUpSpeed { get; }
float FollowInertia { get; }
float FollowOffsetX { get; }
float FollowOffsetY { get; }
float FollowSpeed { get; }
float FormationSpacing { get; }
float IdleDamping { get; }
float IdleDistance { get; }
float OwnerLeashDistance { get; }
float RotationFactor { get; }
float Separation { get; }
float TeleportDistance { get; }
```

## BunnyLoader.Source.BunnyMinionSteering

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static bool FollowOrAttack(ref BunnyProjectileAiState state)` | managed_helper |
| `static bool FollowOrAttack(ref BunnyProjectileAiState state, BunnyMinionMovement movement)` | managed_helper |

## BunnyLoader.Source.BunnyNPC

abstract class; base: `System.Object`.

```csharp
int AiStyle { get; set; }
int AnimationType { get; set; }
int Damage { get; set; }
int Defense { get; set; }
int Frames { get; set; }
int Height { get; set; }
int LifeMax { get; set; }
int Width { get; set; }
string DisplayName { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `bool AI(ref BunnyNpcAiState state)` | worker_async_entity |
| `void OnDespawn(ref BunnyNpcAiState state)` | worker_async_entity |
| `void OnHit(ref BunnyNpcAiState state, int damage)` | worker_async_entity |
| `void OnKill(ref BunnyNpcAiState state)` | worker_async_entity |
| `void OnSpawn(ref BunnyNpcAiState state)` | worker_async_entity |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyNameAttribute

class; base: `System.Attribute`.

```csharp
BunnyNameAttribute(string culture, string text)
string Culture { get; }
string Text { get; }
```

## BunnyLoader.Source.BunnyNativeBatch

class; base: `System.Object`.

```csharp
int Count { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyValue ReadAndInvoke(BunnyNativeMember method, BunnyValue[] destination, BunnyNativeObject target, BunnyValue arg0, BunnyValue arg1, BunnyValue arg2, BunnyValue arg3)` | experimental_game_thread_access |
| `void Read(BunnyValue[] destination, BunnyNativeObject target)` | experimental_game_thread_access |

## BunnyLoader.Source.BunnyNativeDefaults

class; base: `System.Object`.

```csharp
BunnyNativeDefaults()
string Definition { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyNativeDefaults SetBool(string field, bool value)` | managed_helper |
| `BunnyNativeDefaults SetFloat(string field, float value)` | managed_helper |
| `BunnyNativeDefaults SetId(string field, string idClass, string name)` | managed_helper |
| `BunnyNativeDefaults SetInt(string field, int value)` | managed_helper |
| `BunnyNativeDefaults SetProjectile(string field, string key)` | managed_helper |

## BunnyLoader.Source.BunnyNativeMember

class; base: `System.Object`.

```csharp
BunnyValueKind Kind { get; }
bool CanWrite { get; }
bool IsStatic { get; }
string Name { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyValue Invoke(BunnyNativeObject target, BunnyValue arg0, BunnyValue arg1, BunnyValue arg2, BunnyValue arg3)` | experimental_game_thread_access |
| `BunnyValue Read(BunnyNativeObject target)` | experimental_game_thread_access |
| `void Write(BunnyValue value, BunnyNativeObject target)` | experimental_game_thread_access |

## BunnyLoader.Source.BunnyNativeObject

struct; base: `System.ValueType`.

```csharp
bool IsCurrent { get; }
```

| Method | Execution |
| --- | --- |
| `static BunnyNativeObject FromHook(IntPtr self)` | experimental_game_thread_access |

## BunnyLoader.Source.BunnyNativeScope

class; base: `System.Object`.

```csharp
bool IsDisposed { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyNativeBatch Batch(BunnyNativeMember[] members)` | experimental_game_thread_access |
| `BunnyNativeType Type(string fullName)` | experimental_game_thread_access |
| `static string GetStatus()` | experimental_game_thread_access |
| `void Dispose()` | experimental_game_thread_access |

## BunnyLoader.Source.BunnyNativeType

class; base: `System.Object`.

```csharp
string Name { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyNativeMember Field(string name, BunnyValueKind kind, bool isStatic, bool writable)` | experimental_game_thread_access |
| `BunnyNativeMember Method(string name, BunnyValueKind result, bool isStatic, string[] parameterTypes)` | experimental_game_thread_access |
| `BunnyNativeMember Property(string name, BunnyValueKind kind, bool isStatic, bool writable)` | experimental_game_thread_access |

## BunnyLoader.Source.BunnyNpcAiState

struct; base: `System.ValueType`.

## BunnyLoader.Source.BunnyOriginalInt32

struct; base: `System.ValueType`.

| Method | Execution |
| --- | --- |
| `int Invoke()` | managed_helper |

## BunnyLoader.Source.BunnyOriginalVoid

struct; base: `System.ValueType`.

| Method | Execution |
| --- | --- |
| `void Invoke()` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyPlayer

abstract class; base: `System.Object`.

```csharp
int DefenseBonus { get; }
int MoveSpeedPercent { get; }
string Accessory { get; set; }
```

| Method | Execution |
| --- | --- |
| `bool FrameEffects(bool accessoryEquipped)` | compile_policy |
| `bool FreeDodge(bool accessoryEquipped, BunnyHurtModifiers modifiers)` | worker_sync_pre_hit |
| `bool PostUpdate()` | compile_policy |
| `bool PreUpdate()` | compile_policy |
| `int UpdateLifeRegen(bool accessoryEquipped)` | compile_policy |
| `int UpdateManaRegen(bool accessoryEquipped)` | compile_policy |
| `int UpdateMovement(bool accessoryEquipped)` | compile_policy |
| `string OnEnterWorld()` | compile_policy |
| `string OnRespawn()` | compile_policy |
| `string UpdateDead()` | compile_policy |
| `void ModifyHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtModifiers modifiers)` | worker_sync_pre_hit |
| `void ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers)` | worker_sync_pre_hit |
| `void ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers)` | worker_sync_pre_hit |
| `void OnHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtInfo info)` | worker_async_post_hit |
| `void OnHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtInfo info)` | worker_async_post_hit |
| `void OnHurt(bool accessoryEquipped, BunnyHurtInfo info)` | worker_async_post_hit |
| `void RegisterHooks(BunnyHookRegistry hooks)` | worker_registration |
| `void ResetEffects()` | compile_policy_and_worker_hurt |
| `void SetDefaults()` | compile_policy |
| `void UpdateEquips(bool accessoryEquipped)` | compile_policy_and_worker_hurt |

## BunnyLoader.Source.BunnyPlayerHurtDispatcher

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static Int64 Modify(int slot, int damage, int direction, int flags, int life, int maxLife, int equippedMask, int npcIndex, int projectileIndex, int npcType, int npcDamage, int npcLife, int projectileType, int projectileDamage, int projectileOwner, float attackerX, float attackerY, float attackerVelocityX, float attackerVelocityY, float attackerAngle, float attackerDistance)` | managed_helper |
| `static int After(int slot, int damage, int sourceDamage, int direction, int flags, int lifeRemaining, int equippedMask, int npcIndex, int projectileIndex, int npcType, int npcDamage, int npcLife, int projectileType, int projectileDamage, int projectileOwner, float attackerX, float attackerY, float attackerVelocityX, float attackerVelocityY, float attackerAngle, float attackerDistance)` | managed_helper |
| `static int Register(string modId, string assemblyPath)` | managed_helper |
| `static string GetHookSnapshot()` | managed_helper |
| `static string GetLastError(int slot)` | managed_helper |

## BunnyLoader.Source.BunnyPrefix

abstract class; base: `System.Object`.

```csharp
PrefixCategory Category { get; set; }
float RollWeight { get; set; }
string DisplayName { get; set; }
```

| Method | Execution |
| --- | --- |
| `void ApplyAccessoryEffects(ref int defenseBonus)` | compile_policy |
| `void ModifyValue(ref float valueMultiplier)` | compile_policy |
| `void SetDefaults()` | compile_policy |
| `void SetStats(ref PrefixStats stats)` | compile_policy |

## BunnyLoader.Source.BunnyProjectile

abstract class; base: `System.Object`.

```csharp
BunnyMinionMovement MinionMovement { get; set; }
BunnyNativeDefaults NativeDefaults { get; }
bool Friendly { get; set; }
bool IgnoreWater { get; set; }
bool Minion { get; set; }
bool MinionContactDamage { get; set; }
bool TileCollide { get; set; }
float MinionSlots { get; set; }
float MinionTargetRange { get; set; }
int AiStyle { get; set; }
int AnimationFrameTicks { get; set; }
int Frames { get; set; }
int Height { get; set; }
int MinionScanTicks { get; set; }
int Penetrate { get; set; }
int TimeLeft { get; set; }
int Width { get; set; }
string DisplayName { get; set; }
string NativeDefaultsDefinition { get; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `bool AI(ref BunnyProjectileAiState state)` | worker_async_entity |
| `int OnHitNPC(int damage)` | compile_sampled_policy |
| `int OnKill(int damage)` | compile_sampled_policy |
| `int OnKillDuration(int damage)` | compile_sampled_policy |
| `void OnDespawn(ref BunnyProjectileAiState state)` | worker_async_entity |
| `void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage)` | worker_async_entity |
| `void OnKill(ref BunnyProjectileAiState state)` | worker_async_entity |
| `void OnSpawn(ref BunnyProjectileAiState state)` | worker_async_entity |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyProjectileAiState

struct; base: `System.ValueType`.

```csharp
bool HasTarget { get; }
```

## BunnyLoader.Source.BunnyRecipe

class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `BunnyRecipe AddBehavior(BunnyRecipeBehavior behavior)` | managed_helper |
| `BunnyRecipe AddCondition(BunnyRecipeCondition condition)` | managed_helper |
| `BunnyRecipe AddCondition(string description, Func<BunnyCraftContext, bool> predicate)` | managed_helper |
| `BunnyRecipe AddConsumeIngredientCallback(Func<BunnyCraftContext, int, int, int> callback)` | managed_helper |
| `BunnyRecipe AddIngredient(int vanillaItemId, int count)` | managed_helper |
| `BunnyRecipe AddIngredient(string modItemName, int count)` | managed_helper |
| `BunnyRecipe AddIngredient<TItem>(int count)` | managed_helper |
| `BunnyRecipe AddNativeRecipeGroup(string groupField, int count)` | managed_helper |
| `BunnyRecipe AddOnCraftCallback(Action<BunnyCraftContext> callback)` | managed_helper |
| `BunnyRecipe AddRecipeGroup(BunnyRecipeGroup group, int count)` | managed_helper |
| `BunnyRecipe AddTile(int vanillaTileId)` | managed_helper |
| `BunnyRecipe AddTile(string modTileName)` | managed_helper |
| `BunnyRecipe AddTile<TTile>()` | managed_helper |
| `BunnyRecipe Clone()` | managed_helper |
| `bool RemoveCondition(BunnyRecipeCondition condition)` | managed_helper |
| `bool RemoveIngredient(int vanillaItemId)` | managed_helper |
| `bool RemoveIngredient(string modItemName)` | managed_helper |
| `bool RemoveIngredient<TItem>()` | managed_helper |
| `bool RemoveNativeRecipeGroup(string groupField)` | managed_helper |
| `bool RemoveRecipeGroup(string localGroupName)` | managed_helper |
| `void Register()` | managed_helper |

## BunnyLoader.Source.BunnyRecipeBehavior

abstract class; base: `System.Object`.

```csharp
string ConditionDescription { get; }
```

| Method | Execution |
| --- | --- |
| `bool IsAvailable(BunnyCraftContext context)` | worker_sync_recipe_snapshot |
| `int ConsumeIngredient(BunnyCraftContext context, int ingredientType, int amount)` | worker_sync_recipe_snapshot |
| `void OnCraft(BunnyCraftContext context)` | worker_sync_recipe_snapshot |

## BunnyLoader.Source.BunnyRecipeCondition

class; base: `System.Object`.

```csharp
BunnyRecipeCondition(string description, Func<BunnyCraftContext, bool> predicate)
BunnyRecipeCondition Day { get; }
BunnyRecipeCondition InDesert { get; }
BunnyRecipeCondition InSnow { get; }
BunnyRecipeCondition NearWater { get; }
BunnyRecipeCondition Night { get; }
string Description { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyRecipeCondition Not(string description)` | managed_predicate_composition |
| `bool Evaluate(BunnyCraftContext context)` | managed_predicate_composition |
| `static BunnyRecipeCondition All(string description, BunnyRecipeCondition[] conditions)` | managed_predicate_composition |
| `static BunnyRecipeCondition Any(string description, BunnyRecipeCondition[] conditions)` | managed_predicate_composition |

## BunnyLoader.Source.BunnyRecipeGroup

class; base: `System.Object`.

```csharp
BunnyRecipeGroup(string name, string displayName)
string DisplayName { get; }
string Name { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyRecipeGroup AddItem(int vanillaItemId)` | managed_helper |
| `BunnyRecipeGroup AddItem(string modItemName)` | managed_helper |
| `BunnyRecipeGroup AddItem<TItem>()` | managed_helper |

## BunnyLoader.Source.BunnyRecipeLoader

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static string Dispatch(string modId, string assemblyPath, string owner, int index, int kind, string snapshot)` | worker_sync_recipe_snapshot |

## BunnyLoader.Source.BunnyRecipeRuntime

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static string Dispatch(string modId, string assemblyPath, string owner, int index, int kind, string snapshot)` | worker_sync_recipe_snapshot |

## BunnyLoader.Source.BunnyRecipeSource

abstract class; base: `System.Object`.

```csharp
IReadOnlyList<string> RecipeDefinitions { get; }
```

| Method | Execution |
| --- | --- |
| `BunnyRecipe CreateRecipe(int stack)` | managed_helper |
| `void AddRecipes()` | compile_policy |

## BunnyLoader.Source.BunnyRecipeStage

enum; base: `System.Enum`.

Values: Availability, Consumption, Craft.

## BunnyLoader.Source.BunnyRectangle

struct; base: `System.ValueType`.

```csharp
BunnyRectangle(int x, int y, int width, int height)
```

| Method | Execution |
| --- | --- |
| `bool Contains(int x, int y)` | managed_helper |

## BunnyLoader.Source.BunnyScreenEffect

abstract class; base: `System.Object`.

```csharp
float Intensity { get; set; }
int Layer { get; set; }
int Order { get; set; }
string DisplayName { get; set; }
string PipelineFile { get; set; }
string ShaderFile { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyShootContext

class; base: `System.Object`.

```csharp
Random Random { get; }
int ItemType { get; }
int PlayerIndex { get; }
int WeaponDamage { get; }
```

| Method | Execution |
| --- | --- |
| `void Spawn(BunnyShot shot)` | worker_shoot_command_batch |

## BunnyLoader.Source.BunnyShootRuntime

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static string Dispatch(string modId, string assemblyPath, string owner, int index, int kind, string snapshot)` | worker_sync_shoot_snapshot |

## BunnyLoader.Source.BunnyShootSource

abstract class; base: `BunnyLoader.Source.BunnyRecipeSource`.

```csharp
int ShootCallbackMask { get; }
```

| Method | Execution |
| --- | --- |
| `bool CanShoot(BunnyShootContext context)` | worker_sync_shoot_snapshot |
| `bool OnShoot(BunnyShootContext context, BunnyShot shot)` | worker_sync_shoot_snapshot |
| `void ModifyShootStats(BunnyShootContext context, ref BunnyShot shot)` | worker_sync_shoot_snapshot |

## BunnyLoader.Source.BunnyShot

struct; base: `System.ValueType`.

## BunnyLoader.Source.BunnySoundLimitBehavior

enum; base: `System.Enum`.

Values: IgnoreNew, ReplaceOldest.

## BunnyLoader.Source.BunnySoundStyle

class; base: `System.Object`.

```csharp
BunnySoundStyle(string path, float volume, float pitch, float pitchVariance, int maxInstances, BunnySoundLimitBehavior limitBehavior)
BunnySoundLimitBehavior LimitBehavior { get; }
float Pitch { get; }
float PitchVariance { get; }
float Volume { get; }
int MaxInstances { get; }
string Definition { get; }
string Path { get; }
```

## BunnyLoader.Source.BunnyTile

abstract class; base: `System.Object`.

```csharp
BunnyTileFlags Flags { get; set; }
BunnyTileFrameToggle FrameToggle { get; set; }
BunnyTileShape Shape { get; set; }
bool AllowBreaking { get; set; }
bool AllowDrops { get; set; }
bool CacheDrawData { get; set; }
float MineResist { get; set; }
int AdjacentTile { get; set; }
int AnimationFrameHeight { get; set; }
int AnimationFrameTicks { get; set; }
int AnimationFrames { get; set; }
int CoordinateHeight { get; set; }
int MapB { get; }
int MapG { get; }
int MapR { get; }
int MinPick { get; set; }
int TileCallbackMask { get; }
int[] AdjacentTiles { get; set; }
string DisplayName { get; set; }
string DustName { get; set; }
string ItemDropDefinitions { get; }
string MapName { get; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `BunnyTileDrop[] GetItemDrops(int x, int y)` | managed_helper |
| `bool CanDrop(int x, int y)` | managed_helper |
| `bool CanKillTile(int x, int y)` | managed_helper |
| `bool CreateDust(int x, int y, ref int type)` | managed_helper |
| `bool GrowSapling(int x, int y, bool underground, int treeHeightAddon, bool ignoreWalls)` | managed_helper |
| `bool HasSmartInteract(int x, int y, BunnyTileSmartInteractSettings settings)` | managed_helper |
| `bool KillSound(int x, int y, bool fail)` | managed_helper |
| `bool PreDraw(int x, int y)` | managed_helper |
| `bool PreDraw(int x, int y, BunnyTileDrawContext draw)` | managed_helper |
| `bool RightClick(int x, int y)` | managed_helper |
| `bool Slope(int x, int y)` | managed_helper |
| `bool TileFrame(int x, int y, ref bool resetFrame, ref bool noBreak)` | managed_helper |
| `int NumDust(bool fail)` | compile_policy |
| `void AddMapEntry(int red, int green, int blue, string name)` | managed_helper |
| `void AnimateIndividualTile(int type, int x, int y, ref int addFrameX, ref int addFrameY)` | managed_helper |
| `void AnimateIndividualTile(int x, int y, ref int addFrameX, ref int addFrameY)` | managed_helper |
| `void AnimateTile(ref int frame, ref int frameCounter)` | managed_helper |
| `void DrawEffects(int x, int y, int tileFrameX, int tileFrameY, BunnyTileDrawContext draw)` | managed_helper |
| `void EmitParticles(int x, int y, int tileFrameX, int tileFrameY, BunnyTileDrawContext draw)` | managed_helper |
| `void HitWire(int x, int y)` | managed_helper |
| `void KillMultiTile(int x, int y, int frameX, int frameY)` | managed_helper |
| `void KillTile(int x, int y, ref bool fail, ref bool effectOnly, ref bool noItem)` | managed_helper |
| `void ModifyLight(int x, int y, ref float r, ref float g, ref float b)` | managed_helper |
| `void MouseOver(int x, int y)` | managed_helper |
| `void MouseOverFar(int x, int y)` | managed_helper |
| `void NearbyEffects(int x, int y, bool closer)` | managed_helper |
| `void NumDust(int x, int y, bool fail, ref int num)` | managed_helper |
| `void PlaceInWorld(int x, int y)` | managed_helper |
| `void PostDraw(int x, int y, BunnyTileDrawContext draw)` | managed_helper |
| `void RandomUpdate(int x, int y)` | managed_helper |
| `void RegisterItemDrop(int itemType, int[] styles)` | managed_helper |
| `void RegisterItemDrop(string itemName, int[] styles)` | managed_helper |
| `void SetDrawPositions(int x, int y, ref int width, ref int top, ref int height, ref int frameX, ref int frameY)` | managed_helper |
| `void SetSpriteEffects(int x, int y, ref int effects)` | managed_helper |
| `void SetStaticDefaults()` | compile_policy |
| `void SpecialDraw(int x, int y, BunnyTileDrawContext draw)` | managed_helper |

## BunnyLoader.Source.BunnyTileDrawContext

class; base: `System.Object`.

```csharp
int AnimationFrame { get; }
int Count { get; }
int ParticleCount { get; }
```

| Method | Execution |
| --- | --- |
| `void DrawFlameTexture(int frameX, int frameY, int width, int height, int offsetX, int offsetY, UInt32 argb, int effects)` | managed_helper |
| `void DrawGlowTexture(int frameX, int frameY, int width, int height, int offsetX, int offsetY, UInt32 argb, int effects)` | managed_helper |
| `void DrawTileTexture(int frameX, int frameY, int width, int height, int offsetX, int offsetY, UInt32 argb, int effects)` | managed_helper |
| `void EmitDust(int dustType, int offsetX, int offsetY, int width, int height, int speedXMilli, int speedYMilli, int alpha, int scaleMilli, UInt32 argb)` | managed_helper |
| `void EmitDustStyled(int dustType, int offsetX, int offsetY, int width, int height, int speedXMilli, int speedYMilli, int alpha, int scaleMilli, UInt32 argb, bool noGravity, int fadeInMilli, int velocityXScaleMilli, int velocityYScaleMilli, int velocityYAddMilli, int positionXAdd, int alphaAdd)` | managed_helper |
| `void RequestSpecialDraw()` | managed_helper |

## BunnyLoader.Source.BunnyTileDrop

struct; base: `System.ValueType`.

```csharp
BunnyTileDrop(int itemType, int stack)
int ItemType { get; }
int Stack { get; }
```

## BunnyLoader.Source.BunnyTileFlags

enum; base: `System.Enum`.

Values: None, Table, SolidTop, NoAttach, LavaDeath, FrameImportant, DisableSmartCursor, HasOutlines, IgnoredByNpcStepUp, CountsAsTable, Solid, BlockLight, MergeDirt, Lighted, WaterDeath, ChecksForMerge, Cut, Campfire.

## BunnyLoader.Source.BunnyTileFrameToggle

enum; base: `System.Enum`.

Values: None, CampfireVertical, LampHorizontal.

## BunnyLoader.Source.BunnyTileRuntime

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static string Dispatch(string modId, string assemblyPath, string owner, int index, int kind, string snapshot)` | managed_helper |

## BunnyLoader.Source.BunnyTileShape

enum; base: `System.Enum`.

Values: Single, Style2x1, Style3x2, Style1x3.

## BunnyLoader.Source.BunnyTileSmartInteractSettings

struct; base: `System.ValueType`.

```csharp
BunnyTileSmartInteractSettings(int lx, int ly, int hx, int hy)
int HX { get; }
int HY { get; }
int LX { get; }
int LY { get; }
```

## BunnyLoader.Source.BunnyUniversalHookDispatcher

abstract class; base: `System.Object`.

```csharp
string LastError { get; }
```

| Method | Execution |
| --- | --- |
| `static int Register(string id, string path)` | experimental_worker_sync_void0 |
| `static string GetStatusSnapshot(string id)` | experimental_worker_sync_void0 |
| `static void UnloadAll()` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyValue

struct; base: `System.ValueType`.

| Method | Execution |
| --- | --- |
| `BunnyColor AsColor()` | managed_helper |
| `BunnyRectangle AsRectangle()` | managed_helper |
| `BunnyVector2 AsVector2()` | managed_helper |
| `Double AsDouble()` | managed_helper |
| `Int64 AsInt64()` | managed_helper |
| `UInt32 AsUInt32()` | managed_helper |
| `UInt64 AsUInt64()` | managed_helper |
| `bool AsBoolean()` | managed_helper |
| `float AsSingle()` | managed_helper |
| `int AsInt32()` | managed_helper |
| `static BunnyValue FromBoolean(bool value)` | managed_helper |
| `static BunnyValue FromColor(BunnyColor value)` | managed_helper |
| `static BunnyValue FromDouble(Double value)` | managed_helper |
| `static BunnyValue FromInt32(int value)` | managed_helper |
| `static BunnyValue FromInt64(Int64 value)` | managed_helper |
| `static BunnyValue FromRectangle(BunnyRectangle value)` | managed_helper |
| `static BunnyValue FromSingle(float value)` | managed_helper |
| `static BunnyValue FromUInt32(UInt32 value)` | managed_helper |
| `static BunnyValue FromUInt64(UInt64 value)` | managed_helper |
| `static BunnyValue FromVector2(BunnyVector2 value)` | managed_helper |

## BunnyLoader.Source.BunnyValueKind

enum; base: `System.Enum`.

Values: Void, Boolean, Int32, UInt32, Int64, Single, Double, Vector2, Rectangle, Color, UInt64.

## BunnyLoader.Source.BunnyVanillaItems

abstract class; base: `System.Object`.

## BunnyLoader.Source.BunnyVanillaTiles

abstract class; base: `System.Object`.

## BunnyLoader.Source.BunnyVector2

struct; base: `System.ValueType`.

```csharp
BunnyVector2(float x, float y)
float LengthSquared { get; }
```

## BunnyLoader.Source.BunnyVisual

abstract class; base: `System.Object`.

```csharp
int PendingCount { get; }
```

| Method | Execution |
| --- | --- |
| `static bool ClearPlayerOverlay(int playerIndex, int effectId)` | managed_command_queue |
| `static bool SetEquipmentSprite(int playerIndex, int effectId, string texture)` | managed_command_queue |
| `static bool SetPlayerOverlay(int playerIndex, int effectId, string texture, float scale, float opacity, UInt32 color, int durationTicks)` | managed_command_queue |
| `static bool SpawnParticle(int playerIndex, int effectId, string texture, float x, float y, float scale, UInt32 color, int durationTicks)` | managed_command_queue |
| `static bool TryTake(out BunnyVisualCommand command)` | managed_command_queue |
| `static string TakeWire()` | managed_command_queue |
| `static void ClearPending()` | managed_command_queue |

## BunnyLoader.Source.BunnyVisualCommand

struct; base: `System.ValueType`.

```csharp
BunnyVisualCommand(BunnyVisualKind kind, int playerIndex, int effectId, string texture, float x, float y, float scale, float opacity, UInt32 color, int durationTicks)
```

## BunnyLoader.Source.BunnyVisualKind

enum; base: `System.Enum`.

Values: Particle, PlayerOverlay, ClearOverlay, EquipmentSprite.

## BunnyLoader.Source.BunnyVoidHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyVoidHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalVoid original, IntPtr self, AsyncCallback callback, Object object)` | experimental_worker_sync_void0 |
| `void EndInvoke(IAsyncResult result)` | experimental_worker_sync_void0 |
| `void Invoke(BunnyOriginalVoid original, IntPtr self)` | experimental_worker_sync_void0 |

## BunnyLoader.Source.BunnyVoidInt32Hook

class; base: `System.MulticastDelegate`.

```csharp
BunnyVoidInt32Hook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalVoid original, IntPtr self, int first, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(IAsyncResult result)` | managed_helper |
| `void Invoke(BunnyOriginalVoid original, IntPtr self, int first)` | managed_helper |

## BunnyLoader.Source.BunnyVoidInt32PairHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyVoidInt32PairHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalVoid original, IntPtr self, int first, int second, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(IAsyncResult result)` | managed_helper |
| `void Invoke(BunnyOriginalVoid original, IntPtr self, int first, int second)` | managed_helper |

## BunnyLoader.Source.BunnyVoidInt32TripleHook

class; base: `System.MulticastDelegate`.

```csharp
BunnyVoidInt32TripleHook(Object object, IntPtr method)
```

| Method | Execution |
| --- | --- |
| `IAsyncResult BeginInvoke(BunnyOriginalVoid original, IntPtr self, int first, int second, int third, AsyncCallback callback, Object object)` | managed_helper |
| `void EndInvoke(IAsyncResult result)` | managed_helper |
| `void Invoke(BunnyOriginalVoid original, IntPtr self, int first, int second, int third)` | managed_helper |

## BunnyLoader.Source.BunnyWall

abstract class; base: `System.Object`.

```csharp
BunnyWorldLight DayLight { get; set; }
BunnyWorldLight NightLight { get; set; }
bool Housing { get; set; }
int AnimationFrameTicks { get; set; }
int AnimationFrames { get; set; }
int DustCount { get; set; }
int FailedDustCount { get; set; }
string BlendWallName { get; set; }
string DisplayName { get; set; }
string DustName { get; set; }
string NativeDefinition { get; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void AddMapEntry(int r, int g, int b, string name)` | managed_helper |
| `void RandomizeFrame(int from, float chance, int[] alternatives)` | managed_helper |
| `void SetFrameDust(int frameNumber, string dustName)` | managed_helper |
| `void SetStaticDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyWeapon

abstract class; base: `BunnyLoader.Source.BunnyShootSource`.

```csharp
BunnyHoldout Holdout { get; set; }
BunnyNativeDefaults NativeDefaults { get; }
BunnySoundStyle CustomUseSound { get; set; }
bool Summon { get; set; }
float ShootSpeed { get; set; }
int Damage { get; set; }
int MinionBuffTime { get; set; }
int UseTime { get; set; }
string DisplayName { get; set; }
string MinionBuff { get; set; }
string NativeDefaultsDefinition { get; }
string Shoot { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyWorldLight

struct; base: `System.ValueType`.

```csharp
BunnyWorldLight(float r, float g, float b)
float B { get; }
float G { get; }
float R { get; }
```

## BunnyLoader.Source.PrefixCategory

enum; base: `System.Enum`.

Values: Melee, Ranged, Magic, Summon, AnyWeapon, Accessory, Custom.

## BunnyLoader.Source.PrefixStats

struct; base: `System.ValueType`.

```csharp
PrefixStats Identity { get; }
float DamageMultiplier { get; set; }
float KnockbackMultiplier { get; set; }
float ManaMultiplier { get; set; }
float ScaleMultiplier { get; set; }
float ShootSpeedMultiplier { get; set; }
float UseTimeMultiplier { get; set; }
int ArmorPenetrationBonus { get; set; }
int CriticalBonus { get; set; }
int TagDamageBonus { get; set; }
```

