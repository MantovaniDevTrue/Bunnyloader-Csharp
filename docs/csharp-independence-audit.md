## Continuidade de 22/10/2026 — núcleo compartilhado de fundos

| Sistema | Classificação | Evidência e limite |
| --- | --- | --- |
| BunnyBackgroundTexture / BunnyBackgroundTextures | API C# usando serviço nativo independente compartilhado | Reserva/autoload/publicação/lookup por UID e leases. Teste C# por P/Invoke liga o serviço de produção sem QuickJS. |
| BunnyUndergroundBackgroundStyle / BunnyUndergroundBackgrounds | API C# original com bridge própria e núcleo independente compartilhado | MonoVM → ManagedUndergroundBackgrounds → content/backgrounds/UndergroundBackgrounds → picker/getters Terraria. JS utiliza outra bridge para esse núcleo, sem intermediar chamadas C#. |
| Compatibilidade entre as duas linguagens | Serviço nativo compartilhado | Mixed.HostTests executa o exemplo C# e o original JS com QuickJS real no mesmo processo, incluindo transição, prioridade e as duas ordens de unload. É host CLR com metadata controlada, não Android. |
| BunnySurfaceBackgroundStyle / BunnySurfaceBackgrounds | API C# original com bridge própria e serviço nativo independente compartilhado | MonoVM → estágio 13/ManagedSurfaceBackgrounds → SurfaceBackgrounds; JS tem provider separado para o mesmo núcleo. Exemplo C# e JS original executam juntos no host com QuickJS real, worker gerenciado, P/Invoke e mailbox; ainda sem Android. Ver csharp-surface-backgrounds.md. |
| Demais módulos/estilos/biomas | Auditoria ou implementação ainda pendente conforme matriz | Não inferir independência do SDK inteiro ou conclusão dos quinze a partir desses testes. |

Código, limitações e QA em [csharp-underground-backgrounds.md](csharp-underground-backgrounds.md).
CI `23101aad` e `8c1512e2`: quatro jobs source-only aprovados, incluindo linkagem
ARM64; nenhum APK. Mudanças posteriores exigem nova aprovação. As seções abaixo
preservam a auditoria histórica, inclusive dependências ainda não certificadas.

> Continuidade .98 WIP: BunnyShootSource/Context/Shot → worker MonoVM e NativeShooting → fábrica Terraria diretamente. Fonte/dono/modifier ficam na thread do jogo; resolução de referências usa o registro nativo compartilhado. Nenhuma invocação QuickJS. Testes executam o C# e os hooks/contratos C++ de produção com providers; gameplay Android pendente. Nenhum loader inteiro certificado.

> Port integral em desenvolvimento: CustomUseSound usa NativeUseSounds → NativeSoundRouter → AndroidAudio/SfxQueue, sem QuickJS. SDK e binding próprios; serviço de áudio independente compartilhado com JS. Testes de host não certificam áudio Android. Publicação suspensa.

> .96: NativeDefaults e NativeHoldout usam serviços C++ próprios com metadata IL2CPP validada; constantes e referências vêm do registro nativo compartilhado. BunnyName alimenta nomes dos conteúdos no núcleo. Nenhuma chamada QuickJS nesses caminhos. Fixtures BL_HAVE_QUICKJS=0; isto não certifica o desacoplamento dos88 loaders legados.

> .95.2: removido contrato incorreto de identity constatado no Android .95.1. A geração é rastreada no hook nativo SetDefaults compartilhado, sem QuickJS. Binding e reutilização testados com providers controlados; Android da correção pendente.

## Continuidade .95 — ciclo de vida e movimento de summons

`BunnyProjectile.MinionMovement`, buff/dono/geração própria por SetDefaults e ApplyStatsFromSource seguem C# → descritores/MonoVM → núcleo C++ → Terraria, sem executar QuickJS. Tests/summon_lifecycle executa os hooks de produção com providers IL2CPP controlados e BL_HAVE_QUICKJS=0; não comprova a ABI real no Android. C# custom AI continua alternativa explícita ao controlador nativo. Nenhuma independência global nova é inferida para os módulos legados. Ver [csharp-summon-v95.md](csharp-summon-v95.md).

# Independencia C# — auditoria e checkpoint .90.29.1

## Continuidade .93 — política própria de projéteis

