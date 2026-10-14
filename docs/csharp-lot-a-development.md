# Lote A — desenvolvimento integral, sem entrega parcial

Branch: codex/lot-a-complete-v90-29, derivada do commit
28a9d71dac28a4ce14bff16fb492707c55227d05 da branch validada.
O usuario confirmou mascara/robe e carregamento na .90.28.7. Essa versao
permanece intacta. O ExampleMod 0.1.36 atualiza documentacao/matriz; o lote continua aberto.

O escopo obrigatorio possui 33 arquivos, todos listados em
Split/Bunny-Loader/tools/example_completion/lot-a-scope.json. A matriz global
agora tem 201 arquivos (os 199 anteriores mais dois touch buttons). Dependencias em lotes futuros precisam de bridges
reais; nao justificar simplificacoes dos exemplos.

## Infraestrutura presente nesta branch

BunnyItem.Defense recebe 0..1000 e e persistido no SetDefaults nativo com
metadata integral de 8/16/32 bits, offset validado e escrita da largura real.
O registrador informa mod, classe, fonte, campo, tipo encontrado, valor e causa.

Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2,20), Add(MaxMinions,1),
ImmuneTo("OnFire") e Add(BunnyArmorStat.MoveSpeed,0.05f) usam UpdateEquip
do ItemLoader original. Os alvos sao verificados antes de aplicar os efeitos.
Imunidades sao resolvidas uma vez por BuffID; nao buscar classes por quadro.
O Terraria faz seu reset habitual: o host nao retém bonus por jogador.

BunnyArmorSet declara as tres chaves de conteudo e o texto do conjunto; sua
Effects utiliza a mesma API. DefineArmorSet insere o conjunto em ArmorSetBonuses
original, com lookup/tooltip nativos e callback de bonus apenas no conjunto
completo. Chaves locais recebem UID do mod; chaves UID/Nome sao resolvidas
pelo registro nativo, sem nomes especiais do ExampleMod.

Nenhum arquivo ExampleHelmet/Plate/Leggings foi anunciado como concluido: os
contratos sao exercitados por um mod independente de testes (ContractHelmet,
ContractPlate, ContractLegs), que usa compilacao Roslyn e wire reais no CI.
O port final ainda exige receitas na bancada de mod, assets e outros hooks.

## Testes e continuidade

ItemBatch testa a API publica, rejeicoes e compilacao dos fontes de contrato.
armor_behavior_bridge_test.js carrega ModItem, ItemLoader, ArmorSetLoader e
CSharpArmorPolicy reais com doubles de Terraria. Verifica defesa, GrantArmorBenefits,
imunidade, mana/minions, velocidade, bonus de 20%, conjunto incompleto, vanilla,
reset/remoção e falha antes de aplicar efeitos. native_item_binding_test.cpp
verifica defesa em nova instancia, sentinelas e rejeicao de valor/tipo incorreto.
Nenhum desses testes comprova gameplay/FPS no Android.

CI compila um APK de desenvolvimento, mas o passo de publicacao no Builds
privado exclui esta branch. Nao publicar APK ou bmod novo como entrega do Lote A
antes de implementar os 33 arquivos e registrar todas as dependencias.

Proxima dependencia: contrato C# de tile/estacao e receitas com chave de tile,
para ExampleWorkbench. Depois portar integralmente o conjunto e Costume;
seguir o escopo completo para wings/boots/shield, consumiveis/pickups, tools,
raridades/prefixos, tooltips/currency/quests e dependencias de combate/pesca.

## Receitas com estacao de mod

BunnyRecipe.AddTile(string) aceita classe local ou UID/Classe de outro mod.
O wire preserva os 20/21 campos anteriores e amplia somente o token da estacao
para mChave. O parser valida a chave, o nativo qualifica nomes locais pelo UID,
e o RecipeLoader original resolve o tile pela instancia registrada no host.
Estacao ausente recusa a receita antes de registrar, com ID da receita/chave,
etapa, expected/actual e estado Pending. Uma tentativa posterior pode concluir
o registro quando a estacao existir. IDs vanilla continuam limitados a 0..2194.

