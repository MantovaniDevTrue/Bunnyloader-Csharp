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


## TileLoader — callbacks MonoVM e scanner nativo (em implementação)

O SDK declara13 callbacks virtuais e registra seletivamente os overrides no protocolo TILE de20 campos (cache Roslyn v36); os formatos13/14/16/17/19 permanecem aceitos. BunnyTileRuntime é incluído explicitamente em BunnySourceSdk.dll e prepara classes e callbacks antes do uso; a execução de callbacks C# não depende de QuickJS.

Conexões em código: CanKillTile e CanDrop com resposta limitada por tempo, HitWire e RandomUpdate com fila limitada, RightClick com ordem corrigida e prevenção de interação repetida, Slope/PoundTile, PlaceInWorld durante colocação de tile simples, AnimateTile com controle de frame/contador, MouseOver e MouseOverFar com deduplicação por quadro e fallback de cursor para RightClick. ModifyLight transmite RGB inteiro quantizado de0 a4000 (escala1/1000), validado no worker e no C++, com até3 amostras síncronas por atualização e cache. NearbyEffects envia o indicador de área visual/área próxima ao worker, com varredura nativa limitada a6 células por contexto a cada6 atualizações. Ambos usam arrays de tiles nativos, sem varredura JavaScript. O scanner toma o centro da tela como aproximação para a área de buff; **ainda não replica exatamente SceneMetricsScanSettings**. A aplicação da luz por Lighting.AddLight em Main.AnimateTiles requer verificação em cada motor de iluminação do Terraria Android. Os hooks usam validação de assinaturas/campos IL2CPP e recusam políticas indisponíveis em vez de aceitar overrides silenciosamente.

A política NativeTileScanPolicy.h, usada no código de produção, tem testes unitários para RGB inválido, limites, e chaves de posição. Seu teste passou localmente com G++20, -Wall, -Wextra, -Werror, ASan e UBSan, com hashes dos blobs verificados contra o GitHub. A política de máscaras de callbacks e de quebra/drop também passou sob esses parâmetros. Os testes C# no host foram expandidos para ModifyLight e NearbyEffects em ambos os contextos, mas **não foram executados nesta revisão por falta de compilador .NET neste ambiente**.

**Ainda incompletos:** PreDraw, PostDraw, DrawEffects, SpecialDraw, EmitParticles e o contexto de renderização SpriteBatch/DrawData original; efeitos de partículas, alguns hooks de framing e de móveis/portas/baús, item de PlaceInWorld, e contratos de multiplayer/salvamento. O registro recusa esses overrides gráficos até haver implementação real, sem remover efeitos ou transformar callbacks em operações vazias. A compilação Android no SHA atual, testes integrados de ABI e testes de jogo/60 FPS permanecem pendentes. **TileLoader não está certificado completo; a PR continua draft e a publicação de APK/.bmod bloqueada.**

## Continuação integrada: .100 / ExampleMod 0.1.59

Base incorporada: os64 commits do usuário até `7e97db43`, preservados na história da branch. A integração corrige o teste de bancada que ainda esperava13 colunas em TILE, a inicialização incompleta de NativeTileGameplaySpec e dois avisos fatais de indentação do transporte MonoVM. Erros de callbacks de tile agora conservam a exceção/stack trace e ganham diagnóstico nativo com contagem limitada.

Novo caminho: `BunnyWall` → WALL → SourceLoader → NativeWallRuntime → serviços nativos ModWalls/ModTileMap. Registro, compilação de fontes e identidade por domínio incluem paredes; `ExampleWallAdvanced` item e parede podem ter o mesmo nome. O exemplo utiliza os dois PNGs originais, sem substituir exemplos anteriores. As políticas públicas incluem moradia, blend por nome WallID, dust por frame/quantidade, seleção probabilística de frames, animação por cadência e luz diurna/noturna. São políticas executadas no motor, não callbacks gerenciados arbitrários.

`BunnyItem.DefaultToPlaceableWall` resolve chaves locais/UID de mods C# ou JS no registro compartilhado e chama o helper real `Item.DefaultToPlaceableWall(ushort)`. `createWall` tem tipo, largura e offset validados. Overrides após o helper são serializados por máscara, preservando atribuições explícitas a0/false. Antes de resolver uma dependência, escreve zero em campo de parede unsigned. Limitação declarada: getters do objeto C# durante SetDefaults retornam valores da declaração; não consultam o objeto Terraria que só receberá o helper na thread do jogo. Não declarar equivalência de introspecção desses defaults com a API PC.