Base .90.5/PR94 integrada com .92/PR93 sem reverter commits. A seleção de alvo,
a animação e a aplicação de velocidade usam NativeContent/ManagedProjectileHook
→ serviços de conteúdo/Terraria; AI executa no worker MonoVM. Esta expansão não
executa JS para o novo percurso. O registro compartilhado de quadros é atualizado
na aplicação do lote, conservando contagem/medidas nas reconstruções de tabelas.

Políticas públicas AnimationFrameTicks/MinionScanTicks/MinionTargetRange são
validadas pelo compilador e decoder, cacheadas por instância e aplicadas nativamente.
BunnyMinionMovement é um algoritmo C# reutilizável, não uma ponte simulada.
Os contratos dos campos novos e da tabela de frames são verificados antes de uso;
o scanner valida NPC.active/friendly/life, dontTakeDamage quando presente, e o
rank/tipo da tabela NPC. Os testes usam metadata controlada no host e não comprovam
execução Android. Nenhuma certificação nova de independência dos equipamentos ou
de todos os demais módulos é inferida disso. Seguem as lacunas de fidelidade do
minion e dos demais loaders em [csharp-loaders-v93.md](csharp-loaders-v93.md).


## Continuidade .90.29.92 — loaders próprios

As seções abaixo são o histórico da auditoria, não o estado atual da bancada.
`BunnyTile`, `NativeTileGameplay` e a bancada C# já existem na base preservada;
esta execução não os reimplementa. Ver `csharp-loaders-v92.md` para o checkpoint.

| Sistema revisado nesta execução | Classificação atual | Evidência e limite |
| --- | --- | --- |
| Receitas de BunnyItem, BunnyWeapon, BunnyAccessory, BunnyBuffPotion | API C# original usando serviço nativo independente compartilhado | BunnyRecipeSource → compilador/SourceLoader → enqueueOwnedSourceRecipes → NativeRecipes. Sem importação/invocação de QuickJS nesse percurso. |
| Callbacks de receita | API C# original com bridge nativa própria | BunnyRecipeLoader reconstrói handlers no worker MonoVM; BunnyRecipeRuntime mantém o endpoint compatível. Hooks de crafting usam RecipeCallbackProvider, metadados validados e comandos limitados. |
| Dependências entre categorias e mods | Serviço nativo independente compartilhado | ItemRegistry/TileRegistry, chaves mod/conteúdo. O teste de produção adia estações ausentes sem impedir a receita independente; não executa mods JS. Teste misto no Android continua necessário. |
| Equipamentos/policies legadas e demais módulos | Desacoplamento ainda não certificado por esta execução | Não inferir independência do SDK inteiro a partir do teste de receitas. Preservar sistemas existentes, revisar dependência por dependência. |

O teste novo compila realmente três donos em um mod independente. O teste C++
consome esse arquivo e executa fila, resolução, Recipe e ACK de produção com
QuickJS desabilitado, metadados IL2CPP e provider de callbacks controlados.
Os callbacks gerenciados são executados separadamente no host .NET8. Nenhum
resultado é anunciado como execução em Terraria Android. Não há dependência de
assemblies/runtime tModLoader nesta expansão; equivalência integral de APIs não
é afirmada. As 88 entradas do inventário de loaders continuam contabilizadas.


Branch: `codex/lot-a-complete-v90-29`. Base preservada: .90.28.7;
referencia Android anterior .90.28.4. Nao e entrega completa do Lote A.

