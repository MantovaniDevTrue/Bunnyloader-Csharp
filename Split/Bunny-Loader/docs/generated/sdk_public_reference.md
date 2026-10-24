# Public C# SDK reference

Generated from the compiled BunnySourceSdk. Update with `dotnet run --project tools/sdk_reference -c Release -- --write`.

Execution terms are defined in [the SDK guide](../../../../docs/SDK-CSharp.md). Public methods do not imply Android validation, arbitrary per-frame AI, or multiplayer support. Only members declared on each type are listed; inherited members follow its base type.

## BunnyLoader.Source.AutoloadAttribute

class; base: `System.Attribute`.

```csharp
AutoloadAttribute(bool value)
ModSide Side { get; set; }
bool NeedsAutoloading { get; }
```

| Method | Execution |
| --- | --- |
| `static AutoloadAttribute GetValue(Type type)` | managed_helper |

## BunnyLoader.Source.BunnyAccessory

abstract class; base: `System.Object`.

```csharp
BunnyAccessoryVisualMode VisualMode { get; set; }
UInt32 VisualColor { get; set; }
float VisualOpacity { get; set; }
float VisualScale { get; set; }
int DefenseBonus { get; set; }
string DisplayName { get; set; }
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
int DefenseBonus { get; set; }
string DisplayName { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyBuffPotion

abstract class; base: `System.Object`.

```csharp
int Duration { get; set; }
string Buff { get; set; }
string DisplayName { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

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

## BunnyLoader.Source.BunnyContentHostDispatcher

abstract class; base: `System.Object`.

```csharp
string LastError { get; }
```

| Method | Execution |
| --- | --- |
| `static int Dispatch(int operation, string modId, string assemblyPath)` | mono_owner_content_lifecycle |

## BunnyLoader.Source.BunnyContentKind

enum; base: `System.Enum`.

Values: Item, Tile, Weapon, Projectile, Buff, BuffPotion, Accessory, Player, NPC, Prefix.

## BunnyLoader.Source.BunnyContentLoader

abstract class; base: `System.Object`.

```csharp
ModSide ExecutionSide { get; set; }
```

| Method | Execution |
| --- | --- |
| `static BunnyContentMod Load(string modName, Assembly assembly)` | managed_content_lifecycle |
| `static BunnyContentMod Load(string modName, Assembly assembly, Action<BunnyContentMod> configure)` | managed_content_lifecycle |
| `static BunnyContentMod Load(string modName, string assemblyPath)` | managed_content_lifecycle |
| `static bool IsLoaded(string modName)` | managed_content_lifecycle |
| `static bool Unload(string modName)` | managed_content_lifecycle |
| `static void UnloadAll()` | managed_content_lifecycle |

## BunnyLoader.Source.BunnyContentMod

class; base: `System.Object`.

```csharp
Assembly Assembly { get; }
bool IsLoaded { get; }
string Name { get; }
```

| Method | Execution |
| --- | --- |
| `IReadOnlyList<T> GetContent()` | managed_content_lifecycle |
| `bool AddContent(ModType instance)` | managed_content_lifecycle |

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

## BunnyLoader.Source.BunnyHookHandle

struct; base: `System.ValueType`.

```csharp
BunnyHookPhase Phase { get; }
bool IsValid { get; }
int Id { get; }
```

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

## BunnyLoader.Source.BunnyItem

abstract class; base: `System.Object`.

```csharp
BunnyItemBehavior Behavior { get; }
Dictionary<string, string> TooltipTranslations { get; }
IReadOnlyList<string> RecipeDefinitions { get; }
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
string PlacementDefinition { get; }
string Texture { get; set; }
string Tooltip { get; set; }
string UseSound { get; set; }
```

| Method | Execution |
| --- | --- |
| `BunnyRecipe CreateRecipe(int stack)` | managed_helper |
| `void AddRecipes()` | compile_policy |
| `void DefaultToPlaceableTile(int vanillaTileId, int style)` | managed_helper |
| `void DefaultToPlaceableTile(string modTileName, int style)` | managed_helper |
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

## BunnyLoader.Source.BunnyNpcAiState

struct; base: `System.ValueType`.

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
bool Friendly { get; set; }
int AiStyle { get; set; }
int Height { get; set; }
int Penetrate { get; set; }
int TimeLeft { get; set; }
int Width { get; set; }
string DisplayName { get; set; }
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

## BunnyLoader.Source.BunnyRecipe

class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `BunnyRecipe AddIngredient(int vanillaItemId, int count)` | managed_helper |
| `BunnyRecipe AddIngredient(string modItemName, int count)` | managed_helper |
| `BunnyRecipe AddTile(int vanillaTileId)` | managed_helper |
| `BunnyRecipe AddTile(string modTileName)` | managed_helper |
| `void Register()` | managed_helper |

## BunnyLoader.Source.BunnyTile

abstract class; base: `System.Object`.

```csharp
BunnyTileFlags Flags { get; set; }
BunnyTileShape Shape { get; set; }
int AdjacentTile { get; set; }
int CoordinateHeight { get; set; }
int MapB { get; }
int MapG { get; }
int MapR { get; }
string DisplayName { get; set; }
string DustName { get; set; }
string MapName { get; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `int NumDust(bool fail)` | compile_policy |
| `void AddMapEntry(int red, int green, int blue, string name)` | managed_helper |
| `void SetStaticDefaults()` | compile_policy |

## BunnyLoader.Source.BunnyTileFlags

enum; base: `System.Enum`.

Values: None, Table, SolidTop, NoAttach, LavaDeath, FrameImportant, DisableSmartCursor, HasOutlines, IgnoredByNpcStepUp, CountsAsTable.

## BunnyLoader.Source.BunnyTileShape

enum; base: `System.Enum`.

Values: Single, Style2x1.

## BunnyLoader.Source.BunnyVanillaItems

abstract class; base: `System.Object`.

## BunnyLoader.Source.BunnyVanillaTiles

abstract class; base: `System.Object`.

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

## BunnyLoader.Source.BunnyWeapon

abstract class; base: `System.Object`.

```csharp
float ShootSpeed { get; set; }
int Damage { get; set; }
int UseTime { get; set; }
string DisplayName { get; set; }
string Shoot { get; set; }
string Texture { get; set; }
```

| Method | Execution |
| --- | --- |
| `void SetDefaults()` | compile_policy |

## BunnyLoader.Source.ContentInstance`1

abstract class; base: `System.Object`.

```csharp
IReadOnlyList<T> Instances { get; }
T Instance { get; }
```

## BunnyLoader.Source.ILoader

abstract class.

| Method | Execution |
| --- | --- |
| `void ResizeArrays()` | managed_content_lifecycle |
| `void Unload()` | managed_content_lifecycle |

## BunnyLoader.Source.IModType

abstract class.

```csharp
BunnyContentMod Mod { get; }
string FullName { get; }
string Name { get; }
```

## BunnyLoader.Source.LegacyNameAttribute

class; base: `System.Attribute`.

```csharp
LegacyNameAttribute(string[] names)
```

| Method | Execution |
| --- | --- |
| `static IEnumerable<string> GetLegacyNamesOfType(Type type)` | managed_helper |

## BunnyLoader.Source.Loader

abstract class; base: `System.Object`.

```csharp
int TotalCount { get; }
int VanillaCount { get; set; }
```

| Method | Execution |
| --- | --- |
| `void Initialize(int vanillaCount)` | managed_content_lifecycle |
| `void ResizeArrays()` | managed_content_lifecycle |
| `void Unload()` | managed_content_lifecycle |

## BunnyLoader.Source.LoaderManager

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static T Get()` | managed_content_lifecycle |
| `static void AutoLoad()` | managed_content_lifecycle |
| `static void AutoLoad(Assembly assembly)` | managed_content_lifecycle |
| `static void Clear()` | managed_content_lifecycle |
| `static void ReleaseAssembly(Assembly assembly)` | managed_content_lifecycle |
| `static void ResizeArrays()` | managed_content_lifecycle |
| `static void Unload()` | managed_content_lifecycle |

## BunnyLoader.Source.Loader`1

abstract class; base: `BunnyLoader.Source.Loader`.

| Method | Execution |
| --- | --- |
| `T Get(int id)` | managed_content_lifecycle |
| `int Register(T obj)` | managed_content_lifecycle |
| `void Unload()` | managed_content_lifecycle |

## BunnyLoader.Source.ModContent

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static IReadOnlyList<T> GetContent()` | managed_content_registry |
| `static T Find(string fullName)` | managed_content_registry |
| `static T Find(string modName, string contentName)` | managed_content_registry |
| `static T GetInstance()` | managed_content_registry |
| `static bool TryFind(string fullName, out T value)` | managed_content_registry |
| `static bool TryFind(string modName, string contentName, out T value)` | managed_content_registry |

## BunnyLoader.Source.ModSide

enum; base: `System.Enum`.

Values: Both, Client, Server, NoSync.

## BunnyLoader.Source.ModType

abstract class; base: `System.Object`.

```csharp
BunnyContentMod Mod { get; }
string FullName { get; }
string Name { get; }
```

| Method | Execution |
| --- | --- |
| `bool IsLoadingEnabled(BunnyContentMod mod)` | managed_content_lifecycle |
| `string PrettyPrintName()` | managed_content_lifecycle |
| `void Load()` | managed_content_lifecycle |
| `void SetStaticDefaults()` | managed_content_lifecycle |
| `void SetupContent()` | managed_content_lifecycle |
| `void Unload()` | managed_content_lifecycle |

## BunnyLoader.Source.ModTypeLookup`1

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static T Get(string fullName)` | managed_content_registry |
| `static T Get(string modName, string contentName)` | managed_content_registry |
| `static bool TryGetValue(string fullName, out T value)` | managed_content_registry |
| `static bool TryGetValue(string modName, string contentName, out T value)` | managed_content_registry |
| `static void Register(T instance)` | managed_content_registry |
| `static void RegisterLegacyNames(T instance, string[] legacyNames)` | managed_content_registry |

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

## Terraria.Utilities.UnifiedRandom

class; base: `System.Random`.

```csharp
UnifiedRandom()
UnifiedRandom(int seed)
```

| Method | Execution |
| --- | --- |
| `Double NextDouble()` | managed_helper |
| `Int64 NextInt64()` | managed_helper |
| `Int64 NextInt64(Int64 maxValue)` | managed_helper |
| `Int64 NextInt64(Int64 minValue, Int64 maxValue)` | managed_helper |
| `float NextSingle()` | managed_helper |
| `int Next()` | managed_helper |
| `int Next(int maxValue)` | managed_helper |
| `int Next(int minValue, int maxValue)` | managed_helper |
| `void NextBytes(Byte[] buffer)` | managed_helper |
| `void NextBytes(Span<Byte> buffer)` | managed_helper |
| `void SetSeed(int seed)` | managed_helper |

## Terraria.Utilities.WeightedRandom`1

class; base: `System.Object`.

```csharp
WeightedRandom`1()
WeightedRandom`1(Tuple<T, Double>[] theElements)
WeightedRandom`1(UnifiedRandom random)
WeightedRandom`1(UnifiedRandom random, Tuple<T, Double>[] theElements)
WeightedRandom`1(int seed)
WeightedRandom`1(int seed, Tuple<T, Double>[] theElements)
```

| Method | Execution |
| --- | --- |
| `T Get()` | managed_helper |
| `void Add(T element, Double weight)` | managed_helper |
| `void CalculateTotalWeight()` | managed_helper |
| `void Clear()` | managed_helper |

## Terraria.Utils

abstract class; base: `System.Object`.

| Method | Execution |
| --- | --- |
| `static T Next(UnifiedRandom random, IList<T> list)` | managed_helper |
| `static T Next(UnifiedRandom random, T[] array)` | managed_helper |
| `static T NextFromCollection(UnifiedRandom random, List<T> values)` | managed_helper |
| `static T NextFromList(UnifiedRandom random, T[] values)` | managed_helper |
| `static Vector2 NextVector2Circular(UnifiedRandom random, float halfWidth, float halfHeight)` | managed_helper |
| `static Vector2 NextVector2CircularEdge(UnifiedRandom random, float halfWidth, float halfHeight)` | managed_helper |
| `static Vector2 NextVector2Square(UnifiedRandom random, float min, float max)` | managed_helper |
| `static Vector2 NextVector2Unit(UnifiedRandom random, float startRotation, float rotationRange)` | managed_helper |
| `static bool NextBool(UnifiedRandom random)` | managed_helper |
| `static bool NextBool(UnifiedRandom random, int antecedent, int consequent)` | managed_helper |
| `static bool NextBool(UnifiedRandom random, int consequent)` | managed_helper |
| `static float NextFloat(UnifiedRandom random)` | managed_helper |
| `static float NextFloat(UnifiedRandom random, float maxValue)` | managed_helper |
| `static float NextFloat(UnifiedRandom random, float minValue, float maxValue)` | managed_helper |
| `static float NextFloatDirection(UnifiedRandom random)` | managed_helper |

