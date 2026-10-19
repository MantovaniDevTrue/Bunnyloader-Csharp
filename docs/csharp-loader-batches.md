# Entregas de cinco loaders

Regra atual do usuário em19/10/2026: desenvolver todos os loaders, entregando **cinco loaders completos por APK**, com **um exemplo original completo por loader**. Esta regra substitui a espera pelo port integral de todos os exemplos antes da próxima entrega. O objetivo global de portar todos os exemplos permanece; nenhum fonte existente será removido.

Um exemplo de teste pode precisar de item colocável, tile, buff, projétil, textura ou classe auxiliar. Essas dependências pertencem ao mesmo exemplo e precisam funcionar; não contam como loaders extras. Também não vamos contar TileDraw/TileUse/TileWorld como três loaders prontos: seus contratos fazem parte de TileLoader. A API precisa atender ao contrato completo do sistema, além do exemplo selecionado.

O plano executável está em [csharp-loader-batches.json](csharp-loader-batches.json). Contabiliza os88 módulos originais, distribuídos inicialmente entre49 sistemas, mais DustLoader do inventário PC. São dez grupos de cinco. É um inventário inicial: sistemas adicionais identificados na auditoria PC deverão entrar em novos grupos, sem desaparecer do escopo. Não é declaração de paridade com toda a API PC.

| Grupo | Cinco sistemas |
| --- | --- |
| 1 — mundo | Tile, Wall, Tree, TileEntity, GlobalTile |
| 2 — equipamentos | Item, Equip, ArmorSet, Wing, Prefix |
| 3 — combate | Projectile, DamageClass, GlobalProjectile, Buff, GlobalItem |
| 4 — NPCs | NPC, TownNPC, GlobalNPC, BossBar, GlobalLoot |
| 5 — conteúdo | Recipe, Rarity, Localization, Command, Config |
| 6 — ambiente | Biome, SceneEffect, SurfaceBackground, UndergroundBackground, MapBackground |
| 7 — efeitos | WaterStyle, WaterfallStyle, Cloud, Gore, Dust |
| 8 — interação | Mount, AnglerQuest, Pylon, Banner, Bestiary |
| 9 — jogador/UI | Achievement, EmoteBubble, Hair, TouchButton, Keybind |
| 10 — execução | Player, System, ServerSync, Sound, Menu |

A ordem pode mudar por dependências, sem dividir uma entrega em menos de cinco loaders completos. Serviços necessários de grupos posteriores podem ser implementados antes; não são anunciados como loader concluído só por desbloquear outro sistema.

## Grupo1 e exemplos escolhidos

| Loader | Exemplo principal | Dependências e contrato ainda pendente |
| --- | --- | --- |
| TileLoader | ExampleTile | Item colocável/receita/textura incluídos. Completar callbacks gerais de tile, interação, desenho/luz, frames e mundo; o bloco sozinho não certifica o loader. |
| WallLoader | ExampleWallAdvanced | Item, frames, dust condicional, animação, luz noturna, colocação/quebra e persistência. |
| TreeLoader | ExampleTree | Chão ExampleTile, ExampleSapling, folhas originais, crescimento, copa/galhos, madeira ExampleItem, bolotas e shake. |
| TileEntityLoader | BasicTileEntity | Barril de chuva original: instância por posição,3600ticks, toque/picareta/balde, reset, persistência e sincronização. Nome exibido terá prefixo Example; identidade preservada. |
| GlobalTileLoader | ShakeTrees do ExampleMod PC | Referência PC pinada; o pacote JS não contém uma classe que estenda GlobalTile. Auditar comportamento antes do port, incluindo composição de hooks e regras de drops. |

[Contratos de origem](generated/csharp-world-loader-contracts.json) registram os membros públicos declarados e hashes dos cinco tipos originais. São âncoras para revisão manual, não um parser semântico nem prova de execução. Chamadas internas, propriedades nativas usadas pelos exemplos, dependências e funcionalidades PC adicionais também precisam da auditoria; a lista lexical não limita o contrato.

## Estado desta execução