| Sistema | Classificacao | Caminho real / pendencia |
| --- | --- | --- |
| Defaults basicos de BunnyItem | API C# original com bridge propria | MonoVM → SourceLoader → NativeBasicItems; campos tipados. O refresh de instancias agora tambem compila sem QuickJS. |
| Reserva e identificacao de itens | Servico nativo independente compartilhado | ItemRegistry.cpp extraido do core, mesma ordem, chaves e reserva de itens ausentes; JS e C# continuam usando registerModItem. |
| Reserva e identificacao de tiles | Servico nativo independente compartilhado | TileRegistry.cpp extraido do core, mesma ordem e chaves; instalacao/draw/save permanecem em ModTiles/ModTileSave. Reserva de ID nao comprova tile funcional. |
| Receitas BunnyItem | Dependencia corrigida | NativeBasicItems → content/recipes/NativeRecipes → runtime_invoke de Recipe/Item. Nenhuma chamada a __blSyncCSharpRecipes. |
| Dependencias de receitas | Servico nativo independente compartilhado | modTileTypeByKey/modItemTypeByKey; UID/nome, mesma identidade para JS/C#. Resolver espera tile instalado e tenta novamente ingredientes tardios. |
| SyncCSharp legado no JS | Caminho de compatibilidade desativado no runtime novo | nativeRecipes=true impede registro duplicado. Nao e necessario para receitas novas. |
| Equipment/armor/wings policies de BunnyItem | Dependencia de QuickJS ainda existente | CSharpItemBehaviorLoader/ItemLoader/CSharpArmorPolicy/CSharpWingPolicy. O host sem QuickJS recusa estes descritores explicitamente. Nao declarar todo o SDK independente. |
| Bancada C# e itens colocaveis | Recurso pendente | Falta API gerenciada e configuracao nativa de TileObjectData, frames, suporte, drops, dust, mapa, housing, adjacencia e icone da estacao. |
| Limite ampliado de receitas | Recurso pendente de desacoplamento | Fora de Recipe.maxRecipes, o original utiliza hooks JS para menu/Guia/variantes. O servico novo recusa o indice excedente com NativeRecipeLimitReached; nao afirma que estes hooks estao portados. |
| Demais categorias | Auditoria pendente por sistema | APIs de combate/entidades possuem bridges proprias; esta auditoria nao presume independencia de todos os hooks e assets. |

## Contrato do servico de receitas

SDK publico permanece `BunnyItem.CreateRecipe`, `BunnyRecipe.AddIngredient`,
`AddTile` e `Register`; sem alteracao de assinaturas. `AddTile(int)` aceita
TileIDs vanilla 0..753. `2195` era indevidamente usado como limite: e indice da
paleta do mapa, nao TileID.Count. `AddTile(string)` usa nome local ou UID/nome,
resolvido no registro nativo, sem consultar TileLoader.ByType.

Antes do commit, o servico espera SetupRecipes e os itens/tiles instalados.
Dependencias e setup ainda pendentes geram WARN; falhas reais geram ERROR.

O descriptor e resolvido na thread de update, ate oito receitas por rodada,
com round-robin e intervalo de 120 updates. Dependencia ausente nao impede
aplicar defaults do item nem registrar receitas independentes. O diagnostico
inclui mod, classe, arquivo, ID, receita, estacao, etapa, esperado/encontrado e
causa; excecoes de invocacao preservam Exception.ToString quando disponivel.
Erros repetidos identicos nao sao logados novamente.

O adapter valida tipos de campos, static/instance, tamanho Int32, limites dos
offsets contra class_instance_size, assinatura/instance dos metodos, arrays e
layout do parametro ItemVariant. Nunca escreve stack em campo incompatível.
Recipe e criada separadamente; validacao de itens/pilhas precede o commit.
Falha no AddRecipe restaura slot, contador e currentRecipe. Sucesso preserva
currentRecipe de outro registrador. ACK aguarda seis builders de lookups;
retry de builder nao duplica a receita ja adicionada.

## Evidencia automatizada — limites claros

`tests/native_recipes/native_recipes_test.cpp` compila com BL_HAVE_QUICKJS=0 e
liga os **registradores reais de producao** e o **adapter IL2CPP de producao**,
sem QuickJS ou mock de RecipeLoader/ModRecipe. O host fornece metadata,
alocacoes e metodos Terraria simulados; portanto nao prova comportamento do
Terraria no celular. Executa reserva de IDs/chaves/duplicatas, tile ainda nao
instalado, estacoes locais e intermod, ingrediente carregado depois, receita
vanilla, commit/rollback, retry de lookups, isolamento, arrays errados,
alocacao recusada, pilha invalida, campo Single, offset fora do objeto,
assinatura ausente e limite de capacidade. Variantes usam processos separados
para exercer novamente a resolucao do contrato. ASan/UBSan no host aprovados;
alocacoes deliberadas do host nao modelam o GC do jogo.

`native_item_binding_test --recipes-only` liga NativeBasicItems de producao
sem QuickJS, aplica defaults reais do adapter a objetos do host e enfileira
receitas com ingredientes/estacoes ausentes. Nao e teste do boot MonoVM nem
registro de uma bancada colocavel. CI tambem mantem o teste JS legado e
verifica que nativeRecipes=true impede a passagem de receitas C# pelo JS.

## Android ainda obrigatorio