WALL tem16 colunas; itens de parede usam o token `wmChave:0:IntegerMask:FlagMask` ou `wvID:0:IntegerMask:FlagMask`. Tokens antigos de colocação de tiles continuam aceitos. Cache Roslyn v37 invalida compilados incompatíveis. IDs numéricos não são persistidos pelo exemplo. Save/mundo/mapa usam os serviços existentes e ainda exigem validação no Android para este novo port.

ModWalls mantém listeners separados para JS e C#. A recuperação após recriação das tabelas reaplica defaults e substitui atomicamente entradas de mapa, sem duplicar opções nem alterar a parede de outro mod. Falhas de texture/table/mapa são contabilizadas; não produzem um resumo verde nem ficam esperando eternamente se o serviço nativo já recusou o conteúdo. WallID.Count UInt16, wallHouse Boolean[] e wallBlend Int32[] são conferidos antes das operações. Registros com capacidade UInt16 esgotada são recusados com causa específica. O loop de luz ignora a varredura quando todas as cores do período são zero e não entra em Mono/QuickJS por célula.

Testes host:39 casos de itens/fontes, suítes de tiro/summon/receitas/projéteis/combate/entidades, políticas de tiles e testes novos de produção para paredes. `wall_runtime_test.cpp` exercita ABI/erros, commit de defaults, textura ausente, orçamento de dust, drops, frames, luz e restauração; o motor é representado por metadados controlados. `wall_registry_test.cpp` liga os registradores/mapas reais sem QuickJS e testa chaves, duplicatas, substituição atômica e convivência de proprietários. `native_item_binding_test --wall` testa o helper real da bridge com motor controlado e sentinelas de memória. Esses testes não certificam execução no dispositivo.

**Estado:** TileLoader e WallLoader parciais; TreeLoader, TileEntityLoader e GlobalTileLoader seguem pendentes. A matriz preserva todos os206 exemplos. Faltam callbacks gerenciados arbitrários de parede e os contratos restantes dos cinco sistemas; portanto, nenhum APK/.bmod é exportado como entrega do grupo. O gate não foi afrouxado. Compilação Android e validação no telefone continuam estados separados dos testes host.

### Integração CI e ciclo de vida TileLoader (19/10)

O lote do Codex em `08c83e1` adicionou WallLoader parcial e elevou ExampleMod CSharp para 0.1.59; os jobs de SDK/combat falhavam por verificações de versão anteriores e foram corrigidos. O workflow Android também tentava empacotar `examplemodcsharp-0.1.58.bl`; o pipeline foi alinhado a 0.1.59 (commit `a50447f`). Os hooks de quebra do TileLoader agora preservam cada instalação bem-sucedida e permitem retentar apenas os hooks faltantes após falha parcial (`6fe269e`), evitando instalar novamente o mesmo trampoline. O cache RGB agora associa a cor ao tipo de tile e expira ao substituir por outro tipo de mod (`8260d96`). A compilação Android, os hooks gráficos e a validação em aparelho continuam pendentes; a PR permanece draft sem publicação de APK.

### TileLoader C# — draw-data nativo independente

Os callbacks C# `SetDrawPositions`, `AnimateIndividualTile` e `SetSpriteEffects` agora usam a entrada do hook real `TileDrawing.GetTileDrawData` do Bunny original. A publicação dessas três políticas ocorre pelo MonoVM com um pacote tipado `DRAW`; o decoder nativo valida os oito campos de frame, tamanho, deslocamento e efeito antes de escrever na célula. O cache existente de desenho de tiles é reutilizado, sem atravessar QuickJS nem sobrescrever o callback de desenho de mods JS. O loader só ativa a máscara específica de cada tipo após conferir a instalação dos hooks de desenho padrão/especial, com tentativa segura de recuperar falhas parciais.

O contrato SDK C# agora comporta16 overrides verificáveis e o compilador Roslyn usa o cache v38 (conservando os defaults de paredes do Codex). A máscara ocupa16 bits no registro TILE de20 colunas, sem mudar as colunas de mods existentes. Foram adicionados testes de integração do dispatcher C# para os três callbacks, passagem de desenho especial e rejeição de transporte inválido, além de `tests/world_loaders/tile_draw_wire_test.cpp` para validar a rotina real de resposta C++. Os novos testes de C++ são executados por `tests/world_loaders/run.sh`.

