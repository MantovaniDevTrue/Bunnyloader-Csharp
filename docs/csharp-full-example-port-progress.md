# Histórico do port integral e continuidade por cinco loaders

Regra atualizada pelo usuário em19/10/2026: cinco loaders completos por APK, um exemplo original completo por loader. A regra anterior de esperar todo o port foi substituída. [Plano operacional atual](csharp-loader-batches.md) e [manifesto](csharp-loader-batches.json). Grupo1: Tile, Wall, Tree, TileEntity, GlobalTile. Revisão interna .99/ExampleMod0.1.58 inicia terreno sólido e mantém a .96 aprovada. O gate de publicação já exige os cinco contratos, exemplos e recibos; o grupo ainda está incompleto.

As seções .97/.98 abaixo registram o histórico e suas regras de entrega à época. Não são ordens para interromper os novos grupos. Nenhuma build interna passada se torna uma entrega de cinco loaders por mudança do planejamento.

## Estado de implementação

Referência canônica ExampleModBunny:206 JS. O inventário oficial PC também permanece no escopo:449 fontes ativos,112 legados e os arquivos de ExampleLibraryMod, conforme `tmodloader-example-inventory.json`; nenhuma equivalência integral foi certificada por este checkpoint. O port do anexo não substitui essa auditoria separada. O anexo1.8.0 tem178, todos contidos na referência1.9.0. Nenhum foi removido da matriz. Revisão de desenvolvimento `.90.29.98-full-example-port-wip`/code275, ExampleMod0.1.57,78 fontes C# editáveis, SDK93 tipos públicos. Não é release, não conclui os lotes e não foi validada no telefone.

### Áudio nativo reutilizável

`BunnySoundStyle` configura um asset WAV/Ogg/MP3 relativo a content/, volume, pitch, variação, máximo de instâncias e IgnoreNew/ReplaceOldest. `BunnyItem.CustomUseSound` e `BunnyWeapon.CustomUseSound` têm precedência sobre som vanilla. O SDK valida caminhos/valores; o protocolo auxiliar USE_SOUND preserva linhas primárias V2. Cache compilador v29 invalida resultados anteriores.

O SourceLoader resolve dono pelo registro nativo e transfere o descritor ao serviço `NativeUseSounds`. Este valida diretório/arquivo real, confinamento após resolver symlinks, tamanho do asset, campo mutável `Item.UseSound` de referência LegacySoundStyle, assinatura completa do construtor e SoundType enum de4bytes. Cria marker1000 com style registrado no serviço de áudio comum e mantém handle GC forte. A publicação da tabela é atômica, depois de todos os markers; falhas liberam raízes e estilos temporários. A escrita usa barreira do GC, ou fallback somente se o GC foi confirmado não incremental. Não chama QuickJS nem escreve offsets fornecidos pelo mod.

O roteador compartilhado agora usa SfxQueue, com fila/histórico limitados, em vez de manter uma segunda fila C++ e mapas de streams/tokens sem descarte natural. Preserva os tokens JS na faixa antiga, limites, substituição, pausa e cancelamento. Campos e getters estáticos de áudio são validados antes da leitura; instalação rejeitada retorna erro, não style falsamente utilizável. Liberação de estilo não desloca IDs e não reutiliza placeholders JS de som inválido.

ExampleGun possui textura e Ogg originais, ranged/ammoBullet/shootSpeed10/fallbackPurificationPowder, dano20/knockback5/crit0, uso10/autoReuse/noMelee, sellPrice10gold e offset(-18,0). Usa volume0.9, variação0.2 e máximo3. Não substitui ExampleBulletLauncher nem outros fontes existentes. Está implementado aguardando validação, sem certificação de áudio/combate Android.

## Testes

-36 casos gerenciados de itens/receitas, incluindo validação do áudio e compilação/execução do pacote inteiro.
-43 verificações de entidades e31 casos de combate preservados.
-Dez executáveis nativos: os oito anteriores, decode/binder/publicador/raízes/barreira/rollback com wire do ExampleGun compilado e roteador C++ real com campos/getters inválidos, compatibilidade de tokens JS, limites, pausa, recusa e dez mil conclusões.
-Smoke Roslyn completo:54 classes/68 linhas, fontes editáveis, recompilação e segundo mod independente.
-Verificação do pacote exige também Ogg original e recusa áudio ausente/cabeçalho inválido; cabeçalho não prova decodificação no Android.
-Contratos IL2CPP/receitas/projéteis e sintaxe dos fontes nativos modificados preservados. Sanitizers e CI devem ser registrados separadamente depois da execução.

Os providers IL2CPP/Android dos testes de host são controlados. Não executam SoundPool no telefone, decodificador Android nem combate Terraria. Gameplay/FPS e compilação fria em5s não foram medidos nesta revisão. Não há pacote novo entregue ao usuário.

