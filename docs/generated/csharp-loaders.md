# Auditoria dos loaders C#

88 módulos originais contabilizados, inclusive helpers. Nenhum módulo é certificado como completo por esta auditoria.

As APIs indicadas já possuem caminhos reais, mas ainda precisam de comparação hook a hook. O JSON registra hash e caminho de todos os arquivos. Ausência de mapeamento significa auditoria pendente, não prova de que o Terraria impede o recurso.

| Módulo original | API C# relacionada | Estado |
| --- | --- | --- |
| `AchievementLoader` | — | Auditoria pendente |
| `AnglerQuestLoader` | — | Auditoria pendente |
| `ArmorSetLoader` | — | Auditoria pendente |
| `BackgroundTextureLoader` | — | Auditoria pendente |
| `BannerLoader` | — | Auditoria pendente |
| `BestiaryLoader` | — | Auditoria pendente |
| `BiomeBestiaryLoader` | — | Auditoria pendente |
| `BiomeLoader` | — | Auditoria pendente |
| `BiomeShoppingLoader` | — | Auditoria pendente |
| `BossBarLoader` | — | Auditoria pendente |
| `BuffLoader` | BunnyBuff, BunnyBuffPotion | Parcial |
| `ChestNetworkHooks` | — | Auditoria pendente |
| `CloudLoader` | — | Auditoria pendente |
| `CombatLoader` | — | Auditoria pendente |
| `CommandLoader` | — | Auditoria pendente |
| `ConfigLoader` | — | Auditoria pendente |
| `DamageClassLoader` | — | Auditoria pendente |
| `EmoteBubbleLoader` | — | Auditoria pendente |
| `EquipLoader` | BunnyItemBehavior, BunnyArmorEffects, BunnyArmorSet | Parcial |
| `GlobalItemLoader` | — | Auditoria pendente |
| `GlobalLootLoader` | — | Auditoria pendente |
| `GlobalNPCLoader` | — | Auditoria pendente |
| `GlobalProjectileLoader` | — | Auditoria pendente |
| `GlobalTileLoader` | — | Auditoria pendente |
| `GoreLoader` | — | Auditoria pendente |
| `HairLoader` | — | Auditoria pendente |
| `HostSettingsMenu` | — | Auditoria pendente |
| `ItemCombatHooks` | — | Auditoria pendente |
| `ItemDataLoader` | — | Auditoria pendente |
| `ItemDrawLoader` | — | Auditoria pendente |
| `ItemHealingHooks` | — | Auditoria pendente |
| `ItemLoader` | BunnyItem, BunnyWeapon, BunnyAccessory, BunnyShootSource, BunnyShootContext, BunnyShot | Parcial |
| `ItemNetworkHooks` | — | Auditoria pendente |
| `ItemPickupLoader` | — | Auditoria pendente |
| `ItemUseHooks` | — | Auditoria pendente |
| `KeybindLoader` | — | Auditoria pendente |
| `LocalizationLoader` | — | Auditoria pendente |
| `MapBackgroundLoader` | — | Auditoria pendente |
| `MenuLoader` | — | Auditoria pendente |
| `MenuThemeSetting` | — | Auditoria pendente |
| `ModConfigMenu` | — | Auditoria pendente |
| `ModMusic` | — | Auditoria pendente |
| `MountLoader` | — | Auditoria pendente |
| `NPCLoader` | BunnyNPC | Parcial |
| `PlayerDrawHooks` | BunnyVisual | Parcial |
| `PlayerItemHooks` | — | Auditoria pendente |
| `PlayerJumpHooks` | — | Auditoria pendente |
| `PlayerLoader` | BunnyPlayer | Parcial |
| `PlayerNetworkHooks` | — | Auditoria pendente |
| `PlayerUpdateHooks` | BunnyPlayer | Parcial |
| `PlayerWorldHooks` | — | Auditoria pendente |
| `PrefixLoader` | BunnyPrefix | Parcial |
| `ProjectileLoader` | BunnyProjectile, BunnyMinionSteering, BunnyMinionMovement | Parcial |
| `PvpItemNetwork` | — | Auditoria pendente |
| `PylonLoader` | — | Auditoria pendente |
| `RarityLoader` | — | Auditoria pendente |
| `RecipeLoader` | BunnyRecipeLoader, BunnyRecipeSource, BunnyRecipe, BunnyRecipeGroup, BunnyRecipeBehavior, BunnyRecipeCondition | Parcial |
| `SceneEffectLoader` | — | Auditoria pendente |
| `ScreenEffectsSetting` | — | Auditoria pendente |
| `ServerSyncLoader` | — | Auditoria pendente |
| `ServerSyncMenu` | — | Auditoria pendente |
| `SoundLoader` | — | Auditoria pendente |
| `SpawnLoader` | — | Auditoria pendente |
| `SurfaceBackgroundLoader` | — | Auditoria pendente |
| `SystemDrawHooks` | — | Auditoria pendente |
| `SystemLoader` | — | Auditoria pendente |
| `SystemNetworkHooks` | — | Auditoria pendente |
| `SystemUpdateHooks` | — | Auditoria pendente |
| `SystemWorldHooks` | — | Auditoria pendente |
| `TileDrawLoader` | — | Auditoria pendente |
| `TileEntityItemHooks` | — | Auditoria pendente |
| `TileEntityLoader` | — | Auditoria pendente |
| `TileLoader` | BunnyTile | Parcial |
| `TileUseLoader` | — | Auditoria pendente |
| `TileWorldLoader` | — | Auditoria pendente |
| `TooltipLoader` | — | Auditoria pendente |
| `TouchButtonHooks` | — | Auditoria pendente |
| `TouchButtonLayouts` | — | Auditoria pendente |
| `TouchButtonLoader` | — | Auditoria pendente |
| `TownNPCLoader` | — | Auditoria pendente |
| `TreeLoader` | — | Auditoria pendente |
| `UndergroundBackgroundLoader` | — | Auditoria pendente |
| `VanillaMusic` | — | Auditoria pendente |
| `VanillaTooltips` | — | Auditoria pendente |
| `WallLoader` | BunnyWall, BunnyWorldLight | Parcial |
| `WaterStyleLoader` | — | Auditoria pendente |
| `WaterfallStyleLoader` | — | Auditoria pendente |
| `WingLoader` | — | Auditoria pendente |

Prioridades: receitas/itens e seus hooks; buffs/minions/projéteis; NPCs/bosses; tiles/móveis; montarias; biomas/ambiente; sistemas/UI/persistência. Preservar os 206 exemplos JS e o inventário oficial; este documento não substitui as matrizes funcionais. Não declarar liberdade equivalente à API PC enquanto persistirem lacunas.