**Ainda pendentes:** `PreDraw`, `PostDraw`, `DrawEffects`, `SpecialDraw` e `EmitParticles` com acesso real ao contexto `SpriteBatch`/partículas. O `SetDrawPositions` não substitui esse contexto nem torna completo o TileLoader. A build anterior (SHA `8260d96`) foi concluída com sucesso antes desses commits; a build/QA atual e os testes de jogo Android ainda precisam aprovar esse caminho. Nenhum APK de teste deve ser liberado enquanto os callbacks gráficos restantes e o gate de cinco loaders não estiverem concluídos.

### TileLoader — publicação gráfica imutável e compatibilidade com o NDK

Os métodos C# de transformação de desenho (`SetDrawPositions`, `AnimateIndividualTile`, `SetSpriteEffects`) publicam snapshots imutáveis por tipo no cache de desenho do Bunny, sem chamadas JavaScript. A verificação Android do SHA `dc4390527` falhou durante a compilação C++ de `NativeTileGameplay.cpp`: o NDK libc++ rejeitou `std::atomic<std::shared_ptr<const TileBehavior>>` por não ser um tipo trivial. O código passou a usar `PublishedBindings`, que protege apenas a cópia/publicação de `std::shared_ptr<const T>` com `std::shared_mutex`. Depois de liberar a trava, o renderizador mantém seu próprio snapshot imutável; não mantém o mutex preso durante o callback MonoVM.

O teste `tile_draw_bindings_test.cpp` reproduz atualização de um tipo enquanto outra thread lê o snapshot, checa consistência dos campos, limites de ID e vida útil das instâncias antigas. A compilação e execução isoladas passaram localmente em G++ 14 e Clang++ 17 com `-std=c++20 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined`. O teste entra em `tests/world_loaders/run.sh`. **Ainda falta a confirmação pela build NDK no novo SHA, além de jogo real**. `PreDraw`, `PostDraw`, `DrawEffects`, `SpecialDraw` e `EmitParticles` com `SpriteBatch` permanecem sem paridade completa, e o gate de APK segue fechado.

### CacheDrawData C# e compatibilidade do wire — revisão 21

O SDK C# agora expõe `BunnyTile.CacheDrawData` (padrão true), como o ModTile original do ExampleModBunny. Quando false, somente o tipo C# marcado evita reutilizar a saída do cache de `TileDrawing.GetTileDrawData`. Os mods JS continuam usando sua marca original `tile.drawdata.uncached`, sem alteração de comportamento. A política nativa `NativeTileDrawCachePolicy.h` combina o passe especial e as duas marcas sem desativar o cache dos demais tiles. A opção é declarada durante `SetStaticDefaults`; isso não implementa sozinho `PreDraw`, `PostDraw`, `DrawEffects`, `SpecialDraw` nem o contexto `SpriteBatch`.

O protocolo `TILE` acrescenta o21º campo opcional `CacheDrawData=0|1`, conservando suporte a13/14/16/17/19/20 colunas. O compilador Roslyn v39 emite21 campos, o registrador copia a política para os metadados nativos e a publicação de marcas segue os bindings imutáveis, sincronizados com a thread de desenho. O teste `tile_draw_cache_test.cpp` é executado pelo gate de world loaders. Os fixtures do ExampleWorkbench e do tile animado validam respectivamente o cache padrão ligado e o cache desativado. A primeira rodada de auditoria encontrou uma expectativa antiga de20 campos no teste original de bancada, corrigida no commit e95cdff.

**Gate continua fechado:** os callbacks gráficos que desenham sprites/partículas ainda não estão implementados com paridade completa; APK de teste e declaração de TileLoader completo dependem da compilação Android do SHA atual e da validação no aparelho.

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

### AnimateIndividualTile C# — paridade de assinatura e wire

A API do Bunny original fornece `AnimateIndividualTile(type, i, j, frameXOffset, frameYOffset)`. A versão C# passou a disponibilizar a sobrecarga com `type` e preservou a assinatura anterior como compatibilidade. O callback completo delega ao antigo quando o mod apenas sobrescreve a assinatura legada. A máscara de overrides e o prewarm reconhecem ambas as sobrecargas sem `GetMethod` ambíguo.

O snapshot da célula no MonoVM agora inclui o tipo nativo registrado como décimo segundo campo. O dispatcher também aceita os onze campos anteriores para a assinatura legada, valida o tipo novo dentro do intervalo de tiles de mod e repassa o valor sem JS. A referência pública do SDK foi sincronizada com a nova sobrecarga. Este avanço não implementa nem certifica `PreDraw`, `PostDraw`, `DrawEffects`, `SpecialDraw` ou `EmitParticles`; continua proibido gerar APK antes de completar os cinco loaders.