## Sistema de disparos implementado na continuidade .98

BunnyShootSource, BunnyShootContext, BunnyShot e BunnyShootRuntime conectam CanShoot/ModifyShootStats/OnShoot reais ao worker Mono e aos hooks Player.ItemCheck_Shoot/Projectile.NewProjectile. O snapshot chega após seleção de munição e modificadores; fonte/dono/modifier permanecem no jogo. Saída com até64 spawns, validação integral antes de aplicar, referências por registro compartilhado, preparo de métodos/instâncias, prazo3ms, cancelamento e diagnóstico limitado por conteúdo. Não há intermediário QuickJS. O transporte compartilhado tem prioridade sobre polling/eventos/AI; não interrompe uma chamada já em execução.

ExampleShotgun foi portado com defaults, textura original, offset e algoritmo original de oito tiros±15°/70–100%. Estado implementado aguardando validação: host não comprova ABI/gameplay Android. A matriz contém os206, Weapons4/18 implementados (22.22%),0 integralmente validados. Os demais exemplos dependentes de projéteis/IA/CanUseItem/draw continuam pendentes; o callback isolado não os conclui. Nomes exibidos/traduções/prefixos de todo ExampleMod agora começam com Example por pedido do usuário; IDs não mudaram. O teste executa todas as classes e seus nomes localizados.

Testes locais:42 verificações gerenciadas de disparos e hooks C++ de produção com saída da escopeta compilada; binder/ABI/falhas/transação/dono/fonte/munição/dano/AI/limites; ASan/UBSan; warmup/prazo/cancelamento no scheduler. Regressões:36 itens,43 entidades (dez mil ticks sem alocação),31 combate, dez executáveis de summons/áudio, receitas/projéteis, smoke Roslyn55 classes/72linhas e seis verificações de assets. CI Android desta revisão aprovado no código367488d9, registrado abaixo. Nenhum pacote será publicado. Guia: content/Documentation/SHOOTING.md.

## Planejamento anterior de disparos (executado acima)

Implementar Shoot/ModifyShootStats gerenciados, com snapshot tipado e lista de spawns validada. Consultar `script/js/mod/Loaders/ItemLoader.js`, contratos reais de `Player.ItemCheck_Shoot(int,Item,int,bool)` e `Projectile.NewProjectile(IEntitySource,float,float,float,float,int,int,float,int,float,float,float,NewProjectileModifier)`. Preservar fonte/dono/dano já modificado/munição e direção de mira. Não usar um helper fixo de oito tiros como substituto do callback original.

Reutilizar transporte do worker Mono com identificação, limite de fila, prazo e cancelamento, sem misturar métricas de receitas. Preaquecer dispatcher e instâncias para não mascarar inicialização fria com tiro original. Validar todas as saídas antes de tocar o motor, executar spawns na thread do jogo e impedir reentrada dos próprios spawns no mesmo callback. Testar zero/um/oito tiros, exceção/atraso/retorno inválido, ordem de outros hooks JS/C# e dano derivado da arma.

Portar ExampleShotgun e todos os outros exemplos que essa infraestrutura desbloquear. Prosseguir magia/holdouts/IA/draw e demais famílias conforme a matriz; não recriar bancada, armaduras ou summons já existentes e não declarar outros exemplos completos por compartilhar uma propriedade. Restam todos os lotes, até contabilizar e implementar o comportamento de cada arquivo. APIs ainda faltantes são pendências, não limitações técnicas comprovadas.

Configurações de som registradas são imutáveis durante a sessão: repetir a mesma definição reutiliza o registro; mudar asset/opções requer reiniciar o jogo. A recusa explícita evita reutilizar um style ID ainda referenciado por itens antigos. Não é suporte a hot reload de áudio.

Sanitizers dos novos testes de áudio (ASan/UBSan) passaram no host. CI Android registrado abaixo no commit exato; não há validação nova no dispositivo.

O cache nativo reutiliza o asset por caminho canônico e o marker por arquivo/opções; conteúdos que compartilham a mesma configuração compartilham também o limite de instâncias, como no Bunny original. O cache de markers é fraco, sem reter objetos de lotes recusados. Falha assíncrona de decode é registrada no logcat com ID, caminho quando disponível e status real do SoundPool.

## Verificação interna concluída, sem entrega

Fonte `e11f5279b1ed4a573fb08e999cc3228c8cd2dc19`: [CI39190838803](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39190838803) e nove suítes independentes no mesmo SHA aprovados. Gradle executou testes unitários e assembleDebug; assinatura/APK verificados. Etapas de publicação, export dos .bmod e probe puladas; job de conferência de entrega pulado. Repositório Builds acessível, tag correspondente ausente (HTTP404). Não existe nova release para recomendar ao usuário.

