# A1 — bancada C# nativa: primeiro checkpoint de infraestrutura

Base preservada: Android .90.29.8 e ExampleMod C# 0.1.36.
Branch de desenvolvimento: codex/lot-a-complete-v90-29.
Este checkpoint nao gera APK e nao declara o ExampleWorkbench portado.

## API C# e compilacao

BunnyTile passa a fazer parte de BunnySourceSdk. SetStaticDefaults define Flags,
Shape (Single ou Style2x1), CoordinateHeight, AdjacentTile, DustName, entrada
de mapa e NumDust. O parser reconhece SetStaticDefaults em BunnyTile,
o compilador executa esse metodo com isolamento e gera a linha TILE de 13
campos, mantendo os wires ITEM anteriores e a versao V2.

A identidade nativa e CATEGORY:Name: ITEM:ExampleWorkbench e
TILE:ExampleWorkbench sao distintos, embora o identificador publico
dos itens e tiles no registro original continue sendo UID/Name.

O host reserva BunnyTile no registrador original de ModTiles, nao no de
itens. O callback nativo de tiles agora e separado do callback JS, com
a mesma janela de destravamento de TileObjectData e restaura o cache de
forma apos aplicar dados C#. Mudancas pendentes depois do setup sao
processadas apenas quando necessario.

## Fixture original sem port prematuro

tests/basic_items/tile_fixture contem dois exemplos C# com namespaces
distintos e nome ExampleWorkbench, reproduzindo as propriedades declaradas
no ExampleModBunny: TileObjectData.Style2x1, altura 18, flags de mesa,
plataforma e destruicao, DustName Platinum, bancada vanilla adjacente,
mapa RGB 200/200/200, NumDust 1/3 e item 28x14, valor 150, receita
WorkBench + 10 ExampleItem, sem estacao.

Os dois fontes sao verificados por Roslyn e CompiledAssemblyProbe. Esse
fixture NAO esta no ExampleMod CSharp e NAO alterou sua quantidade de
fontes, assets ou versao. A fonte JS original nao foi modificada.

## Gaps para finalizar A1

A bridge atual aplica flags estaticas, copia TileObjectData.Style2x1,
configura CoordinateHeights, registra addTile e cor/mapa. O segundo incremento
inclui entrada nativa em RoomNeeds.CountsAsTableTypes e associação da estacao
com o item colocavel em CraftingStationItemId, se a resolucao do item estiver
pronta. Um hook nativo adicional de Player.AdjTiles verifica somente os
metatile C# com adjacencia declarada, num retangulo pequeno proximo ao jogador;
se uma bancada de mod estiver proxima, marca o proprio tile e WorkBenches como
adjacentes. O hook ainda precisa de validacao no dispositivo.

Faltam dust/drop de objetos multitile, fallback de localizacao do nome no mapa
e os testes de ABI
e Android para colocacao, remocao, save/reload e 60 FPS.
Nao inferir que a aprovacao das suites comprova esses efeitos.

## A1 — lote de destruicao e receitas originais

O teste de compilacao Roslyn inclui a bancada 2x1, seu item colocavel e tres
pecas de armadura com os mesmos nomes, ingredientes, estacao e bonus dos
arquivos JS originais do ExampleModBunny: capacete +5 defesa, bonus completo
de 20% de dano melee/ranged/magic/minion; peitoral +6 defesa, +20 mana,
+1 minion e imunidade OnFire; calca +5 defesa e +5% movement.
O teste exige os tres itens e a recipe station ExampleWorkbench, alem da
identidade separada TILE:ExampleWorkbench / ITEM:ExampleWorkbench.

A implementacao de tile permanece native-first: NativeTileGameplay usa
os ganchos do WorldGen para drop unico de moveis grandes, verificacao
de integridade 2x1 e limpeza da metade sobrevivente. A poeira usa
KillTile_MakeTileDust e limite fail=1/normal=3, com DustID por nome.
Os hooks verificam o tamanho real de Terraria.Tile antes de instalar;
se falhar, nao tentam usar a ABI desconhecida. Sem callbacks Mono/JS
na renderizacao. O algoritmo de geometria e limites e coberto por um teste
C++ separado.

A implementacao C# do conjunto passa pelo contrato de comportamento
BunnyItemBehavior ja existente; ainda depende da ponte de comportamento
para a aplicacao em jogo. Falta validar a ponte e adicionar os assets
originais ao ExampleModCSharp antes de habilitar as classes na amostra
distribuida. Todos os hooks nativos de gameplay permanecem NAO
VALIDADOS no Android ate teste de colocacao, quebra, relog,
destruicao de suporte, multiplayer e desempenho.

## Endurecimento de seguranca e desempenho

Os registros de adjacencia usam uma tabela limitada e montada durante a
inicializacao. Player.AdjTiles apenas percorre os registros e uma copia
local dos TileArrays, sem alocar vetor, adquirir mutex, chamar Mono ou JS
por tick. Os offsets de Player/TileData sao conferidos antes do hook.
O DustID por nome aceita constantes IL2CPP de 16/32 bits, busca os bytes
reais do campo na API nativa e cai no comportamento do jogo se nao puder
identificar o identificador; nao inventa um DustID de outro Terraria.
A limpeza dos frames 2x1 usa runtime_invoke com captura de excecao nas
rotinas do WorldGen e preserva o restante das categorias de tile.

Mesmo com CI aprovado, e necessario validar a ABI de Tile e o comportamento
real de drops e frames no Android antes de distribuir a versao nova.

## Android A1 .90.29.10 — corrigido o bloqueio de carregamento

A APK .90.29.9 rejeitou o mod A1 por divergencia na contagem do contrato
de compilacao MonoVM. O probe inclui seis classes (5 BunnyItem e
1 BunnyTile), mas expectedManagedSourceTypes esperava apenas cinco,
ignorando mod.tiles. O status "expected 5" era rejeitado ANTES de
aplicar os defaults: armaduras, receitas e bancada nao eram registradas.

A rotina expectedManagedSourceTypes agora inclui mod.tiles. O teste
managed_prefix_auto_count_v90_1 verifica A1 5+1=6, preserva os mods
anteriores e tambem verifica mods contendo apenas BunnyTile. O fixture
C# original foi ampliado com ExampleItem e agora verifica 6 classes,
alinhado ao pacote A1 que foi enviado. Erros futuros mostram tambem
quantos tipos o worker executou, quando o status fornecer o numero.

A correcao preserva MonoVM, assinatura v3 e warm-start; nao altera
algoritmos de renderizacao ou os efeitos existentes. Ainda e preciso
validar dentro do Terraria Android a quebra de tiles, drops, poeira,
instalacao de receitas, armaduras e FPS.