### TileLoader C# — PreDraw booleano no caminho nativo (parcial)

O override `BunnyTile.PreDraw(x,y)` passa a integrar o mesmo `TileDrawing.GetTileDrawData` usado por `SetDrawPositions`/animação, apenas no passe normal e sem QuickJS. O dispatcher retorna opcionalmente o nono campo `drawDefault=0|1` no registro `DRAW`; os oito campos anteriores continuam aceitos. O decoder rejeita campos adicionais/malformados e o nativo zera a largura/altura somente quando `PreDraw` devolve `false`. Erro, warmup pendente ou deadline preservam a renderização original; não há ocultação por fallback. Um tile que defina somente PreDraw também passa a registrar seu caminho de desenho. O teste real do compilador Roslyn confirma máscara de override, passe normal/especial e true/false; o teste C++ cobre a compatibilidade do protocolo.

`CacheDrawData=true` segue com o cache nativo de até 5 s e invalidação por mundo/enquadramento; quem precisa do retorno de PreDraw recalculado durante animações deve declarar `CacheDrawData=false`, com custo maior do MonoVM. **Limitação intencional:** esta implementação decide apenas o desenho padrão; ainda não dá um `SpriteBatch` utilizável pelo mod, nem substitui o `PreDraw` que desenha sprites personalizados. `PostDraw`, `DrawEffects`, `SpecialDraw` e `EmitParticles` continuam pendentes. Não marcar o TileLoader completo, não publicar APK ou bmod até terminar o grupo de cinco loaders e a validação Android.

### TileLoader C# — adaptador nativo de SpriteBatch e protocolo SPRITES (revisão 24)

Foi incluído `NativeTileSpriteBatch`, um adaptador privado para `Microsoft.Xna.Framework.Graphics.SpriteBatch.Draw(Texture2D, Vector2, Nullable<Rectangle>, Color)`. Ele valida por metadados IL2CPP a assinatura real, o layout de Vector2/Rectangle?/Color, o estado `_beginCalled` e a tabela `TextureAssets.Tile`. Os sprites usam a textura já carregada pelo Bunny, o recorte de frame, a cor e a posição de tela relativa à câmera. A chamada nativa usa `runtime_invoke`, sem transferir objetos Unity à thread MonoVM.

O decoder `NativeTileSpriteWire.h` aceita `SPRITES|0` ou até 8 entradas tipadas em até 1024 bytes. Rejeita coordenadas, IDs, recortes, offsets, contagens e cores fora dos limites; não faz alocações por frame. Os testes de host verificam exemplos válidos/malformados e compilam a unidade nativa com `-Werror -fsyntax-only`. Isso implementa a **ponte privada de desenho**, mas ainda não a liga ao `TileDrawing.PostDrawTiles` e não declara paridade de `PreDraw`, `PostDraw`, `DrawEffects`, `SpecialDraw` ou `EmitParticles`. Antes de ativar o hook é obrigatório validar quem abre e encerra o SpriteBatch e como o desenho padrão do Bunny coexiste com os mods JS. O grupo de cinco loaders e o gate de APK continuam fechados.

### TileLoader — PostDrawTiles nativo e fila de sprites (revisão 25)

Um novo hook nativo `NativeTileSpritePass` implementa a mesma seleção de passada do `TileDrawLoader.js`: primeiro chama o desenho original e só atua em `!solidLayer && !intoRenderTargets && !Main.gameMenu`. O instalador compara o método de três parâmetros booleanos e valida os campos estáticos reais de `Main` antes de instalar pelo gerenciador compartilhado de hooks; isso evita substituir o hook do JavaScript. Os comandos são armazenados numa fila nativa de capacidade 32 por `GameUpdateCount`, sem transferir objetos Unity entre threads, e a fila só é consumida no frame correspondente.

A passada **nunca inicia nem encerra um SpriteBatch alheio**: só desenha quando `_beginCalled` indica lote aberto. Os comandos não são processados se o lote estiver fechado. Esta é uma etapa do ciclo de vida; ainda precisa abrir/encerrar o lote exatamente como o `TileDrawLoader.#Begin`, e conectar a emissão dos callbacks C# `PreDraw/PostDraw/DrawEffects/SpecialDraw/EmitParticles`. As funções `installNativeTileSpritePass` e `stageNativeTileSprites` ficam opt-in e não são chamadas pelo registro de tiles nessa revisão, para não adicionar trabalho no caminho quente ou alterar mods JS antes da validação da API gráfica. Testes C++ verificam seleção de passada, limites, expiração de frame, concorrência e compilação sintática sem QuickJS. Nada disso certifica gameplay ou 60 FPS Android. Nenhum APK/.bmod até concluir o grupo de cinco loaders.