1. Abrir ExampleMod 0.1.36 e conferir recipes de ExampleItem/Pickaxe/Hamaxe.
2. Com ingredientes e bancada vanilla, verificar menu, Guia, material e craft.
3. Mod C# independente com AddTile("UID/EstacaoJS") e AddIngredient("UID/ItemJS"):
   verificar ausencia/presenca da dependencia, log individual e ordem inversa.
4. Save/Exit, reabrir e confirmar tipos/chaves e receitas sem duplicacao.
5. Preservar equipamentos, prefixos, asas e armaduras da infraestrutura anterior.
6. Registrar FPS, estabilidade e ciclo de vida. Nenhuma medicao Android nova feita.

A bancada C# e as tres armaduras foram integradas posteriormente no ExampleMod 0.1.37.
O caminho de tiles/receitas usa NativeTileGameplay/NativeRecipes e registros
nativos compartilhados. O teste do TileRegistry ou de colocacao vanilla nao
comprova colocacao/destruicao/persistencia da ExampleWorkbench no Android.
Na continuacao 0.1.38, ExampleWings usa a infraestrutura existente, mas ainda
depende de CSharpWingPolicy/WingLoader e do instalador JS de equipamentos.
O port e parcial frente ao requisito de independencia; esses componentes
precisam migrar para um servico nativo antes de declarar esse requisito cumprido.

## Entrega experimental autorizada

O workflow da branch permite uma prerelease privada identificada como
EXPERIMENTAL / Lote A incomplete. Antes de enviar, exige os oito workflows
independentes aprovados para o SHA exato, alem dos testes e build do proprio
job. Falha, cancelamento, SHA diferente ou suite ausente recusam a entrega.
A release oficial do lote completo continua pendente. O ExampleMod 0.1.36 atualiza documentacao/matriz e conserva
51 fontes: esta etapa modifica infraestrutura e documentacao, nao adiciona ports.

O teste adicional registry_regression_test executa os registradores reais com
oito threads disputando 192 chaves por categoria. Confere duplicatas, nomes
iguais em mods/categorias diferentes, lookup reverso e reserva dos 64 itens
nao carregados, que nao pode satisfazer dependencia de ingrediente. Nao e
teste de salvar/abrir um mundo no Terraria.

## .90.29.2 — dependencia indireta removida e bounds de itens

NativeBasicItems chamava `script::structFieldOffset`, cujo corpo em Value.cpp
so e compilado com BL_HAVE_QUICKJS. O antigo teste de binding fornecia esse
helper como stub; isso nao demonstrava o contrato real dos offsets nullable.
O stub foi removido. O C# agora utiliza `il2cpp/ValueLayout.h`, independente de
JS, que valida todos os campos do valor e aceita somente uma convencao de
layout inequivoca (boxed ou unboxed). Layout ambiguo, misto, fora do valor ou
sem metadata e recusado com causa especifica; nao existe fallback heuristico.
References aninhadas sao enumeradas para as barreiras de GC do som.
O helper JS original nao foi alterado, preservando seu caminho existente.

Todos os defaults escalares/flags/knockBack/UseSound de BunnyItem e os bindings
de defense/equip slots agora validam a largura e os offsets contra
class_instance_size antes de escrever. UseSound estatico e rejeitado; ausencia
de exports indica nomes reais no diagnostico. O contrato Byte de useStyle e
Item.active opcional permanecem preservados.

Os testes sem QuickJS executam LegacySoundStyle e Nullable<SoundStyle> com
metadata boxed/unboxed, sentinels e rejeicao de offset fora da instancia.
value_layout_test exercita ambiguidade, layout misto, overflow, campos estaticos,
referencias aninhadas e recursao invalida. Nao ha mock de structFieldOffset nem
link com a bridge Value.cpp. Cache do host/compilador teve nova geracao.
Native equipment policies continuam pendentes de desacoplamento; a bancada e
os testes reais Android continuam pendentes. Isto nao prova boot integral do
loader sem JS, nem adiciona qualquer exemplo artificialmente concluido.

## .90.29.3 — integracao real da fila e ordem das policies

O teste --pump agora liga NativeBasicItems, NativeRecipes e ambos os registradores
reais de producao, com BL_HAVE_QUICKJS=0. Aplica defaults, executa a fila real de
update, registra uma receita e confirma o preview, deixa outra pendente por
estacao ausente e a registra depois de a dependencia aparecer, sem duplicar a
primeira. Nao fornece mocks de ScriptEngine, JsLock ou structFieldOffset.
O host ainda emula metadata/metodos Terraria e nao possui mundo/renderizacao.

