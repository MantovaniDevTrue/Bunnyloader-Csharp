# Regra vigente — 21/10/2026: quinze loaders completos, sem APK parcial

A instrução mais recente exige avançar até cerca de **quinze loaders completos**,
com APIs públicas, bridges reais e exemplos editáveis/comentados. **É proibido
gerar APK enquanto o escopo escolhido tiver funcionalidades faltantes.**
A regra antiga de cinco loaders por APK, preservada abaixo como histórico,
não autoriza uma entrega menor nesta execução.

Escopo verificável: [quinze contratos e pendências](csharp-loader-completion-15.json).
Continuidade e evidências: [integração de 21/10](csharp-loader-integration-20261021.md).
O gate `tools/ci/check_loader_delivery_gate.py` bloqueia empacotamento/publicação;
os testes e a linkagem de `libbunny.so` sem APK continuam permitidos.
SystemUpdate (#126) e Gore (#121) foram integrados como trabalhos existentes:
seus callbacks não são contados como loaders extras nem substituem pendências
na lista de quinze. Android e FPS permanecem estados de validação separados.

O inventário do anexo ExampleModBunny 1.9.0 contém 206 fontes JS, preservados
com hashes em `docs/generated/examplemod-1.9.0-upload-inventory.json`. As metas
globais JS/PC e a identidade dos exemplos anteriores continuam preservadas.
Nenhuma porcentagem de conclusão total é deduzida da aprovação do CI.

Continuidade de 22/10: [fundo subterrâneo nativo e exemplo original C#](csharp-underground-backgrounds.md).
Dois dos quinze contratos estão implementados nos fontes e testados no host;
treze permanecem pendentes. Nenhum dos dois está validado no Android. As
integrações adicionais SystemUpdate/Gore continuam fora dessa contagem.

## Histórico — regra de cinco loaders, substituída nesta execução

### Registro anterior — cinco loaders completos por APK

O pedido mais recente de19/10/2026 substitui a exigência anterior de aguardar o port integral antes de cada entrega: **cinco loaders completos por APK e um exemplo original completo por loader**. Preservar todos os exemplos existentes e a meta global. Plano operacional: [grupos, critérios e continuidade](csharp-loader-batches.md); inventário executável: [csharp-loader-batches.json](csharp-loader-batches.json). Primeiro grupo: Tile, Wall, Tree, TileEntity e GlobalTile. Auxiliares não contam como loaders extras. Nenhum dos cinco está certificado; publicações são condicionadas ao gate dos cinco, às regressões e à build. CI39246685678 e nove suítes passaram no código f70caa80; nenhuma publicação ou validação de gameplay Android. [Evidência](validation/csharp-world-loaders-v99-2026-10-19.json).

As entradas anteriores abaixo são histórico, incluindo as proibições de entrega parcial e a antiga ordem de lotes. Para as entregas daquele registro, prevalecia aquela regra; as matrizes de206 JS e do ExampleMod PC continuam no escopo.

> [Evidência final interna .98](validation/csharp-v98-internal-2026-10-19.json): código367488d9, APK compilado/validado, nenhuma publicação de APK/.bmod. Port integral continua em desenvolvimento.

> Continuidade .98: nomes Example em todo conteúdo, callbacks C# CanShoot/ModifyShootStats/OnShoot com bridge real e ExampleShotgun; testes de host e matriz atualizados; CI Android39236367596 e nove suítes aprovados no código367488d9, gameplay Android pendente. Publicação permanece bloqueada. O usuário aprovou a .96 com ExampleMod de forma geral; registro em [evidência Android](validation/csharp-v96-android-user-2026-10-19.json).

> Verificação interna sem entrega: [CI do port em desenvolvimento](validation/csharp-full-port-wip-2026-10-19.json), fonte `e11f5279`, nove suítes e Android build aprovados. Publicação e export dos pacotes pulados; nenhuma release nova. Port integral ainda aberto.

> Preservar também o inventário oficial PC (449 ativos/112 legados e ExampleLibraryMod); a matriz de206 JS não substitui nem reduz esse escopo. Não há equivalência automática com assemblies tModLoader.

> Continuidade do port integral: [estado, APIs de áudio e próximo sistema compartilhado](csharp-full-example-port-progress.md). Revisão de desenvolvimento .98/ExampleMod0.1.57 sem publicação.

> Regra de entrega atualizada em 19/10/2026: o usuário exige o port integral antes de nova entrega. Publicação automática da branch `feat/csharp-summon-lifecycle-v95` desabilitada nas políticas push/PR; compilação e testes permanecem habilitados. Não reabilitar publicação para entregar outro subconjunto. A .96 já publicada permanece apenas checkpoint histórico; 206 exemplos seguem no escopo e nenhum pendente pode ser omitido.

> Entrega experimental .96: [CI, artefatos e validação pendente](validation/csharp-v96-ci-2026-10-19.json). Fonte testada `96c90e63`; nove suítes e Android build aprovados. Não conclui os lotes.

> Continuidade .96: [defaults nativos, holdouts e quatro ports](csharp-native-defaults-v96.md). Anexo1.8.0/178 scripts integralmente contabilizado dentro dos206 canônicos. ExampleMod0.1.55 com76 fontes. Novos exemplos aguardam Android; não conclui o lote de armas/projéteis.

> Correção atual .95.2: relatório Android da .95.1 comprovou ausência de Projectile.identity e carga parcial do ExampleMod. Contrato corrigido com ciclo nativo SetDefaults e testes sem esse campo; validar novamente o conjunto no Android. Ver [relatório](csharp-summon-v95.md). Não promove exemplos a completos.

## Continuidade .95 — summons por mecanismos compartilhados

Ver [csharp-summon-v95.md](csharp-summon-v95.md). .93/.94/.90.6 integradas, cancelamento e dono tratados nativamente, controlador de movimento por update e originalDamage pela origem real. Não conclui Lote 1 nem muda pendências para validadas Android. Guia no ExampleMod0.1.54. Manter os 192 históricos e 206 canônicos.

> Continuidade .93: [última build .90.5 integrada aos loaders .92](csharp-loaders-v93.md). Novas políticas públicas de projétil e exemplos summon parciais; testes Android pendentes. Histórico preservado abaixo.

> Continuidade atual: [loader próprio .92](csharp-loaders-v92.md), [PR93](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/93). Fonte `2ea904f4`: nove regressões, build/assinatura e entrega privada aprovadas. [Evidência](validation/csharp-v92-ci-2026-10-18.json). Preserva a .91; três receitas adicionais por API comum, sem certificar conclusão do Lote A ou paridade de todos os loaders. Gameplay Android da .92 pendente.

# Plano global de conclusao do ExampleMod C#

> Retomada atual: branch `feat/csharp-loader-foundation-v91`, PR92, derivada da
> .90.3.1/PR90. Runtime `.90.29.91`, ExampleMod 0.1.50. Ver
> [relatório atual](csharp-loaders-v91.md) e [evidência CI](validation/csharp-v91-ci-2026-10-18.json).
> As seções anteriores de progresso abaixo são histórico; bancada, armaduras,
> equipamentos e receitas já existentes devem ser preservados. Todos os loaders
> continuam no escopo; .91 é experimental e ainda aguarda gameplay Android.

## Referencias e regra de conclusao

Base estavel atual: Bunny .90.28.4 / ExampleMod 0.1.34, relatada pelo usuario
como funcional no Android. A referencia historica .90.27.6 / 0.1.33 dos
equipamentos nativos tambem foi preservada. Referencia atual: `examplemod.bl` 1.9.0 fornecido pelo usuario, com **206 scripts JS**.
O snapshot upstream e608114efb15a3a021887a6132d9e2a487ba6b62 conserva os
201 arquivos anteriores; nenhum desapareceu. Os cinco novos globais e a alteracao
de `Content/ExampleRecipes.js` estao identificados no inventario reproduzivel.
O fork historico tem 178 scripts. Contagens historicas de 169/192 nao limitam o escopo.
Cada arquivo tem uma entrada na [matriz](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
e dados completos em [JSON](../Split/Bunny-Loader/docs/generated/example_completion/matrix.json).
O pacote e seus assets sao identificados por SHA-256 em
`tools/example_completion/package-reference.json`; scripts novos/alterados tambem
preservam o fonte como referencia, sem serem executados pelo mod C#.

Estado de continuidade em 2026-10-15: HEAD preservado `94c8c1c7161e0d7dfbe1dbc7f02d444021006fa3`,
Bunny `.90.29.17-csharp-armor-tooltip-lookup`. Bancada e tres armaduras ja integradas;
nao reimplementar esses sistemas. ExampleWings integrado no ExampleMod 0.1.38,
com pendencia explicita de independencia do caminho de equipamentos em QuickJS.
Desenvolver os grupos A2/A3 e executar CI antes do proximo checkpoint de APK.
Shaders personalizados continuam fora desta etapa; a mencao no inventario global
nao antecipa sua implementacao.

Estado implementado exige comportamento original, assets e bridge reutilizavel.
Estado Validado no Android exige evidencia de jogo do exemplo inteiro;
Validado por testes automatizados registra apenas os contratos exercitados no host. O efeito isolado de Shield/Shoes validado nao equivale
a portar dash, voo, waterWalk, pesca ou outras mecanicas do arquivo JS.
Ausencia de API C# nao e prova de limitacao tecnica do Terraria.

## Lotes e dependencias (nenhuma categoria excluida)

1. **Auditoria:** inventario canonico, classes/hooks/campos/assinaturas/imports,
   comparacao do core original/fork, exemplos adicionais C#, estados e evidencias.
2. **Itens/atributos/equipamento/armadura:** base geral de itens e receitas;
   ferramentas, materiais, consumiveis, ammo, raridades, prefixos completos;
   slots/sets/animacoes; atributos, dash, voo e efeitos de cada acessorio; conjunto
   de armadura e cosmeticos. Preservar Guardian/Trailblazer/Mystic atuais.
3. **Armas/projeteis:** DamageClass e mana/ammo; melee/ranged/magic/summon;
   lancas/chicotes/ioios/flails/espada curta, beams/lasers/holdouts, minions,
   sentries e tiros, animacao/collisions/hitboxes/efeitos e mira. IA fiel.
4. **NPCs/bosses:** estado/IA/animacao/spawn/loot/GlobalNPC; town NPC, shops,
   moedas customizadas, boss real/fases/ataques/barra/invocacao/progressao/save.
5. **Tiles/moveis/mundo:** todos os 37 tiles, 36 itens colocaveis, paredes;
   frames, portas/camas/chests/dressers/clocks/storage/drops/light/glow/plantas;
   banners/pylons/arvores/tile entities, trofeus/reliquias/music boxes/armadilhas.
6. **Biomas/ambiente:** contagem/deteccao, surface/underground/background,
   agua/cachoeiras/nuvens/music/condicoes. Reusar mecanismos do loader original.
7. **Especiais:** pesca/quest fish/bobbers/hooks/grappling; pets/light pets,
   montarias/minecarts, conquistas/emotes/hairs, commands/configs/UI/modmenu,
   systems globais, persistencia, audio e todos os helpers/main ainda pendentes.
8. **Auditoria final:** comparar novamente cada arquivo, SDK publico e bridge;
   preencher lacunas, testar convivencia JS/C#, multiplos mods, loadouts,
   Save/Exit/reentrada, morte/respawn, assets e desempenho no dispositivo.

A dependencia itens/receitas antecede armaduras e armas; pets/minions dependem
de projeteis e buffs; town shops dependem de itens/moedas; bosses dependem de
NPC/projetil/persistencia. A ordem interna pode mudar por essas dependencias,
mas todos os arquivos permanecem no backlog numerado na matriz.

## Lacunas da arquitetura identificadas antes do lote seguinte

- SourceParser/SourceLoader agora reconhecem BunnyItem alem das bases especializadas.
  Ainda faltam contratos gerais de ModSystem, Tile/Wall, biome e outros sistemas.
- Mods somente de BunnyItem ja carregam sem arma/projetil. A faixa de verificacao
  Mono foi ampliada de 2..32 para 1..1024 com testes de fronteira; preservar esse contrato.
- Arquivo atualizado no grupo .90.28.5: 512 .cs, 256 KiB cada, 16 MiB totais; defaults
  wire de 2 MiB, 256 por categoria/1024 tipos. Contratos de fronteira continuam
  obrigatorios; slots e filas nativas tem limites proprios.
- SetDefaults e politica precomputada nao substituem callbacks de jogo; cada
  hook novo precisa de contexto, caminho runtime e teste de comportamento.
- O pipeline JS original ja tem receitas, equipment, tiles e sistemas; priorizar
  reutilizacao no host em vez de duplicar logica de Terraria dentro do MonoVM.

## Entrega e evidencia por lote

Codigo-fonte publico + .bmod com fontes/guia/assets + APK no Builds privado.
CI gera/conferir matriz usando checkout upstream PINADO e executa regressao.
Relatorio deve distinguir contrato/bridge testado no host, compilacao Android,
execucao real no aparelho e FPS medido. O alvo e 60 FPS; nenhuma captura ou
compilacao isolada prova FPS sustentado. Shaders customizados estao fora desta etapa.

Ao retomar: ler matriz JSON, evidencias e este plano; verificar HEAD e atualizar
o inventario se houver novos exemplos upstream. Nunca promover estados de forma
automatica so porque o build passou. Roteiros Android acompanham cada lote.

## Ponto atual para retomar

Base validada: .90.28.7 / ExampleMod 0.1.35, incluindo mascara e robe confirmados
pelo usuario. Branch dessa base: codex/example-completion-v90-28; preservar seus
commits. Branch de trabalho do Lote A completo: codex/lot-a-complete-v90-29.

Escopo fechado e verificavel: 33 arquivos em lot-a-scope.json; inventario global
199 JS. Nao publicar novo lote por partes nem passar a B antes de concluir A.

Infraestrutura ja implementada na branch de trabalho: defesa nativa, UpdateEquip
com imunidade/mana/minions/atributos, ArmorSetBonuses com tooltip e bonus real,
receitas em estacoes de mod por UID/nome (inclusive tiles JS), wingSlot/WingStats/
VerticalWingSpeeds no WingLoader original, diagnosticos por receita e round-robin
nas filas de registros pendentes. APIs sao publicas e testes usam fontes C# de
mods independentes. Isso ainda nao e port dos exemplos finais.

Proxima dependencia concreta: BunnyTile e bridge para ExampleWorkbench original,
com TileObjectData 2x1, flags de mesa/estacao, drop, mapa, frames e AdjTiles; depois
port integral Helmet/Breastplate/Leggings com texturas e receitas originais.
Seguir todos os 33 arquivos e dependencias, inclusive Costume, asas/acessorios,
consumiveis, fishing/tools/projeteis, prefixos/raridades e tooltips/currency.
Checkpoint testado: runtime e466ae4c4fc1097b67ad0d0c6460a36e4fb66b8b,
run Android 38573406719, oito suites aprovadas.
Ver [contratos e continuidade](csharp-lot-a-development.md). CI compila Android,
mas nao publica APK desta branch de lote incompleto. Nenhuma nova alegacao de
validacao no Android ou 60 FPS para as APIs em desenvolvimento.

## Historico das correcoes preservadas

### Regressao Android .90.28.5 / correcao .90.28.6

ExampleBeard foi recusado no registro real por Texture sem extensao. A correcao
normaliza o caminho no registrador e elimina rejeicoes silenciosas no preflight.
Teste novo cruza fontes reais, parser e o contrato de caminho do registrador;
o CI tambem verifica os assets. Mantidos .90.28.4 como base estavel, todos os
199 exemplos e o backlog do Lote 2. Validacao Android da correcao pendente.

Build corretiva .90.28.6 aprovada no run `38563527063`, commit de runtime
`cec309b023f4092723d29c43567865c3ebb2fe33`, ExampleMod 0.1.35. Oito suites
independentes passaram. Proximo passo: confirmar registro no Android; o proximo
grupo de implementacao continua Workbench/receitas e armaduras.

## Regra de entrega atualizada pelo usuario

Lotes A, B, C e seguintes sao entregas integrais de suas categorias: todos os
exemplos contabilizados na matriz, com suas funcionalidades originais aplicaveis.
Grupos por dependencia servem para organizar commits e testes internos, mas nao
substituem o lote completo nem sao anunciados como uma entrega de novos exemplos.
Nao passar ao lote seguinte enquanto o atual estiver incompleto. Bloqueios reais
exigem evidencia e permanecem na matriz; nunca contam como exemplos funcionais.
Correcoes de regressao dos exemplos ja entregues podem gerar APK corretivo,
identificado explicitamente como correcao, sem declarar o lote concluido.

### Correcao .90.28.7: equipamento de vaidade

Usuario confirmou carregamento sem erros na .90.28.6, mas ExampleBossMask e
ExampleRobe nao equipam. Slots atribuídos apenas no host nao eram persistidos
no caminho nativo de SetDefaults dos BunnyItem. A bridge agora liga o slot ao
registro do item e o reaplica em novas instancias, validando campo/tipo/largura
e limite antes da escrita. O Lote A permanece aberto e precisa ser entregue
com TODOS os seus exemplos, nao apenas os seis da entrega anterior.

Correcao .90.28.7: pipeline `38566058511` aprovado, runtime commit
`e50e613f649e8806f8a82c919859dbc4c60309db`; oito suites de regressao passaram,
APK assinado e ExampleMod 0.1.35 gerados. Evidencia de CI final registrada apos
o build; o pacote inclui a matriz/roteiro do commit de runtime. Validacao
Android da mascara e do robe pendente. Lote A permanece incompleto.

## Continuacao integral do Lote A

Usuario confirmou .90.28.7 funcional. Desenvolvimento em
`codex/lot-a-complete-v90-29`, derivada da branch validada sem alterar a release.
Escopo completo: 33 arquivos, em lot-a-scope.json. Ver
[codigo, contratos e continuidade](csharp-lot-a-development.md).
Infraestrutura de armadura testada por mod de contrato independente; exemplos
ainda nao portados nao recebem promocao na matriz. Nao publicar entrega parcial.

### Retomada .90.29.1: receitas independentes

Servico nativo de recipes implementado e ligado a fila C#, com registradores
compartilhados separados da instalacao de assets. Mantem .90.28.7 e historico;
nao conclui o Lote A. [Auditoria de independencia](csharp-independence-audit.md)
registra testes reais do adapter no host sem QuickJS e dependencias restantes.
Prioridade seguinte permanece bancada completa, seus dois arquivos originais
e receitas das armaduras. 33 arquivos do escopo e 199 da matriz preservados.

### Atualizacao da referencia original em 14/10

Original avancou seis commits; dois exemplos de touch buttons foram incluidos
sem remover os 199 anteriores. Matriz atual tem 201 JS. O escopo A permanece
33. Novos exemplos estao Nao implementado no Lote F/sistemas especiais; APIs
de botoes de toque, ProcessTriggers e layout persistente devem ser auditadas
nesse lote. Nenhuma alteracao do core original foi transplantada nesta etapa.

ExampleMod 0.1.36: matriz embutida 201 e guia editavel RECEITAS_NATIVAS.md,
mesmos 51 fontes/26 texturas; nenhuma classe nova anunciada. Catalogo marca
receitas nativas como experimentais ate validacao no Android.

Checkpoint de codigo seguinte: .90.29.2/code167 remove o helper QuickJS de
ValueLayout dos itens, valida bounds reais de Item e amplia regressao boxed/
unboxed/nullable sem mocks da bridge JS. Ver csharp-independence-audit.md.
A entrega permanece experimental; bancada e demais exemplos continuam pendentes.

Checkpoint .90.29.3/code168: teste executavel --pump cobre a fila real, registradores,
defaults e recipes sem QuickJS, com preview/dependencia tardia. Policies precedem
recipes e sua ausencia nao bloqueia itens independentes. SDK/examples continuam
preservados; bancada completa ainda e prioridade de retomada.

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


## Checkpoint 17/10/2026 — compilador experimental .90.29.85

A prioridade imediata solicitada passou a ser a primeira compilação C# em até 5 s. A .84 medida no Xiaomi preparou cache em 576 ms, mas a primeira compilação consumiu 10.110 ms, dos quais 8.324 ms no Emit. A .85 adiciona AOT normal ARM64 de partes do compilador/framework, preserva o Mono corrigido, o carregamento automático editável, fallback JIT e os dois históricos .84, incluindo ExampleMod 0.1.45.

Código testado: `a22bfc258159aa72ce1527bcad20c0a39d9504e7`; [CI 39030923144](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39030923144) aprovado, oito workflows independentes aprovados, APK assinado e entrega privada experimental concluída. O alvo de 5 s, memória, estabilidade e FPS da .85 aguardam Android. A alteração não adiciona ports nem altera os estados da matriz; os números de checkpoints anteriores deste plano são históricos, não uma nova auditoria.

Relatório de implementação, contratos, artefatos e procedimento de teste com o ExampleMod: [csharp-cold-compiler-v90-29-85.md](csharp-cold-compiler-v90-29-85.md). Próxima ação deste grupo: colher o perfil cold/warm do Xiaomi e tratar o custo restante antes de afirmar a meta; os lotes e pendências funcionais continuam regidos pela matriz vigente.


## Checkpoint 17/10/2026 — revisão .90.29.88.8 e ampliação da referência

A revisão parte da .88.7, ExampleMod CSharp 0.1.47 e Lights And Shadows 0.4.2.3, preservando a branch `hotfix/csharp-88-5-sigabrt-logcat-tail`. Código de runtime testado: `af844936dfa0b9435388723d02724daa5024985f`; PR [84](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/84), branch `fix/csharp-88-8-lights-review`. A .28.4 e os checkpoints anteriores continuam sendo evidências históricas, sem reversão dos sistemas posteriores.

Foram corrigidos o limite de expressão do workflow Android, os contratos de tipo/tamanho/layout/armazenamento das leituras IL2CPP de ScreenScene e o resíduo da transição de luz no subsolo. Não foram adicionados shaders, removidos passes/efeitos nem alterados IDs persistidos. Ver [relatório da revisão](csharp-review-v88-8.md) para testes e entrega. Os dados opcionais de céu/bioma/gravidade do Lights ainda não estão todos conectados a leitores nativos; não afirmar equivalência integral ao PC.

O usuário ampliou expressamente a meta para todos os exemplos oficiais do tModLoader, adaptados à API própria do Bunny. A referência oficial fixada contém 1.210 arquivos do ExampleMod, 561 fontes C# (449 ativos e 112 legados em Old), mais 11 arquivos da dependência ExampleLibraryMod. Todos estão contabilizados no [novo inventário](tmodloader-example-inventory.md), com caminhos e SHA dos blobs; nenhum port é certificado por coincidência de nome. Não há dependência de assemblies tModLoader no Android.

A matriz JavaScript existente permanece independente e preservada: a auditoria CI desta revisão contabilizou 178 JS no fork e 206 na referência canônica. Não promover estados dessa matriz por gerar inventário ou compilar APK. Nenhum port novo foi concluído nesta revisão. Os estados antigos do escopo A precisam ser reconciliados com os fontes atuais antes de implementar: bancada, armaduras e asas já possuem implementação posterior aos primeiros checkpoints; não recriar esses sistemas a partir das pendências históricas deste documento.

Retomada concreta: auditar os comportamentos e assets de materiais/itens/receitas da referência oficial contra o SDK e os exemplos atuais; agrupar lacunas compartilhadas, implementar API pública e bridge reutilizável, portar todos os dependentes do grupo e registrar execução de contratos e comportamento. Manter o gate de lote completo para anunciar conclusão do Lote A. Inventário de arquivos não substitui a matriz funcional detalhada, que ainda precisa ser ampliada para a referência oficial. Antes de aumentar o pacote, conferir os limites atuais de fontes/tipos/registro/wire e ampliar com testes de fronteira quando necessário.

A primeira compilação em até 5 s, a estabilidade, a fidelidade visual completa e 60 FPS continuam dependendo de medição no Xiaomi. A validação local de GLSL em Mesa e o sucesso do CI não substituem execução Android.

Evidência deste checkpoint: [Android CI 39077778658](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39077778658) aprovado, nove suítes independentes aprovadas no mesmo SHA, assinatura V3 e entrega privada experimental concluídas. ExampleMod permanece 0.1.47 e Lights permanece 0.4.2.3; não houve alteração funcional nos pacotes de fontes desses mods. O commit seguinte contém somente documentação/inventário e o teste reproduzível de GLSL para host.


## Continuidade .90.29.89 — grupos nativos de receitas

A autorizacao do usuario abrange novas APIs/classes/hooks/bridges reutilizaveis para portar toda a referencia oficial do tModLoader. Branch de desenvolvimento: feat/csharp-native-recipe-groups-v89, derivada integralmente da revisao .88.8. Versoes entregues experimentalmente: Bunny .90.29.89/code260 e ExampleMod 0.1.48; Lights permanece 0.4.2.3.

Implementadas APIs de ingredientes por grupos nativos (AddNativeRecipeGroup), referencias genericas locais (AddIngredient<TItem>/AddTile<TTile>), Clone de receita declarada e remocao de ingredientes/grupos. A bridge chama Recipe.RequireGroup real, verifica metadata e boxing Int32 e preserva as receitas antigas. O ExampleWorkbench C# demonstra duas receitas adicionais e a copia independente; sua receita original permanece intacta.

Os testes locais e o CI b22962178a6beafa34301caa89a0bced46a598bb passaram: fixture C# compilado/serializado executado no pump real sem QuickJS, SDK por reflection, nove suites independentes e compilacao/assinatura Android. [Build 39080974810](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080974810) aprovado; artefatos privados experimentais entregues. Ver [contratos, evidencias e pendencias](csharp-native-recipe-groups-v89.md).

Content/ExampleRecipes.cs oficial permanece Parcial: grupos customizados/merge/membros, multiplas estacoes, condicoes, callbacks, shimmer e alteracao global ainda faltam. Sua linha foi detalhada no inventario, sem remover os demais 449 fontes ativos/112 legados ou promover a matriz JavaScript. O lote A nao esta concluido e nenhum port oficial integral e anunciado por este checkpoint.


## Validacao recebida em 18/10/2026 — .89 / ExampleMod 0.1.48

O usuario confirmou que o fluxo entregue funcionou perfeitamente no Xiaomi. Relatorio da sessao pid=4488: MonoVM verificado, dez receitas, nenhum erro/aviso e dois mods C# preparados com cache em 609 ms, junto de Weapon Out Lite JavaScript. Registrar como validacao relatada do fluxo, sem promover todas as variantes, reentrada, FPS ou o arquivo oficial completo. [Evidencias e limites](validation/csharp-v89-android-2026-10-18.md).

A primeira compilacao em ate cinco segundos continua pendente: o snapshot historico, de outro processo, registra nativeWallMs=6254; CompileMs=0 da sessao atual significa cache. Nao misturar a reutilizacao/verificacao do worker com compilacao fria.

Proxima implementacao: investigar e ampliar grupos customizados/registro de membros e as demais lacunas de receitas oficiais usando servicos nativos independentes, preservando este checkpoint validado. Nao recriar a bancada, receitas originais, equipamentos ou sistemas ja funcionais. Manter todos os arquivos oficiais/JS contabilizados; Lote A e Content/ExampleRecipes.cs continuam parciais.


## Checkpoint em andamento .90 — PR86 e receitas gerenciadas

Branch feat/csharp-buff-recipes-v90 deriva da .89 validada. Implementa flags e
descrições executadas pelo C#, correção de todos os slots de buffs, DelBuff real,
imunidade à enfermeira, multiplicador de defesa pós-equipamentos, grupos
personalizados e callbacks/condições por worker Mono. ExampleMod 0.1.49 contém
receita da bancada C# e demonstração de debuff/poção. [Relatório e contratos](csharp-buffs-recipes-v90.md).

O conjunto solicitado NÃO está concluído. Continuar obrigatoriamente minion/cajado/
contador, Blocky/costume, montaria/carrinho, tooltip temporal e os mecanismos de
receitas ainda listados no relatório. Não são bloqueios comprovados do Android.
A matriz JS e os 449 fontes ativos/112 legados oficiais continuam preservados.
Host: 26 verificações gerenciadas e contratos nativos passaram. CI e Android devem
ser registrados separadamente; a .90 é experimental WIP, não conclusão do Lote A.

## Continuidade .91 — base atualizada pelo usuário e loaders

Retomar em `feat/csharp-loader-foundation-v91`, preservando PR90/.90.3.1
`77be9001c2e0328324bb9b60db0fd3a7a64ad23a`. As correções nativas de Register e
OnCraft continuam intactas. [Relatório .91](csharp-loaders-v91.md) e
[inventário de todos os módulos](generated/csharp-loaders.md) registram o novo
checkpoint. Existem 88 módulos no diretório original, inclusive helpers; não
confundir inventário com port completo. Nenhuma categoria pode desaparecer.

SDK e RecipeLoader: comportamentos em classes, condições compostas/remoção,
estágio de callbacks e isolamento por dono, usando as bridges existentes.
ExampleMod 0.1.50 mostra a bancada C# com o comportamento separado e fonte editável.
A .91 ainda é experimental e requer Android; o conjunto inteiro permanece aberto.
Prosseguir nas pendências explícitas de buffs/minions/Blocky/montarias/receitas e
nas dependências dos demais loaders, sem recriar sistemas estáveis.

Checkpoint de entrega .91: runtime `3126971483569438060020cd26d1f691f018b46c`, Android [39136870790](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39136870790), nove suítes do mesmo SHA, assinatura e verificação de entrega privada aprovadas. [Registro](validation/csharp-v91-ci-2026-10-18.json). O .bmod remoto possui os mesmos 67 fontes editáveis e SHA-256 conferido. A validação de gameplay Android e a conclusão dos loaders continuam pendentes. O commit seguinte de documentação não modifica o APK nem o pacote.
