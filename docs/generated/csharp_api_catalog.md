# API C# Bunny Loader — inventario gerado

Fontes: ExampleModBunny JS, SDK C# e ExampleModCSharp. A presenca de um callback JS nao comprova suporte C#.

## Totais

- JS: 178 arquivos, 30 bases, 204 pares base/callback.
- C#: 7 classes do SDK, 12 metodos, 36 propriedades, 15 classes de exemplo.
- Recursos classificados: 19.

## Cobertura

| Recurso | Estado | SDK | Exemplo |
| --- | --- | --- | --- |
| legacy_live_hits | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/BunnyLiveEvents.cs |
| managed_npc_debuff | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs |
| managed_npc_impulse | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs |
| npc_registration | device_verified | BunnyNPC | samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs |
| on_hit_context_v2 | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs |
| on_hit_context_v3 | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs |
| on_hit_context_v4 | device_verified | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs |
| projectile_motion | device_verified | — | samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleWaveBoltMotion.cs |
| projectile_registration | device_verified | BunnyProjectile | samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleWaveBolt.cs |
| weapon_registration | device_verified | BunnyWeapon | samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/ExampleWaveWand.cs |
| accessory | ci_verified | BunnyAccessory | samples/ExampleModCSharp/Playable/content/Content/Items/Accessories/ExampleFocusAccessory.cs |
| buff_definition | ci_verified | BunnyBuff | samples/ExampleModCSharp/Playable/content/Content/Buffs/ExampleDefenseBuff.cs |
| buff_potion | ci_verified | BunnyBuffPotion | samples/ExampleModCSharp/Playable/content/Content/Items/Consumables/ExampleBuffPotion.cs |
| player_equips | ci_verified | BunnyPlayer | samples/ExampleModCSharp/Playable/content/Common/Players/ExampleFocusPlayer.cs |
| npc_ai_prototype | experimental | — | samples/ExampleModCSharp/Playable/content/Common/Live/ExampleLiveNpcAI.cs |
| managed_dynamic_projectile_spawn | not_implemented | — | — |
| multiplayer_effect_sync | not_implemented | — | — |
| npc_bestiary | not_implemented | — | — |
| npc_loot_rules | not_implemented | — | — |

## Callbacks JavaScript por classe base

A lista de JS e apenas um roteiro de ports. Ela nao implica compatibilidade binaria ou gerenciada.