O pump preserva processamento de policies antes de criar previews de receitas.
Receitas aguardam policies do resultado/ingredientes quando declaradas; erro de
policy nao vira ACK. Ausencia da VM para as policies legadas nao impede receitas
nativas de itens independentes. Readiness/ACK de policy e exercitado pelos testes
de binding; nao alegar que as policies foram portadas para C++.

## Checkpoint final desta continuidade

Runtime .90.29.3 / code168, ExampleMod 0.1.36. Commit:
6fac739d8e748ca390d83cc7ebe55ba4e7b9f44a. Oito suites de regressao aprovadas;
Android build/assinatura e entrega experimental privada aprovados em
[38609568759](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38609568759).
PR de acompanhamento em rascunho: [21](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/21).
Nao e release oficial do Lote A. Nenhuma validacao de gameplay/FPS nova no Android.

Lote A permanece: 33 exemplos; 8 implementados, 6 com evidencias automatizadas,
7 parciais e 18 nao implementados. Matriz global 201, sem remover os anteriores.
Os 51 fontes/26 texturas do ExampleMod foram preservados. Nao foram adicionados
ports neste grupo. A versao 0.1.36 atualiza guia/matriz/catalogo.

Proximo passo concreto continua sendo a bancada C# completa: identidade por
categoria, SDK publico de tile/estacao/item colocavel; registro e TileObjectData;
frames/ancoras, destruicao/drop/dust, mapa/housing, adjacencia e icone do Guia.
Usar os registradores independentes e NativeRecipes existentes; nao criar
registro paralelo nem voltar a chamar RecipeLoader JS. Portar ambos os arquivos
ExampleWorkbench e as receitas originais das armaduras depois dos contratos
funcionais, mantendo os demais 33 exemplos no escopo. Policies de equipamento
e suporte de recipes alem do limite ainda precisam de desacoplamento.


## .90.29.5 — colocacao por itens

DefaultToPlaceableTile recebe IDs vanilla ou chaves de mod e escreve os campos
reais createTile/placeStyle pelo NativeBasicItems. Registro/resolucao/retry usam
TileRegistry diretamente. O --placement-wire liga o wire de C# realmente
compilado ao pump, registradores e Recipe adapter de producao sem QuickJS.
Metadados e chamadas do Terraria sao emulados no host; nao prova mundo Android.

Classificacao: API C# com bridge propria usando registro nativo independente
compartilhado. O layout/renderizacao/housing/apoio/drop/dust de uma estacao C#
ainda precisa de contratos adicionais; nao foi exposta uma falsa BunnyTile nem
foi promovido ExampleWorkbench. Policies de equipamento continuam dependendo
do caminho JS existente. Eight suites e Android build 38617948804 aprovados;
validacao da nova API no Android pendente.


## Grupo de mundo .99

BunnyTile.Flags → TILE → SourceLoader → NativeTileFlags/NativeContent → arrays Terraria. API C# com bridge nativa própria; registro/persistência usam serviços nativos independentes compartilhados. O teste world_loaders/tile_flags_test.cpp executa o publicador de produção com metadata controlada e wire do ExampleMod compilado, BL_HAVE_QUICKJS=0. Não inicializa VM JS. Isso comprova esse caminho de flags, não a independência integral dos loaders de tile/árvore/parede ainda pendentes. TreeLoader/TileEntity/GlobalTile C# completos não existem nesta revisão; não se expõem fachadas vazias.

## Paredes C# .100

Classificação: API C# original com bridge própria, utilizando serviços nativos independentes compartilhados. BunnyWall/SourceLoader chamam ModWalls diretamente; não executam API JS nem inicializam QuickJS. NativeWallRuntime mantém políticas de gameplay em tabela nativa imutável, e os listeners de registro C# e JS são separados.

A prova host contém o registro/mapa real (`wall_registry_test.cpp`, BL_HAVE_QUICKJS=0), os hooks de produção com engine/metadata controlada (`wall_runtime_test.cpp`) e a aplicação de defaults de item com o helper nativo e suas máscaras (`native_item_binding_test --wall`). Não comprova renderização/persistência no Terraria nem independência de TODOS os outros sistemas: os callbacks arbitrários de parede, Tree/TileEntity/GlobalTile permanecem pendentes. O helper é aplicado na thread do Terraria; getters C# anteriores à aplicação retornam apenas o estado declarado.