### SpriteBatch C# — ciclo de vida nativo do lote (revisão 26)

A passada opt-in tem suporte para abrir e fechar o `SpriteBatch` usando o mesmo contrato de oito parâmetros do `TileDrawLoader.#Begin`, com `SpriteSortMode=0`, `BlendState.AlphaBlend`, `Main.DefaultSamplerState`, `DepthStencilState.None`, `Main.Rasterizer`, `Main.Transform` e `defferedBatch=true`. O binding valida o tipo/armazenamento dos campos, getters e layout de `Nullable<Matrix>`; obtém objetos via getters estáticos ou campos do jogo e nunca transfere ponteiros IL2CPP para MonoVM. O método só executa `End` se a própria passada C# abriu o lote; caso a passada JS tenha deixado o batch aberto, reutiliza sem fechá-lo. Uma falha de binding impede a instalação do hook; callback ausente mantém o renderizador original.

O fluxo permanece opt-in, **sem produção de sprites C#** nesta revisão. Os testes de host cobrem as escolhas `Reject/Reuse/Begin`, encerramento condicionado e compilação da unidade com `-Werror`. A chamada `Begin/End` ainda não está validada no Android: não promover APK nem paridade completa de TileLoader antes de testes reais no Terraria.

### SpriteBatch — sobrecarga de nove parâmetros e SpriteEffects (revisão 27)

O adaptador de sprites C# agora resolve exatamente a sobrecarga `SpriteBatch.Draw(Texture2D, Vector2, Nullable<Rectangle>, Color, float, Vector2, float, SpriteEffects, float)` utilizada nos exemplos originais do Bunny. A configuração básica usa rotação 0, origem (0,0), escala 1 e profundidade 0. O decoder `SPRITES` mantém os 10 campos anteriores (efeito 0) e aceita o 11º campo `SpriteEffects`, limitado a 0–3, incluindo espelhamento horizontal e vertical. A suíte C++ cobre os formatos e valores inválidos. Texturas glow independentes, iluminação nativa e emissão dos callbacks permanecem pendentes; nenhuma nova renderização será ativada antes da integração e validação Android.

### Otimização da passada gráfica — verificação antecipada do frame (revisão 28)

O `SpritePassQueue` publica um `pendingFrame` atômico e expõe `pendingFor(GameUpdateCount)`. O hook agora lê o frame primeiro e só consulta matriz, `SpriteBatch` ou executa `Begin` quando existem sprites para o mesmo quadro. Comandos atrasados permanecem na fila até serem substituídos ou até o reset, sem reabrir um lote vazio em todos os quadros. Os testes de host verificam frames antigos, leitura sem consumir, consumo e expiração, preservando o limite de 32 sprites por quadro. A integração segue opt-in e requer testes reais Android antes de ser ativada para mods.

### PreDraw C# — emissão de sprites tipados até a passada nativa (revisão 29)

A sobrecarga `PreDraw(x,y,BunnyTileDrawContext)` expõe `DrawTileTexture` com frame, tamanho, offset, cor ARGB e SpriteEffects. O contexto controla 8 desenhos por célula, exclusivamente a textura nativa do próprio tile; a assinatura anterior `PreDraw(x,y)` continua funcionando. `BunnyTileRuntime` inclui os comandos na mesma resposta `DRAW` como uma única seção opcional `SPRITES`, sem segundo RPC. O nativo valida o wire, propriedade das coordenadas/ID e enfileira até 32 sprites para o quadro corrente; se houver comandos, mas a passada não estiver ativa, a fila estiver cheia ou a resposta for inválida, não aplica a supressão de renderização ao tile. O contexto não suporta outras texturas, iluminação, glowmask nem callbacks especiais nesta fase.

O hook de pós-desenho é instalado apenas quando existe PreDraw C# registrado, sem alterar o hook JS. Os autores de mods devem definir `CacheDrawData=false` para desenhar a cada quadro, pois armazenar o resultado do PreDraw impede a emissão dos comandos nos quadros seguintes. Os testes C++ cobrem o protocolo combinado e o compilador Roslyn verifica a emissão do contexto, o tipo nativo e a máscara do callback. Continua pendente testar no Android a ordenação real da passada, a luz, a transparência e a estabilidade com vários tiles visíveis. Não gerar APK/.bmod até fechar o grupo de cinco loaders e a validação prática.