Revisão interna `.90.29.99-world-loaders-wip`, ExampleMod0.1.58. Novo caminho de flags tipadas para terreno e port de ExampleTile/ExampleTileItem com assets originais. O publicador C# também replica o pós-registro original: um tile marcado Solid ativa `Main.tileNoSunLight`, sem exigir um segundo campo público. Solid e BlockLight seguem distintos; o conjunto é validado antes da primeira escrita, inclusive se `tileNoSunLight` estiver ausente ou incompatível. Nenhum dos cinco loaders inteiros está certificado; portanto, **nenhum APK é entregue como grupo1 concluído**.

`BunnyTile.Flags` adiciona Solid, BlockLight, MergeDirt, Lighted, WaterDeath, ChecksForMerge e Cut. O publicador das flags booleanas resolve todos os arrays solicitados antes de escrever, valida campo estático, tipo Boolean[], rank1, elemento de1byte e índice. Falha de uma flag booleana preserva as demais sem escrita parcial desse conjunto. CountsAsTable utiliza separadamente a lista nativa Int32[] de RoomNeeds, sem exigir uma tabela bool fictícia; valida cópia, limite e duplicações antes de publicar a lista. As transações de outros sistemas continuam sendo contratos separados.

O protocolo TILE aceita os13 campos anteriores e uma14ª coluna opcional AnimationFrameHeight (0..4096); o compilador C# gera14 campos. O decoder mantém compatibilidade com13. A máscara aceita16bits conhecidos. Cache Roslyn v32 invalida o anterior. A altura nativa de frame não substitui AnimateTile, ainda pendente. Novo esquema16 campos: AnimationFrames (1..64) e AnimationFrameTicks (0..600), com frames>1 exigindo altura e ticks positivos. O C++ passa a atualizar Main.tileFrame/tileFrameCounter no hook nativo AnimateTiles, depois da animação vanilla, validando os arrays IL2CPP Int32[] e habilitando somente tipos C# registrados. O formato13/14 permanece aceito. O protocolo17 campos acrescenta AdjacentTiles: até16 aliases vanilla validados, ordenados e sem duplicatas; o registro nativo mantém todos os pares por tile, em vez de sobrescrever o anterior. A rotina Player.AdjTiles busca pares ordenados por tipo e marca todas as estações equivalentes, sem JS por frame. O formato16 também permanece aceito. O contrato arbitrário AnimateTile e desenho/interações dinâmicos ainda não estão completos; nenhum APK liberado. Não há execução QuickJS, IDs fixos de mod, alteração dos IDs existentes ou shaders novos.


### TileLoader — complemento nativo, ainda em desenvolvimento

O SDK C# mantém o contrato de animação por altura, quadros e ticks com validações e atualiza o motor por Main.AnimateTiles. O SDK também aceita até16 equivalências AdjacentTiles por tile, mantendo a propriedade antiga AdjacentTile. As listas são ordenadas e consultadas no hook nativo Player.AdjTiles, com a mesma tabela de tipos da engine. O wire TILE mantém os formatos 13/14/16/17 e agora acrescenta 19 campos; o compilador novo escreve 19. Cache Roslyn v35. Nenhum caminho exige JavaScript para executar essas duas políticas.

Arquivos de teste adicionais: `tests/world_loaders/tile_animation_test.cpp` e `tests/world_loaders/tile_adjacency_test.cpp`. As respectivas políticas foram reproduzidas e compiladas isoladamente com G++20, -Wall, -Wextra, -Werror, ASan e UBSan, retornando PASS. Isso **não** valida ainda a compilação do código completo da branch, o ABI dos campos Main.tileFrame/tileFrameCounter no Terraria Android nem o comportamento dentro do jogo. Os testes de política de wire e de quebra/drop foram executados localmente com G++20, -Wall, -Wextra, -Werror, ASan e UBSan, retornando PASS. Os quatro arquivos (cabeçalhos e testes) tiveram seus hashes git blob comparados com os do GitHub, todos idênticos. O CI/Android desta revisão ainda não foi executado e o APK continua bloqueado.

