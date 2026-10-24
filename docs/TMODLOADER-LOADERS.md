# Matriz de loaders tModLoader → Bunny C#

Referência: tModLoader **1.4.5**, commit [`fcafebc0b792`](https://github.com/tModLoader/tModLoader/tree/fcafebc0b792b90e110bac7c1fc67cac81cb60ed). Inventário de **43 arquivos** `*Loader.cs`, incluindo arquivos com mais de uma classe. [Dados e hashes](tmodloader-loader-inventory.json).

O objetivo é portar todos os loaders. Esta matriz não declara paridade Android por existir uma classe C# ou um loader JavaScript. `Não auditado` exige comparar API, hooks, assinaturas, callbacks, assets, persistência, rede, unload e testes antes de iniciar um lote. PRs citadas são um retrato de 22/10/2026 e podem mudar. A ausência de PR nesta tabela não prova que a área está livre: a seleção exige nova busca de PRs e branches.

**Lote atual:** núcleo oficial `Loader`, `Loader<T>`, `ILoader`, `LoaderManager`, consulta `ModTypeLookup<T>` e ciclo gerenciado. [Escopo, diferenças e bloqueios](csharp-content-loader.md). Não representa a conclusão dos loaders de gameplay. **APK continua proibido.**

| Arquivo oficial | Classes | Situação C# observada | PRs relacionadas |
| --- | --- | --- | --- |
| `AccessorySlotLoader.cs` | `AccessorySlotLoader` | Não auditado contra o C# |  |
| `BiomeConversionLoader.cs` | `BiomeConversionLoader` | Não auditado contra o C# |  |
| `BiomeLoader.cs` | `BiomeLoader` | Trabalho em PR; não duplicar | [#125](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/125) |
| `BossBarLoader.cs` | `BossBarLoader` | Não auditado contra o C# |  |
| `BuffLoader.cs` | `BuffLoader` | Não auditado contra o C# |  |
| `BuilderToggleLoader.cs` | `BuilderToggleLoader` | Não auditado contra o C# |  |
| `CloudLoader.cs` | `CloudLoader` | Trabalho em PR; não duplicar | [#125](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/125) |
| `CommandLoader.cs` | `CommandLoader` | Trabalho em PR; não duplicar | [#117](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/117) |
| `DamageClassLoader.cs` | `DamageClassLoader` | Trabalho em PR; não duplicar | [#135](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/135) |
| `DustLoader.cs` | `DustLoader` | Trabalho em PR; não duplicar | [#115](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/115) |
| `EmoteBubbleLoader.cs` | `EmoteBubbleLoader` | Não auditado contra o C# |  |
| `EquipLoader.cs` | `EquipLoader` | Não auditado contra o C# |  |
| `ExtraJumpLoader.cs` | `ExtraJumpLoader` | Não auditado contra o C# |  |
| `GoreLoader.cs` | `GoreLoader` | Trabalho em PR; não duplicar | [#121](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/121) |
| `HairLoader.cs` | `HairLoader` | Trabalho em PR; não duplicar | [#134](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/134) |
| `InfoDisplayLoader.cs` | `InfoDisplayLoader` | Não auditado contra o C# |  |
| `ItemLoader.cs` | `ItemLoader` | Não auditado contra o C# |  |
| `KeybindLoader.cs` | `KeybindLoader` | Trabalho em PR; não duplicar | [#120](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/120) |
| `LiquidLoader.cs` | `LiquidLoader` | Não auditado contra o C# |  |
| `Loader.cs` | `Loader`, `Loader<T>`, `LoaderManager` | Núcleo e bootstrap de host implementados; QA Android pendente | [#134](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/134), [#135](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/135), [#125](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/125), [#131](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/131), [#126](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/126), [#120](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/120), [#121](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/121), [#102](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/102), [#117](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/117), [#118](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/118), [#100](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/100), [#114](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/114), [#115](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/115), [#101](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/101) |
| `LocalizationLoader.cs` | `LocalizationLoader` | Trabalho em PR; não duplicar | [#101](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/101) |
| `MapLayerLoader.cs` | `MapLayerLoader` | Não auditado contra o C# |  |
| `MapLoader.cs` | `MapLoader` | Não auditado contra o C# |  |
| `MenuLoader.cs` | `MenuLoader` | Não auditado contra o C# |  |
| `ModLoader.cs` | `ModLoader` | Não auditado contra o C# |  |
| `MountLoader.cs` | `MountLoader` | Não auditado contra o C# |  |
| `MusicLoader.cs` | `MusicLoader` | Não auditado contra o C# |  |
| `NPCHeadLoader.cs` | `NPCHeadLoader` | Não auditado contra o C# |  |
| `NPCLoader.cs` | `NPCLoader` | Não auditado contra o C# |  |
| `PlayerDrawLayerLoader.cs` | `PlayerDrawLayerLoader` | Não auditado contra o C# |  |
| `PlayerLoader.cs` | `PlayerLoader` | Não auditado contra o C# |  |
| `PrefixLoader.cs` | `PrefixLoader` | Trabalho em PR; não duplicar | [#100](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/100) |
| `ProjectileLoader.cs` | `ProjectileLoader` | Não auditado contra o C# |  |
| `PylonLoader.cs` | `PylonLoader` | Não auditado contra o C# |  |
| `RarityLoader.cs` | `RarityLoader` | Trabalho em PR; não duplicar | [#102](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/102) |
| `RecipeLoader.cs` | `RecipeLoader` | Não auditado contra o C# |  |
| `ResourceDisplaySetLoader.cs` | `ResourceDisplaySetLoader` | Não auditado contra o C# |  |
| `ResourceOverlayLoader.cs` | `ResourceOverlayLoader` | Não auditado contra o C# |  |
| `SceneEffectLoader.cs` | `SceneEffectLoader<T>`, `SceneEffectLoader` | Trabalho em PR; não duplicar | [#131](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/131) |
| `SystemLoader.cs` | `SystemLoader` | Trabalho em PR; não duplicar | [#1](https://github.com/MantovaniDevTrue/Bunny-Loader-Csharp/pull/1) |
| `TileLoader.cs` | `TileLoader` | Não auditado contra o C# |  |
| `WallLoader.cs` | `WallLoader` | Não auditado contra o C# |  |
| `WaterStyleLoader.cs` | `WaterFallStylesLoader`, `WaterStylesLoader` | Não auditado contra o C# |  |

Loaders exclusivos do Bunny (como Sound, Tree e AnglerQuest) e auxiliares como `ModTypeLookup` não são arquivos `*Loader.cs` oficiais deste inventário; isso não os remove do roadmap. Não confundir contagem de arquivos com quantidade de recursos completos.
