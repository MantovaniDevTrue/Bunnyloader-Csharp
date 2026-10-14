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
configura CoordinateHeights, registra addTile e cor/mapa. Ainda sao
necessarios os hooks nativos de adjacencia (Player.AdjTiles),
RoomNeeds.CountsAsTableTypes, dust/drop de objetos multitile e
CraftingStationItemId para Guia, com verificacoes de ABI e testes Android
para colocacao, remocao, save/reload e 60 FPS.
Nao inferir que a aprovacao das suites comprova esses efeitos.