Pendências principais: callbacks de TileDrawLoader (PreDraw, PostDraw, DrawEffects, ModifyLight dinâmico e afins), TileUseLoader (RightClick, smart-interact, mouseover, resting e móveis), TileWorldLoader (PlaceInWorld, RandomUpdate, wires, nearby, slope), hooks de quebra e drop modificáveis pela lógica C# e testes reais de save/reload/multiplayer. A animação por cadência fixa não equivale ao callback livre AnimateTile. Progresso adicional: no protocolo19 campos, BunnyTile.AllowBreaking e BunnyTile.AllowDrops (true por padrão) são regras de carregamento puramente nativas. A primeira usa hook de WorldGen.CanKillTile com verificação rigorosa da assinatura IL2CPP e bloqueia apenas quando a declaração pede false; a segunda impede os drops da rota nativa de KillTile_GetItemDrops, sem limitar customizações posteriores. O hook de CanKillTile só é instalado sob demanda e um ABI incompatível recusa o tile sem ativar registro de gameplay. São políticas estáticas, não equivalem aos callbacks dinâmicos originais CanKillTile/CanDrop. O decodificador de 17 campos foi corrigido: antes de 75b49a4 não interpretava AnimationFrames/AnimationFrameTicks quando recebia AdjacentTiles. Uma política separada com testes preserva os formatos anteriores. Sem testes reais de device/ABI, esta integração não está certificada. Suporta wire13/14/16/17 legados; v35 emite19. Portanto, **TileLoader não está certificado completo**.


## Critério e gate de entrega

`tools/loader_batch_gate.py` confere todos os88 módulos, duplicações, cinco sistemas por grupo e contratos de origem. `--delivery` exige cinco contratos e exemplos completos, ausência de funcionalidades pendentes e recibos de revisão com cobertura de cada membro, fontes SDK/nativas/testes e hashes de todos os módulos envolvidos. Mudança no original invalida o recibo. Não basta alterar um campo para complete.

Os recibos documentam a revisão humana; não substituem execução. Depois desse gate, continuam obrigatórias as suítes independentes no mesmo SHA, build Android, assinatura e destino privado existentes. O CI não publica enquanto o grupo estiver incompleto. O publicador direto também executa o gate antes de acessar credenciais/rede. `--stable` exige adicionalmente evidência Android para cada loader. A entrega inicial para testes continua explicitamente experimental e não certifica FPS.

## Continuidade exata

1. Preservar o caminho nativo de bancada/receitas/armaduras e os demais sistemas estáveis; .96 continua sendo a aprovação geral mais recente do usuário.
2. Completar o contrato TileLoader com seus auxiliares TileDraw/Use/World e revisar colocação/quebra/drop/mapa/save do ExampleTile no Android quando o grupo estiver disponível.
3. Criar registro público de paredes C# usando o serviço nativo de paredes, com callbacks reais necessários a ExampleWallAdvanced; não registrar só a textura.
4. Integrar TreeLoader ao serviço compartilhado ModTrees e implementar crescimento/sapling/shake/folhas sem QuickJS. Não publicar uma árvore sem suas dependências.
5. Implementar entidades de tile e globais com ciclo de vida, estados, serialização e sincronização. Auditar o exemplo PC pinado antes de adaptar seu comportamento.
6. Adicionar testes de execução de cada integração, concluir os recibos, executar regressões e build. Somente os cinco completos desbloqueiam o próximo APK.

As matrizes206 JS e o inventário PC permanecem oficiais. Implementado, teste automatizado, build Android e validação no telefone são estados separados.

## Evidência deste checkpoint

Código `f70caa8044c2d6ae8a27716c6526a40080446601`: [CI39246685678](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39246685678) aprovado, incluindo ARM64, testes Gradle, APK e assinatura. Nove suítes independentes de push aprovadas no mesmo SHA. Publicação, exportação de .bmod e job de conferência privada pulados; somente artefatos de resultados de testes/medições exportados. Repositório Builds privado confirmado e tag deste run ausente (HTTP404).

[Registro completo](validation/csharp-world-loaders-v99-2026-10-19.json), com etapas, suítes, contagens e hashes dos dois assets originais. O commit posterior só registra essa evidência; o código compilado é o SHA acima. Gameplay Android/FPS pendentes e zero loaders inteiros certificados. Próxima execução continua os contratos do grupo1; não remove a .96 nem publica um subconjunto.