### TileLoader C# — PostDraw na fila nativa (revisão 30)

A API `BunnyTile.PostDraw(x,y,BunnyTileDrawContext)` reutiliza o contexto de sprites do `PreDraw`, com máscara nativa bit 16. A sobrecarga é detectada por reflexão e registrada mesmo sem PreDraw, com callback via o único RPC `DRAW` existente por célula. Se ambos os callbacks estão sobrescritos, o worker executa PreDraw antes de PostDraw e mantém até oito comandos por célula, desenhados após os tiles padrão na passada nativa. A supressão do desenho padrão por PreDraw continua protegida pelo sucesso do enfileiramento dos sprites.

PostDraw só atua na passada normal e com tipo de tile validado (snapshot nativo de 12 campos). O protocolo legado de 11 campos permanece funcionando para as operações antigas e não permite PostDraw. Testes de Roslyn exercitam PostDraw isolado, conjunto PreDraw+PostDraw, ordenação, rejeição do tipo ausente e a passada especial; testes de host validam a máscara até o bit 16 e rejeitam o bit 17. A emissão de texturas externas, glowmasks, iluminação e callbacks DrawEffects/SpecialDraw/EmitParticles continua pendente. Para desenho por frame, usar `CacheDrawData=false`; não publicar APK nem afirmar 60 FPS até teste Android.

### TileLoader C# — variantes de textura _Glow e _Flame (revisão 31)

O contexto de desenho aceita `DrawGlowTexture` e `DrawFlameTexture` como camadas 1 e 2, mantendo `DrawTileTexture` como camada 0. O wire SPRITES aceita a camada como 12º campo opcional (10 e 11 campos anteriores ainda válidos). O carregamento usa o `ContentAssets.loadTextureAsset` nativo e somente os caminhos irmãos `<tile>_Glow.png` e `<tile>_Flame.png` do arquivo de textura registrado no Bunny; não há caminhos arbitrários nem acesso a assets de outro mod. Somente os tiles C# com callbacks de desenho solicitam as duas texturas, e a leitura do PNG acontece uma vez na thread do jogo, após o registro do conteúdo, não no desenho. Os assets são mantidos por gchandles no registro nativo do tile.

Antes de enfileirar, cada camada extra deve existir e o recorte deve caber nas dimensões reais da textura. A ausência da variante ou um frame inválido rejeita o lote e preserva o desenho padrão. A chamada SpriteBatch.Draw compartilha a mesma sobrecarga de nove parâmetros, incluindo SpriteEffects. O teste nativo valida limites e o protocolo; o smoke C# valida a ordem e a serialização de camadas. A iluminação dinâmica, DrawEffects, SpecialDraw e EmitParticles continuam em desenvolvimento. Não declarar fidelidade visual de glow/flame nem 60 FPS sem execução no Android; APK/.bmod só após fechar os cinco loaders.

### TileLoader C# — DrawEffects e SpecialDraw na fila com duas fases (revisão 32)

O SDK recebe `DrawEffects(x,y,tileFrameX,tileFrameY,BunnyTileDrawContext)` e `SpecialDraw(x,y,BunnyTileDrawContext)`, com máscara nos bits 17 e 18. A chamada `RequestSpecialDraw()` do contexto reproduz o pedido de `AddSpecialLegacyPoint` usado pela luminária do ExampleModBunny, sem expor o SpriteBatch IL2CPP à thread MonoVM. Os comandos de SpecialDraw são marcados como fase 1 no wire e só são gerados se DrawEffects os solicitar; todos os comandos normais são renderizados primeiro e os especiais depois, sem alocações dinâmicas na fila nativa, até 8 por célula e 32 por frame.

O protocolo `SPRITES` permanece compatível com 10/11/12 campos antigos. A 13ª coluna opcional de fase aceita apenas 0 ou 1, sendo 1 emitida pela API somente durante SpecialDraw. O hook executa PreDraw → DrawEffects → PostDraw → SpecialDraw quando solicitado, e ignora essas operações em `special=1`. Os testes de SDK verificam solicitações, ausência de solicitação e passada especial; os testes C++ cobrem máscara, serialização e fase inválida. A paridade com `TileDrawing.AddSpecialPoint` com kind 5 e os dusts reais de EmitParticles continuam pendentes. A validação real da renderização Android também continua pendente; sem APK/.bmod até concluir cinco loaders.

