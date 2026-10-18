# Bunny Loader C# — lacunas de API priorizadas

**Somente pesquisa:** assinaturas JS do ExampleModBunny nao sao APIs C# prontas.
Esta lista indica o que implementar e testar futuramente; nenhuma entrada e promocao automatica de suporte.

## Candidatos

| Prioridade | Recurso | JS base e callbacks | Fontes JS | Alvo C# proposto |
| --- | --- | --- | ---: | --- |
| P1 | item_recipes | ModItem: AddRecipes | 46 | BunnyWeapon crafting bridge |
| P1 | item_prefixes | ModPrefix: Apply, ModifyValue, CanRoll | 2 | BunnyPrefix (ainda inexistente) |
| P1 | npc_loot | ModNPC: ModifyNPCLoot | 2 | BunnyNPC loot hook |
| P1 | projectile_spawning | ModProjectile: OnSpawn | 1 | ManagedProjectile spawn bridge |
| P2 | item_tooltips | ModItem: ModifyTooltips | 2 | BunnyWeapon tooltip API |
| P2 | projectile_collisions | ModProjectile: OnTileCollide, OnKill | 2 | BunnyProjectile collision hooks |
| P2 | npc_ai_lifecycle | ModNPC: AI, PreAI | 1 | BunnyNPC AI managed event |
| P2 | player_dash | ModPlayer: UpdateDash, OnStartDash | 1 | BunnyPlayer movement events |
| P2 | world_save | ModSystem: SaveWorldData, LoadWorldData | 1 | BunnyWorld persistence API |
| P3 | tile_interactions | ModTile: RightClick, ModifyLight | 14 | BunnyTile interaction API |
| P3 | projectile_drawing | ModProjectile: PreDraw | 5 | BunnyProjectile draw bridge |
| P3 | npc_bestiary | ModNPC: SetBestiary | 3 | BunnyNPC bestiary bridge |

## Arquivos-fonte JS que fundamentam cada candidato

### item_recipes

Criacao e registro de receitas para itens C# exigem verificacao no sistema nativo.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Items/Accessories/ExampleWings.js (ExampleWings: AddRecipes)
- samples/ExampleMod/content/Content/Items/Armor/ExampleBreastplate.js (ExampleBreastplate: AddRecipes)
- samples/ExampleMod/content/Content/Items/Armor/ExampleHelmet.js (ExampleHelmet: AddRecipes)
- samples/ExampleMod/content/Content/Items/Armor/ExampleLeggings.js (ExampleLeggings: AddRecipes)

### item_prefixes

Prefixos de itens e acessorios sao pedidos recorrentes. Inspecionar hook real de item prefix e o contrato de save antes de expor C#.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Prefixes/ExampleAccessoryPrefix.js (ExampleAccessoryPrefix: ModifyValue)
- samples/ExampleMod/content/Content/Prefixes/ExamplePrefix.js (ExamplePrefix: Apply, CanRoll, ModifyValue)

### npc_loot

Permitir drops condicionais sem depender do editor JS; exigir hook na thread do jogo e regras deterministicas.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/NPCs/ExampleBoss/ExampleBoss.js (ExampleBoss: ModifyNPCLoot)
- samples/ExampleMod/content/Content/NPCs/ExampleSlimeNPC.js (ExampleSlimeNPC: ModifyNPCLoot)

### projectile_spawning

Separar pedido de spawn MonoVM e criacao na thread do Terraria, com limites por tick e validacao.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Projectiles/ExampleBobber.js (ExampleBobber: OnSpawn)

### item_tooltips

Permitir descricoes dinamicas e traducoes nos itens gerenciados.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Items/ExampleTooltipItem.js (ExampleTooltipItem: ModifyTooltips)
- samples/ExampleMod/content/Content/Items/Weapons/Melee/ExampleYoyo.js (ExampleYoyo: ModifyTooltips)

### projectile_collisions

Callbacks de colisao e morte na API real antes de portar projeteis complexos.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Projectiles/ExampleSentry.js (ExampleSentry: OnKill, OnTileCollide)
- samples/ExampleMod/content/Content/Projectiles/ExampleSentryShot.js (ExampleSentryShot: OnKill)

### npc_ai_lifecycle

Ha prototipo AI, mas nao o ciclo completo por NPC a cada tick; medir custo e sincronizacao.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/NPCs/ExampleBoss/ExampleBoss.js (ExampleBoss: AI, PreAI)

### player_dash

ResetEffects/UpdateEquips nao garantem hooks de movimento; precisa API separada.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Common/Players/ExampleDashPlayer.js (ExampleDashPlayer: OnStartDash, UpdateDash)

### world_save

Persistencia por mundo para estados gerenciados, sem depender de campos nao serializados.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Common/Systems/DownedBossSystem.js (DownedBossSystem: LoadWorldData, SaveWorldData)

### tile_interactions

Interacao e emissao de luz em tiles gerenciados com limites de custo por frame.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Tiles/ExampleAnimatedTile.js (ExampleAnimatedTile: ModifyLight)
- samples/ExampleMod/content/Content/Tiles/ExampleCampfire.js (ExampleCampfire: ModifyLight, RightClick)
- samples/ExampleMod/content/Content/Tiles/ExampleLamp.js (ExampleLamp: ModifyLight)
- samples/ExampleMod/content/Content/Tiles/ExampleLivingFireTile.js (ExampleLivingFireTile: ModifyLight)

### projectile_drawing

Renderizacao de projeteis customizados exige roteamento correto para thread grafica.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/Projectiles/ExampleAdvancedAnimatedProjectile.js (ExampleAdvancedAnimatedProjectile: PreDraw)
- samples/ExampleMod/content/Content/Projectiles/ExampleFlailProjectile.js (ExampleFlailProjectile: PreDraw)
- samples/ExampleMod/content/Content/Projectiles/ExampleLaserBeam.js (ExampleLaserBeam: PreDraw)
- samples/ExampleMod/content/Content/Projectiles/ExampleSwingingEnergySwordProjectile.js (ExampleSwingingEnergySwordProjectile: PreDraw)

### npc_bestiary

Catalogo visual exige registro adicional, nao basta NPC.SetDefaults.

Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:
- samples/ExampleMod/content/Content/NPCs/ExampleBoss/ExampleBoss.js (ExampleBoss: SetBestiary)
- samples/ExampleMod/content/Content/NPCs/ExamplePerson/ExamplePerson.js (ExamplePerson: SetBestiary)
- samples/ExampleMod/content/Content/NPCs/ExampleSlimeNPC.js (ExampleSlimeNPC: SetBestiary)

## Como promover uma API

Implementar primeiro o contrato real C# no SDK e na ponte Bunny; adicionar um pequeno ExampleModCSharp.
Compilar com Roslyn, verificar em CI e confirmar no Android para o status device_verified.
A versao .86 validada nao e modificada por estas ferramentas.