Os testes cobrem SDK/wire reais, parser, qualificacao nativa, estacao de outro
mod JS, ausencia/registro tardio, lookups e ACK sem duplicar receitas. A API
ja funciona com tiles JS carregados. O contrato C# de Tile/ExampleWorkbench
continua pendente: nao marcar receitas dos exemplos como concluidas por isso.
A mudanca de tokens de estacao invalida caches anteriores; contratos vigentes registrados abaixo.

## Wings e movimento nativo

BunnyEquipKind.Wings registra a textura no EquipLoader original e persiste
wingSlot com sua largura nativa. SetWingStats configura tempo, velocidade e
aceleracao de voo, alem dos cinco refs de VerticalWingSpeeds. WingLoader original
aplica o movimento, gravidade invertida e limite de subida, com filtro nativo
apenas nos slots de mod. Nao existe passagem Mono por quadro.

Teste wing_behavior_bridge_test.js usa o WingLoader real e wire Roslyn de um
mod independente ContractWings, verifica todos os refs, caps, gravidade invertida,
tempo de voo, filtro nativo, caso especial da asa vanilla e recriacao do item.
Ainda faltam port integral do ExampleWings, sprite real, receita na bancada C#
e validacao no Android. Nao declarar o exemplo concluido.

Contratos atuais: cache Roslyn v14 e nativo armor_station_wings.
Primeira infraestrutura de armadura passou em oito suites e build Android
`38569808364` no commit eefe0c304ee5272782c2df1c543193166bc755fe;
publicacao privada foi excluida por esta ser uma branch de lote incompleto.

## Isolamento e diagnostico de registros pendentes

Uma receita com estacao ausente nao interrompe as outras do mesmo lote. Erros
de registro/lookups/ACK incluem mod, classe, ID, arquivo, etapa e excecao/stack,
e ficam visiveis por registro em bl.csharp.pendingItemRecipes(). Sucesso so
recebe ACK apos recriar lookups. Registros ja feitos nao se duplicam no retry.

As filas de comportamento e receitas continuam limitadas a oito entradas por
consulta, mas agora usam round-robin: oito registros com erro nao impedem o
nono de ser processado. Teste nativo cria nove registros sem ACK e verifica
rotacao de ambas as filas e persistencia do diagnostico/source. Sem aumentar
trabalho por frame ou retirar limites seguros.

Armadura+estacoes+asas passaram nas oito suites e build Android do run
38571726472, commit a3242340700dd16bba4b2e528907892130cc5ee3.
Alteracoes de isolamento posteriores precisam de nova regressao.

## Proximo contrato: bancada, sem perda de comportamento

Referencia original: Content/Tiles/Furniture/ExampleWorkbench.js e
Content/Items/Placeable/Furniture/ExampleWorkbench.js. O tile exige todas as
flags tileTable/SolidTop/NoAttach/LavaDeath/FrameImportant, DisableSmartCursor,
HasOutlines, IgnoredByNpcStepUp, DustType Platinum, AdjTiles WorkBenches,
TileObjectData.Style2x1 com CoordinateHeights [18], RoomNeeds.CountsAsTable,
mapa RGB 200/200/200 e NumDust(fail?1:3). O item exige DefaultToPlaceableTile,
tamanho 28x14, valor 150 e receita WorkBench + ExampleItem x10 sem estacao.
Nao simplificar bancada para tile de 1x1 nem trocar a receita por estacao vanilla.

O original tem a mesma classe ExampleWorkbench em dois tipos de conteudo.
Antes de adicionar ambos em C#, revisar identidade (categoria/nome nativo versus
classe CLR/namespace) no SourceParser, SourceLoader, CompiledAssemblyProbe e
serializacao. Preservar identificadores persistidos e diagnosticos da fonte
correta; nao permitir que colisao sobrescreva o mapa de tipos/arquivos.
Essa lacuna e do SDK, nao evidencia de impossibilidade tecnica no Terraria.

## Checkpoint final desta execucao