| Base JS | Arquivos | Metodos encontrados |
| --- | ---: | --- |
| DamageClass | 1 | GetEffectInheritance, GetModifierInheritance, SetDefaultStats, ShowStatTooltipLine |
| ExampleGemsparkBlockBase | 1 | HitWire, ModifyLight, SetStaticDefaults, TileFrame |
| ExamplePrefix | 1 |  |
| ExampleTrapBase | 1 | AddRecipes, SetDefaults |
| GlobalNPC | 1 | ModifyNPCLoot |
| Mod | 1 | Load, PostSetupContent |
| ModAchievement | 2 | OnNPCKilled, SetStaticDefaults |
| ModBiome | 2 | IsBiomeActive, SetStaticDefaults |
| ModBuff | 8 | ModifyDescription, SetStaticDefaults, UpdatePlayer |
| ModCloud | 1 | OnSpawn, SpawnChance |
| ModCommand | 2 | Action |
| ModConfig | 1 | OnChanged, RandomizeGlow |
| ModEmoteBubble | 2 | GetFrame, GetFrameInEmoteMenu, SetStaticDefaults, UpdateFrame |
| ModHair | 1 | GetUnlockConditions |
| ModItem | 79 | AddArmorSets, AddRecipes, AnglerQuestChat, CanAccessoryBeEquippedWith, CanRightClick, CanUseItem, GetAlpha, HoldItem, HoldoutOffset, IsAnglerQuestAvailable, IsVanitySet, Load, MagicPrefix, MeleePrefix, ModifyFishingLine, ModifyItemLoot, ModifyShootStats, ModifyTooltipLines, ModifyTooltips, PostUpdate, PreDrawTooltipLine, RangedPrefix, SetDefaults, SetMatch, SetStaticDefaults, Shoot, UpdateAccessory, UpdateArmorSet, UpdateEquip, UpdateVanity, UpdateVanitySet, UseItem, UseStyle, VerticalWingSpeeds |
| ModMenu | 1 | OnSelected, PreDrawLogo |
| ModMount | 2 | SetMount, SetStaticDefaults, UpdateEffects |
| ModNPC | 3 | AI, AddShops, ApplyBuffImmunity, CanTownNPCSpawn, CheckConditions, FindFrame, GetChat, HitEffect, ModifyNPCLoot, OnChatButtonClicked, OnKill, PreAI, SetBestiary, SetChatButtons, SetDefaults, SetNPCNameList, SetStaticDefaults, SpawnChance, TownNPCAttackProj, TownNPCAttackProjSpeed, TownNPCAttackStrength |
| ModPlayer | 4 | CanUseDash, CatchFish, FrameEffects, OnEnterWorld, OnStartDash, ResetDash, ResetEffects, UpdateDash, UpdateEquips, UpdateMovement |
| ModPrefix | 2 | Apply, ApplyAccessoryEffects, CanRoll, GetTooltipLines, ModifyValue, RollChance, SetStats |
| ModProjectile | 18 | AI, Aim, CanCutTiles, CheckActive, DamageNPCs, DrawLikeExcalibur, DrawSparkle, FindTarget, GeneralBehavior, GetAlpha, GrappleCanLatchOnTo, LaserColor, LaserEnd, MinionContactDamage, Movement, OnKill, OnSpawn, OnTileCollide, PreAI, PreDraw, PreDrawExtras, SearchForTargets, SetDefaults, SetStaticDefaults, ShootBeam, ShouldConsumeMana, TileCollideStyle, UpdateDash, UpdateExtraMovement, UpdateFading, UseGrapple, Visuals |
| ModRarity | 2 | GetPrefixedRarity |
| ModSurfaceBackgroundStyle | 1 | ChooseCloseTexture, ChooseFarTexture, ChooseMiddleTexture, ModifyFarFades |
| ModSystem | 3 | AddRecipeGroups, AddRecipes, ClearWorld, LoadWorldData, NetReceive, NetSend, SaveWorldData, TileCountsAvailable |
| ModTile | 32 | AnimateIndividualTile, AnimateTile, CanDrop, CreateDust, DefaultContainerName, DrawEffects, EmitParticles, GetItemDrops, GetMapOption, HitWire, IsLockedChest, KillMultiTile, KillSound, LockChest, ModifyLight, ModifySittingTargetInfo, ModifySleepingTargetInfo, MouseOver, MouseOverFar, NearbyEffects, NumDust, PlaceInWorld, PostDraw, PostSetDefaults, PreDraw, RandomUpdate, RightClick, SetDrawPositions, SetSpriteEffects, SetStaticDefaults, Slope, SpecialDraw, TileFrame, ToggleTile, UnlockChest |
| ModUndergroundBackgroundStyle | 1 | FillTextureArray |
| ModWall | 2 | AnimateWall, CreateDust, ModifyLight, NumDust, SetStaticDefaults, WallFrame |
| ModWaterStyle | 1 | BiomeHairColor, ChooseWaterfallStyle, GetDropletGore, GetRainTexture, GetSplashDust, LightColorMultiplier |
| ModWaterfallStyle | 1 | AddLight |
| PlayerDrawLayer | 1 | Draw, GetDefaultPosition, GetDefaultVisibility |

## API declarada no SDK C#

### BunnyProjectile

Metodos: SetDefaults(), OnHitNPC(int damage), OnKill(int damage), OnKillDuration(int damage)

Propriedades: DisplayName, Texture, Width, Height, AiStyle, Friendly, Penetrate, TimeLeft

### BunnyWeapon

Metodos: SetDefaults()

Propriedades: DisplayName, Texture, Damage, UseTime, ShootSpeed, Shoot

### BunnyBuff

Metodos: SetDefaults()

Propriedades: DisplayName, Texture, DefenseBonus

### BunnyBuffPotion

Metodos: SetDefaults()

Propriedades: DisplayName, Texture, Buff, Duration

### BunnyAccessory

Metodos: SetDefaults()

Propriedades: DisplayName, Texture, DefenseBonus

### BunnyPlayer

Metodos: SetDefaults(), ResetEffects(), UpdateEquips(bool accessoryEquipped)

Propriedades: Accessory, DefenseBonus

### BunnyNPC

Metodos: SetDefaults()

Propriedades: DisplayName, Texture, Width, Height, Damage, Defense, LifeMax, AiStyle, AnimationType, Frames

## Politica dos estados

- device_verified: somente confirmacao explicita em dispositivo; o script nunca atribui isso sozinho.
- ci_verified: fonte e contrato comprovados, nao desempenho Android.
- experimental: superficie encontrada, comportamento parcial ou ainda nao comprovado.
- not_implemented: ausencia do contrato C#, sem exemplo executavel.

Gerar: python3 tools/csharp_api_discovery.py --write
Conferir no CI: python3 tools/csharp_api_discovery.py --check