[Registro técnico completo](validation/csharp-full-port-wip-2026-10-19.json). Os testes de host e a build Android não substituem gameplay. ExampleGun aguarda validação, demais exemplos e o inventário PC continuam no escopo, zero loaders inteiros certificados. O commit posterior só documenta a evidência; não modifica os binários internos.


## Próximos passos após este checkpoint

1. Conferir CI desta revisão sem reabilitar publicação, incluindo Android/JNI/link e todos os testes existentes.
2. Implementar snapshot público reutilizável de CanUseItem/contagem de projéteis e os ciclos de IA/draw de lanças/chicotes/holdouts/beam. Consultar cada original antes de promover exemplos.
3. Portar ExampleSpear/ExampleWhip/ExampleSentryItem e seus projéteis/buffs completos em grupos coerentes. Shoot sozinho não substitui esses sistemas.
4. Seguir todas as categorias da matriz206 e o inventário oficial PC, mantendo fontes/assets/comentários e procedimentos Android por ExampleMod. Não reimplementar as bridges estáveis.

A aprovação geral da .96 está em validation/csharp-v96-android-user-2026-10-19.json; não informa testes individuais nem medição de FPS. A .98 permanece experimental interna e sem certificado Android de gameplay. Publicação parcial continua proibida pelo pedido vigente do usuário.

O transporte aceita até32KiB para os64 comandos, com teste de identidades longas. OnShoot preserva a propriedade BunnyWeapon.Shoot existente. Exceções nativas são capturadas pelo GuardedCall existente, com restauração do escopo TLS e teste de retomada após falha; não se promete rollback de efeitos que o Terraria já aplicou antes de lançar a exceção.

O sentinela da fábrica foi conferido nos ItemLoader/GlobalItemLoader/PlayerItemHooks originais:1000 nesta versão Android. O teste cobre a pool com1001 células e recusa de pool menor; não se usa comprimento do array como retorno falso de Shoot.

O preparo executa apenas operações internas do SDK (parsing/serialização/random/math), sem chamar gameplay do mod. O contexto fecha também quando OnShoot lança exceção e Spawn valida a thread criadora; os testes cobrem contexto retido e emissão de outra thread.


## CI final .98 — código367488d9

Fonte `367488d989ac1a81ae867e8492d36a61c6880945`: [CI39236367596](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39236367596) aprovado, incluindo testes unitários Gradle, compilação ARM64/assembleDebug e assinatura/APK. Nove suítes independentes de push aprovadas no mesmo SHA. A revisão intermediária aea495b0 também compilou; ela não certifica os ajustes posteriores, cobertos pelo CI final.

Publicação de APK, exportação de .bmod e job de conferência de entrega pulados. O workflow exportou somente resultados de testes/medições; nenhum pacote executável. Repositório Builds acessível e tag correspondente ausente (HTTP404). Não há nova release para entregar. A .96 permanece o checkpoint aprovado pelo usuário; a .98 precisa de gameplay Android.

[Evidência completa com jobs, passos, suítes, contagens e cobertura](validation/csharp-v98-internal-2026-10-19.json). O commit posterior é apenas documentação e matrizes. Não confundir esse commit com o SHA de código testado acima.

Continuidade: implementar as dependências reais de CanUseItem, contagens por dono, IA/draw e spawns em outros ciclos antes de promover lança/chicote/sentry/holdouts. Auditar também o compartilhamento de assemblies/estado entre os dispatchers antes de exemplos que dependam de estáticos entre sistemas. Nenhuma dependência ainda não investigada é declarada limitação técnica comprovada. Preservar todos os206 JS, o inventário oficial PC e o bloqueio de entregas parciais.


## Continuidade atual .99 — mundo em cinco sistemas

Ver csharp-loader-batches.md. ExampleTile e ExampleTileItem portados com flags booleanas nativas tipadas/atômicas por conjunto, mapa, dust, colocação e receita original. Assets preservados. SDK amplia sete flags; TILE mantém13 colunas e cache Roslyn passa a v31. Nenhum loader inteiro é promovido. Todas as88 entradas do inventário permanecem distribuídas no plano; DustLoader também está no escopo PC. Primeiro concluir contrato TileLoader, depois paredes, árvores completas, entidades e globais. As suítes e o gate devem ser executados antes de qualquer entrega.


Checkpoint .99 confirmado: fonte f70caa80, CI39246685678 e nove suítes independentes de push aprovados no mesmo SHA. APK interno compilado/validado; publicação e exportação dos pacotes puladas pelo gate dos cinco loaders. Gameplay/FPS Android pendentes. Evidência: validation/csharp-world-loaders-v99-2026-10-19.json. Ver csharp-loader-batches.md para continuar exatamente o grupo1.