### TileLoader C# — EmitParticles fase 1: protocolo e limite por frame

Foi implementado `BunnyTile.EmitParticles(x,y,tileFrameX,tileFrameY,BunnyTileDrawContext)` no mesmo callback nativo `DRAW` já existente. `BunnyTileDrawContext.EmitDust` cria somente comandos de partículas, nunca objetos IL2CPP na thread MonoVM. A resposta aceita a seção `PARTICLES|N` depois da seção opcional SPRITES, sem alterar o formato das respostas anteriores. O comando registra o tipo real do tile, coordenadas, tipo de Dust, offsets locais, tamanho, velocidades em milésimos, alpha, escala em milésimos e ARGB. O C# limita duas partículas por célula, a fila nativa limita 24 por quadro, rejeita frames antigos e evita alocações por quadro na fila.

A referência é `Dust.NewDust(Vector2,int,int,int,float,float,int,Color,float)` usada por `ExampleCampfire.js` e `ExampleLamp.js`. Estes arquivos também modificam depois propriedades como `noGravity`, `velocity` e `fadeIn`; a fidelidade dessas alterações fica para lote posterior. Esta primeira fase valida a API C# e seu protocolo com testes Roslyn/C++; a criação real na thread de desenho e o binding IL2CPP serão conectados após a validação do protocolo. Sem APK/.bmod até testar no dispositivo e finalizar os cinco loaders.

### TileLoader C# — EmitParticles fase 2: Dust nativo na passada pós-tiles

`NativeTileSpritePass` vincula agora por assinatura exata `Terraria.Dust.NewDust(Vector2,int,int,int,float,float,int,Color,float)`, confere retorno Int32, parâmetros, ponteiro do método e layout nativo de Color antes de aceitar comandos. No hook `PostDrawTiles`, sempre depois do desenho original e apenas na passada normal/fora do menu, drena a fila de 24 partículas do `GameUpdateCount` corrente e invoca `NewDust` na thread do jogo. Partículas não precisam abrir `SpriteBatch`; só os sprites abrem lote. Não há alocação ou varredura de mundo na fila. Sem binding nativo, os comandos de partículas são descartados sem suprimir os sprites nem o desenho padrão e ocorre aviso diagnóstico controlado.

O `applyManagedTileDraw` valida a seção opcional de partículas, ownership de tipo/célula e a passagem normal, mantendo compatibilidade com payload DRAW e SPRITES anteriores. O teste nativo exercita os limites, ownership, frames e erros de protocolo. As personalizações de propriedades do Dust após `NewDust` (velocidade pós-spawn, `fadeIn`, `noGravity`) permanecem pendentes para fidelidade máxima. O retorno do SpriteBatch real, a aparência das partículas e o desempenho no Android ainda requerem teste no dispositivo; não liberar APK ou .bmod antes da conclusão dos cinco loaders.

### TileLoader C# — Dust pós-spawn com estilo (revisão 34, protocolo)

`EmitDustStyled` adiciona opções explícitas sem alterar a assinatura binária de `EmitDust`: `noGravity`, `fadeInMilli`, fatores pós-spawn de velocidade em milésimos, soma Y de velocidade, deslocamento X e incremento de alpha. Esses valores reproduzem propriedades manipuladas pelo ExampleCampfire e ExampleLamp depois de `NewDust`. O wire `PARTICLES` aceita sete inteiros opcionais após o ARGB, com validação estrita e rejeição de formatos incompletos, fora dos limites ou com campos extras. Sem opções novas, o wire antigo é idêntico. Nesta fase o transporte C# e os testes do protocolo estão prontos; a alteração dos campos IL2CPP ocorrerá em etapa própria, sempre com layout validado.

### TileLoader C# — Dust pós-spawn nativo, com validação de layout

A chamada `Dust.NewDust` segue na thread de desenho original, mantendo as 24 partículas por quadro. Antes de permitir `EmitDustStyled`, o binding verifica que `Main.dust` é o array estático de `Terraria.Dust`, que os objetos são classes com tamanho controlado e que `position`, `velocity`, `fadeIn`, `noGravity` e `alpha` possuem exatamente os tipos, larguras e offsets IL2CPP esperados. A escrita acontece somente após `NewDust` devolver um índice real menor que 6000; índices sentinela e falhas de ABI não acessam o objeto. A velocidade original produzida pelo jogo é multiplicada pelos fatores do wire e ajustada no Y, com rejeição de valores não finitos. A posição X pode ser deslocada, o alpha é saturado em 255 e o fadeIn só é alterado quando pedido.