Runtime commit e466ae4c4fc1097b67ad0d0c6460a36e4fb66b8b. Auditoria/SDK/bridges
38573406958 e sete outras suites aprovadas; pipeline Android 38573406719
compilou e assinou com sucesso. O passo de publicacao privada ficou skipped.
Nenhum APK de lote parcial foi publicado, e .90.28.7 segue como base validada.

Estado: infraestrutura nova consistente; ExampleMod segue com seus 51 fontes
e a versao 0.1.35. Os 33 ports do Lote A ainda nao estao completos. Retomar
pelo contrato de tile/identidade descrito acima e concluir todos os arquivos
do escopo antes de gerar a entrega final. Nao reinterpretar os fixtures de
teste como exemplos finais nem os resultados de CI como gameplay Android.

## Continuidade .90.29.1 — servico nativo de receitas

O novo `content/recipes/NativeRecipes` substitui a passagem das receitas C#
pelo RecipeLoader JS. Os registros existentes de itens/tiles foram separados
dos instaladores graficos, conservando suas estruturas, ordem e chaves.
Ingredientes e estacoes intermod resolvem tarde, sem rejeitar todos os defaults
do mod quando uma dependencia ainda nao carregou. Contratos IL2CPP incluem
bounds contra tamanho real da instancia, transacao de Recipe.currentRecipe,
rollback e ACK somente depois dos lookups. Cache do compilador/host invalidado.

Testes executaveis sem QuickJS usam registradores e adapter de producao com
host IL2CPP controlado. Ver [auditoria de independencia](csharp-independence-audit.md)
para escopo e limitacoes. Equipment policies ainda passam pelo JS; a bancada
C# ainda precisa da bridge completa de colocacao, suporte, drops e demais
comportamentos ja descritos. Nenhum dos 33 exemplos foi promovido nesta etapa.

Proximo grupo exato: API publica de tile/estacao e item colocavel, identidade
por categoria, TileObjectData.Style2x1/CoordinateHeights [18], housing/map/dust,
frames/suporte/drop nativos, adjacencia e CraftingStationItemId, port dos dois
ExampleWorkbench e receitas originais dos tres itens de armadura. Nao usar
2195 como TileID.Count: o core usa 754; 2195 pertence a paleta de mapa.

### Resultado verificavel do runtime .90.29.1

Commit 051766a6c0a34b9b27def096ff891f5667933820: oito suites aprovadas e
Android build/assinatura aprovados no run 38596246405. Artefato experimental
privado disponibilizado pelo workflow, com APK .90.29.1 e .bmod 0.1.35; nao e
release oficial do Lote A. Nenhuma validacao Android/FPS nova registrada.
As alteracoes posteriores a esse SHA sao regressao adicional dos registradores
e documentacao; devem manter o mesmo runtime e nenhum novo port e declarado.

Atualizacao posterior: original e608114efb15a3a021887a6132d9e2a487ba6b62,
matriz 201 e escopo A 33. ExampleMod 0.1.36 atualiza documentacao das recipes,
matriz embutida e catalogo; mesmos 51 fontes e 26 PNG, nenhum port novo.

### Continuidade .90.29.2

Removida dependencia indireta de NativeBasicItems para o helper de structs da
bridge JS. ValueLayout nativo valida a convencao de offsets e referencias
aninhadas; defaults de Item, defense e slots conferem limites da instancia.
Testes sem QuickJS agora ligam essa implementacao real em vez de um stub do
helper JS. Nova versao .90.29.2/code167 e nova geracao de cache; ExampleMod
permanece 0.1.36, pois seus exemplos/documentacao de receitas nao mudaram.
Retomar bancada pelo contrato acima; nenhum dos 33 exemplos foi promovido.

### Continuidade .90.29.3

A fila real de recipes foi executada no host sem QuickJS, junto aos registradores
reais e NativeBasicItems, incluindo preview, estacao tardia e nao duplicacao.
Foi preservada a ordem policy → receita e adicionado readiness por item: recipes
com policy ainda pendente aguardam, sem bloquear as de outros itens. Code168 e
nova geracao de cache. Bancada continua sendo a proxima implementacao concreta;
nenhum exemplo foi promovido. ExampleMod mantem 0.1.36/51 fontes/26 PNG.