Se o layout IL2CPP não puder ser validado, a fila rejeita efeitos estilizados e mantém `EmitDust` sem personalização; nada disso suprime os sprites ou o desenho padrão. Os efeitos aleatórios dos exemplos dependem de o mod C# escolher quando emitir e quais valores usar; não há varreduras de mundo nem novos callbacks no hot path. A validação CI não comprova o layout real nem os 60 FPS no Android.

### TileLoader C# — shapes 3×2 / 1×3 (revisão 36, aguardando aparelho)

O enumerador `BunnyTileShape` aceita agora `Single`, `Style2x1`, `Style3x2` e `Style1x3`. O compilador C#, o parser de fonte, os defaults nativos e o runtime de gameplay validam valores 0–3. O registro nativo de 3×2 copia os dados vanilla de `Terraria.ObjectData.TileObjectData.GetTileData(TileID.Campfire=215,style=0,alternate=0)`, com `StyleLineSkip=9`; 1×3 copia `TileObjectData.Style1xX`, usa `StyleLineSkip=2`, `DrawYOffset=2` e espelhamento horizontal. São chamadas nativas com checagens de assinaturas e exceções; caso falhem, o registro da forma é rejeitado.

A limpeza nativa de suportes de móveis usa footprint de até 3×3, frames de 18 pixels, reconhece variantes ligadas/desligadas via `frameY` e `frameX`, identifica peças de cada objeto pelas coordenadas e quadro, remove peças existentes com `noItem` e aciona uma única passagem de drop composto. A implementação preserva o caminho anterior 2×1, mas exige validação real no Terraria Android da ABI, da criação, quebra, persistência, drop único e frames antes de qualquer release. As interações com fios e estados originais da fogueira/luminária ainda não estão portadas.

### TileLoader C# — declaração de alternância (revisão 37)

A propriedade `BunnyTile.FrameToggle` recebe `None`, `CampfireVertical` (somente forma 3×2) ou `LampHorizontal` (somente 1×3). O registro `TILE` ganha um 22º campo. Pacotes com 13/14/16/17/19/20/21 campos seguem válidos e usam `None`; compilador e nativo rejeitam combinações de modo/forma inválidas. A etapa de integração dos setters de frames nativos permanece separada e sujeita a ABI validada.

### TileLoader C# — iluminação ligada/desligada e regressão de máscaras

O runtime aplica a política nativa de iluminação do frame antes de pedir `ModifyLight`, e remove entradas antigas da cache quando o móvel é desligado. Fogueira em frames Y 0/18 emite; Y 36/54 não. Luminária com frameX=0 emite; frameX=18 não. A alternância limpa a cache inteira para não manter luz antiga. O teste C++ valida estados e frames; o pacote C# compilado tem teste que exige shape e `FrameToggle` reais, com callbacks gráficos acima de 16 bits.

## TileLoader C# — RegisterItemDrop por estilo (20/10)

A API `BunnyTile.RegisterItemDrop(string itemName, params int[] styles)` e o overload de item numérico replicam a tabela de estilo do `ModTile.js` do ExampleModBunny: sem estilo, regra geral; com estilos, override por estilo; duplicatas usam a última declaração. O C# serializa o mapa ordenado em campo 23 do `TILE` (22 e anteriores seguem aceitos); a ponte nativa valida nomes, IDs, cardinalidade e duplicatas antes de registrar. Itens próprios são resolvidos por `mod/item`, sem QuickJS. `GetItemDrops` gerenciado mantém prioridade e o fallback de item colocável continua quando não há regra correspondente.

Em tiles com `TileObjectData`, a consulta de estilo usa a assinatura exata `GetTileStyle(Terraria.Tile)`, validada por IL2CPP apenas na instalação de regras por estilo; em tiles simples, o estilo padrão é 0, como no loader de origem. ABI ausente ou item não registrado rejeita a vinculação em vez de inventar ID/estilo. Há testes host para regras válidas e inválidas, sobreposições e serialization no smoke Roslyn.

**Não certificado completo:** falta a validação do drop efetivo no Terraria Android, diferentes estilos de móveis e portas, multiplayer, save/reload e a bateria integral de TileLoader. Manter PR draft e sem distribuição APK/.bmod; Wall/Tree não liberados.
