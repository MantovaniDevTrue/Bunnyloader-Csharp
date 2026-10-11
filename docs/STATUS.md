# Estado real do desenvolvimento

## .75 — Multi-Mod Android APROVADO no cenário observado (11/10/2026)

O usuário testou **dois mods C# simultâneos** com **100 inimigos durante Blood Moon**, informando **60 FPS sem quedas observadas**. Prints do editor: `sourceModCount=2`, MonoVM pronto, ambos os mods `ready`; fila global **256/256**, 142 hits, 114 kills, `dropped=0`, `lockDrops=0`, `failures=0`, fila máxima 3/64. `ExampleWaveBolt`: 92/92, 60 hits e 32 kills. `SecondBolt`, de outro UID/assembly: **100/100**, 50 hits e 50 kills, `lastResult=4074`, zero falhas e perdas. Os vínculos adicionais estão incluídos no total global. FPS é **relato do testador**, não um trace; pausa/descarregamento, sessões prolongadas e multiplayer não foram validados.

**Decisão:** .75 é a nova referência funcional para dois mods C# nesse cenário, com .74 mantida para rollback. [Relatório detalhado](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-multimod-managed-v75-experiment/docs/csharp-multimod-v75.md).

## .75 — Histórico da compilação anterior ao teste Android

[GitHub Actions #38151335551](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38151335551) **success**. APK v0.1.10.16.75 e dois mods de teste publicados: `ExampleModCSharp 0.1.9` e `SecondExampleModCSharp 0.1.0` (UID separado). O novo dispatcher vincula eventos de duas assemblies/UIDs sem duplicar o worker MonoVM ou alterar a FIFO64; o smoke Roslyn verificou métodos `BunnyLiveEvents` de mesmo nome com estado independente nas assemblies. **Ainda não há teste real em Android.** Pausa por mod é desarme de callbacks, não remoção da assembly. A .74 segue estável. [Procedimento técnico](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-multimod-managed-v75-experiment/docs/csharp-multimod-v75.md) · [Release de teste](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.75-multimod-managed).

## .74 — Managed Default APROVADA no Android (11/10/2026)

A APK v0.1.10.16.74, sem o protótipo `libbunnycs.so` no boot, foi testada com ExampleModCSharp 0.1.9 e **60 FPS relatados**. `hostInfo()` indicou `runtimeMode="Roslyn+MonoVM"`, `monoVmReady=true`, `sourceModCount=1`, `nativeAotPrototypeAvailable=false`, `nativeAotExternalCount=0`; `managedAvailable()=true`. Auto: `ready`, `cached=true`, `curves=enabledCurves=2`. Eventos globais **559/559** (359 hits, 200 kills) com `dropped=0`, `lockDrops=0`, `failures=0`, pico 3/64 e fila vazia no final. `ExampleWaveBoltLiveEvents` no tipo 1136 processou **477/477** (318 hits, 159 kills, `lastResult=2171`, zero falhas), **já incluídos** nos totais globais. Os zeros `runtimeAbi=0`/`hostApi=0` pertencem ao protótipo legado e não significam falha MonoVM.

Observação não bloqueante: mensagens do C# aparecem em momentos separados no editor; causa não verificada, **nenhuma modificação solicitada agora**. A .74 é baseline estável dos recursos testados; multi-mod, multiplayer, reentrada e compatibilidade NativeAOT externa continuam sem teste específico. [Relatório completo](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-managed-default-v74-cleanup/docs/csharp-managed-default-v74.md).

## .74 — Histórico do build (anterior à validação Android)

Objetivo: tornar **Roslyn + MonoVM o caminho principal de C#**, retirando a publicação obrigatória de BunnyCsPrototype/libbunnycs.so e a inicialização NativeAOT do processo, sem apagar o carregador legado de mods externos libmod.so nem as pontes C++ de conteúdo. Melhorias de diagnóstico: managedAvailable(), hostInfo().monoVmReady e campos explícitos de legado. VersionCode 99, ExampleModCSharp 0.1.9 funcionalmente preservado e guia atualizado. **GitHub Actions [#38149174534](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38149174534) aprovado e [release .74](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.74-managed-default) com APK de 223.287.037 bytes e ExampleModCSharp 0.1.9 publicados. O APK não contém libbunnycs.so. Ainda não promover até testar no Android.** [Fonte e testes .74](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-managed-default-v74-cleanup/docs/csharp-managed-default-v74.md).

## .73 — callbacks C# independentes por projétil APROVADOS no Android (11/10/2026)

**Build GitHub Actions aprovado:** [APK 0.1.10.16.73 MultiLiveProjectiles + ExampleModCSharp 0.1.9](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.73-multi-live-projectiles), [job #38147299182](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38147299182) concluído com sucesso e [suíte rápida #38147095615](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38147095615) também aprovada. Base: .72 Managed Ingress preservada. Novidade: callbacks live `OnHitNPC`/`OnKill` de `ExampleWaveBoltLiveEvents` selecionados por tipo de projétil através da **mesma FIFO64 e do mesmo worker MonoVM**, com contadores isolados em `bl.csharp.sourceLiveProjectileStatus()`. Máximo inicial: 7 vínculos extras no mesmo mod da sessão principal, limite agregado 25.000; arquitetura multi-mod não está validada. O APK incrementa `versionCode` de 97 para 98 e reutiliza o MonoVM patchado validado da .72.

**VALIDAÇÃO REAL:** fonte e filas demonstraram 985/985 eventos processados (620 hits, 365 kills), 0 dropped, 0 lockDrops, 0 failures, fila final vazia, pico 3/64. ExampleWaveBolt recebeu **879/879 eventos independentes** (571 hits, 308 kills), tipo 1136, lastResult 2330, e sem falhas. **FPS e duração da .73 não foram informados**; permanecem pendentes os testes de vários mods, multiplayer e reinícios. [Protocolo de testes](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-multi-live-projectiles-v73-experiment/docs/multi-live-projectiles-v73.md).

## ExampleModCSharp 0.1.8 — duas armas e dois projéteis C# validados no Android (11/10/2026)

**VALIDADO no cenário observado, com APK .72:** `sourceAutoStatus` apresentou `phase=ready`, `applied=true`, `curves=2/2`, `enabledHits=1/1`, `enabledKills=1/1`, sem erro. `ExampleBolt`: 47 spawns, 360 atualizações de curva e 360 escritas de velocidade, 0 erros; `ExampleWaveBolt`: 107 spawns, **2.393 atualizações/escritas de velocidade**, 0 erros. `sourceLiveHitStatus`: **161/161 eventos gerenciados** (79 hits, 82 kills), 0 `dropped`, 0 `lockDrops`, 0 falhas, fila máxima 2/64; `projectileType=1137` corresponde ao projétil original. O usuário relatou **60 FPS** durante o teste; não foi medido o FPS de renderização por profiler nem informada duração.

O segundo projétil tem curvas independentes, mas **não** possui `OnHitNPC`/`OnKill` gerenciados próprios; a política de eventos live da .72 segue vinculada a um único tipo por processo. Os contadores de curva referem-se ao hook nativo que reproduz uma tabela pré-calculada pelo Roslyn, não a chamadas MonoVM por frame. **Próxima etapa:** multi-vínculo de eventos por mod/tipo com isolamento de ciclo de vida, testes de carga e nenhuma regressão na fila .72.

[Registro de teste e limites](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-examplemod-dual-projectile-0.1.8/docs/examplemod-csharp-dual-projectile-018.md) · [Pacote 0.1.8 experimental](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-examplemod-0.1.8-dual-projectile-test).

## .72 — Managed Ingress: primeiro perfil Android aprovado (11/10/2026)

**VALIDADO no cenário observado:** janela de 15.010 ms com 899 atualizações de `Main.DoUpdate` (**59,9583 Hz**), 0 acima de 25/33 ms; **82/82 callbacks C# aceitos/processados**, `eventsQueueDropped=0`, `lockDrops=0`, `eventsFailed=0`, fila máxima 2/64. Tempo médio de espera **77,37 µs**, tempo médio do callback **91,80 µs**. Os contadores acumulados do dispatcher registraram **224/224** eventos (112 `OnHitNPC`, 112 `OnKill`), `dropped=0`, `failures=0`, `phase=ready`. Não confundir `updateHz` com FPS do renderizador. A fila separada removeu as perdas por mutex **neste ensaio**, mas não há prova de ausência de perdas sob saturação, em múltiplos mods ou em longo prazo. [Relatório .72](managed-ingress-v72-android.md) · [Release .72](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.72-managed-ingress).

**Teste prolongado adicional (11/10/2026):** o usuário relatou ~7–8 minutos de uso contínuo do livro de teste contra 3 bosses e 112 inimigos durante Blood Moon, com **60 FPS observados durante toda a sessão**. `sourceLiveHitStatus()`: `queued=2897`, `processed=2897`, `hitProcessed=1443`, `killProcessed=1454`, `dropped=0`, `lockDrops=0`, `failures=0`, `maxQueueDepth=3/64`, `phase=ready`. Duração e FPS são **relatos do usuário**; sem captura contínua de FPS do renderizador. Os contadores são acumulados desde a ativação, não necessariamente apenas durante os ~7–8 minutos. [Evidência e limites](managed-ingress-v72-android.md).

## .68 — DLL instalada e benchmark QuickJS × MonoVM validado no Android

**Carga máxima de 5 milhões aprovada:** QuickJS 1.657.350 µs (CPU 1.646.433 µs) versus MonoVM 23.067 µs (CPU 22.444 µs), checksums iguais `-70193220`, `passed=true`, `phase="matched"`, `error=""`; vantagem **71,85×** no tempo de parede e **73,36×** na CPU da thread nesse único kernel. Quatro tamanhos de carga testados, sem falhas. [Resultados](vm-parity-v68.md).

**Repetibilidade preliminar na mesma carga confirmada (três execuções × 1 milhão):** todos os checksums iguais `1336190628`, `passed=true`, `phase="matched"`, erros vazios. QuickJS: **334,217 / 334,131 / 340,668 ms**; MonoVM: **4,532 / 4,499 / 4,491 ms**; razões por execução **73,746× / 74,268× / 75,856×**. Médias: **336,339 ms** e **4,507 ms**, razão das médias ~**74,62×**. Resultado restrito a algoritmo matemático isolado, não gameplay nem ganho de FPS. [Tabela com métricas completas](vm-parity-v68.md).

**Terceira amostra Android aprovada:** 2.000.000 de iterações, `jsResult=monoResult=-1159315361`, `passed=true`, `phase="matched"`, erro vazio. QuickJS: **675.728 µs** tempo / **661.439 µs** CPU da thread; MonoVM: **9.078 µs** tempo / **8.982 µs** CPU da thread. Razão **74,44×** tempo / **73,64×** CPU nessa matemática isolada. Agora há 500 mil, 1 milhão e 2 milhões de iterações confirmados com checksum correto. Ainda precisamos repetir cada mesma carga diversas vezes e medir gameplay real. [Relatório com as três amostras](vm-parity-v68.md).

**Segunda amostra confirmada:** 500.000 iterações, `jsResult=monoResult=-799234484`, `passed=true`, `phase="matched"` e erro vazio. QuickJS **171.763 µs** de tempo decorrido / **170.302 µs** de CPU da thread; MonoVM **2.462 µs** / **2.275 µs**. Razões **69,77×** em tempo decorrido e **74,86×** em CPU da thread. A comparação segue limitada à mesma matemática isolada e ainda precisa de repetições sob cargas iguais. [Detalhes da segunda amostra](vm-parity-v68.md).

A .67 compilou no GitHub com o DLL válido, mas no Android não o encontrou: `BunnyVmBenchmark67.dll missing or invalid`. **A .68 corrigiu o caminho da assembly e validou a paridade no Android**: 1 milhão de iterações em QuickJS (`jsWallUs=445536` µs, CPU 439607 µs) e MonoVM (`monoWallUs=6040` µs, CPU 5798 µs), checksums **iguais a `1336190628`**, `passed=true`, `phase="matched"`, `error=""`. A razão nessa única execução foi **73,764× em tempo de parede**, **75,820× em CPU por thread** a favor do MonoVM para *esse kernel isolado*. **Não é um resultado geral de FPS ou de todos os mods**.

Análise do código-base: `ManagedProbeService.installManagedFramework()` extrai as assemblies somente quando o serviço/worker é acionado. O processo `:game` chamava o MonoVM usando o arquivo antigo já existente em `filesDir/bunny-csharp`, sem garantir que a nova DLL do APK fosse instalada. A .68 instala/verifica especificamente `BunnyVmBenchmark67.dll` **no início de `GameActivity.onCreate`, antes de iniciar o núcleo nativo**, com SHA-256 do manifesto `assets/managed/framework.sha256`, cópia temporária e verificação final. Evita reinstalações desnecessárias e não mexe no gameplay normal. O benchmark agora executa em ambos os runtimes; ainda precisamos de repetições e comparação em gameplay real. [Resultados completos .68](vm-parity-v68.md).

## .67 — corrigindo a busca do método MonoVM na comparação QuickJS × C# (aguarda Android)

O teste real da .66 executou 1.000.000 de iterações em QuickJS (`jsWallUs=341098`, `jsThreadCpuUs=338308`, checksum `1336190628`), mas o MonoVM não conseguiu encontrar o método `BunnyManagedProbe.Probe.BenchmarkMath(int,int)`. O próprio MonoVM permaneceu inicializado e verificou `result=42`. **A comparação da .66 foi inconclusiva**; não houve medição de desempenho C#.

A .67 utiliza assembly exclusiva `BunnyVmBenchmark67.dll`, tipo `BunnyLoader.VmBenchmark67.Kernel`, método `Execute(int,int)`. O GitHub valida o arquivo DLL gerado **por reflexão e chamada gerenciada**, exigindo checksum `1336190628` antes de empacotá-lo no APK; também verifica a presença do arquivo no APK. A busca em Android permanece pendente de validação real. A .65 segue preservada como baseline de gameplay. [Protocolo da .67](vm-parity-v67.md).

## .66 — QuickJS × MonoVM: comparação controlada (aguarda Android)

Implementado teste opt-in de cálculo determinístico com o mesmo algoritmo e entradas para os dois runtimes: QuickJS no editor e MonoVM na thread proprietária. Mede tempo de parede, CPU da thread e instantâneos de RSS do processo, confere checksums. Até quatro execuções por inicialização. **Ainda não significa medir FPS em combate nem a sobrecarga da ponte por evento.** [Protocolo](vm-parity-v66.md).

## .65 — gameplay profiler VALIDADO no Android (11/10/2026, 00:56 BRT)

Xiaomi Android 14, ExampleModCSharp 0.1.7, singleplayer. Profiler concluído (`elapsedMs=15007`): 907 amostras de `Main.DoUpdate`, `updateHz=60.5040`, `updateP90Ms=20` (bucket aproximado), 0 intervalos acima de 25/33 ms. Foram perfilados 30 callbacks: **50,2 µs** de espera média no FIFO e **195,8 µs** de invocação C# média, máximos 186 µs / 2.225 µs. Nenhum descarte (`lockDrops=0`), nenhuma falha C#. O status acumulado tinha 65 callbacks (5 hits/60 kills), pico de fila 1/64, `phase=ready`; MonoVM verificado com `result=42`. O usuário confirmou gameplay muito fluido.

Isso **não prova aumento de FPS** em comparação com a .64 ou QuickJS, pois o profiler mede ritmo de `Main.DoUpdate`, não FPS do renderizador, e não tivemos controle A/B nem medições de CPU/RSS. As cópias de strings por callback foram eliminadas, mas o efeito quantitativo isolado ainda precisa ser comparado. [Registro completo](gameplay-perf-v65.md) · [Release .65](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.65).

## .64 — 25 mil callbacks C# comprovados no Android (11/10/2026, 00:26 BRT)

**VALIDADO no Xiaomi Android 14, singleplayer:** benchmark `passed=true`, `phase="completed"`, **25.000 callbacks gerenciados** (`OnHitNPC=12500`, `OnKill=12500`), `failures=0`, `dropped=0`, `lockDrops=0`, `elapsedMs=12336`. Média aproximada de 2.026 callbacks/s. A fila de capacidade 64 teve ocupação máxima de **13**; ao final, `queueDepth=0`. O limite foi alcançado, o dispatcher foi desligado normalmente (`limit-reached`) e o usuário confirmou que o Terraria continuou funcionando após a conclusão.

**Limitação da evidência:** o benchmark foi executado pelo editor, que congela o jogo, e gera eventos sintéticos para callbacks C# reais. Não significa que medimos 60 FPS **durante** os 25 mil eventos, nem comprova efeitos nativos ou mods complexos. Próxima fase: latência/FPS/memória no gameplay real e generalização por múltiplos mods. [Protocolo e resultado](managed-live-benchmark-v64.md) · [Release .64](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.64).

## Teste Android da .63 — validada parcialmente em 11/10/2026, 00:01 (BRT)

**APROVADO para execução contínua acima de 512 callbacks com fila FIFO64 no ExampleModCSharp 0.1.7 em singleplayer Android 14.** O teste ultrapassou o antigo limite sem desativar callbacks e o usuário confirmou Terraria a 60 FPS no cenário observado.

| Indicador | Resultado observado |
| --- | --- |
| Executados e aceitos | `processed=1156`, `queued=1156` |
| Eventos por tipo | `hitProcessed=515`, `killProcessed=641` |
| Execução gerenciada | `failures=0`, `automatic=true`, `enabled=true`, `phase=ready` |
| Fila | `queueCapacity=64`, `maxQueueDepth=2`, `queueDepth=0`, `inFlight=false` |
| Descartes | `dropped=12`, `lockDrops=12`: todos atribuídos à contenção do mutex, não à fila cheia |
| MonoVM no processo do jogo | `domainInitialized=true`, `methodExecuted=true`, `result=42`, `phase=verified` |
| Teto de segurança | `eventLimit=25000` configurado; **não atingido nem validado no aparelho** |
| FPS | Usuário relata 60 FPS estáveis durante esse teste; sem perfil temporal p90, CPU ou memória |

**Próximo teste:** benchmark sintético controlado e isolado do gameplay para percorrer 25 mil eventos reais gerenciados rapidamente sem disparos manuais; checar `draining → limit-reached`, contadores e CPU/latência. Não confundir isso com 25 mil impactos reais de combate; eles são cenários diferentes. Preservar a .62 como baseline já validada. [Detalhes da .63](managed-event-queue-v63.md).

## Teste Android v0.1.10.16.62 — validado em 10/10/2026, 22:25 (BRT)

**Resultado: APROVADO para a execução automática gerenciada de `OnHitNPC` e `OnKill` neste ExampleModCSharp 0.1.7, em singleplayer.** O usuário testou no Xiaomi 2109119DG / Android 14 / arm64-v8a, Terraria 1.4.5.8.6. O jogo permaneceu jogável após o encerramento automático dos callbacks no limite.

| Evidência | Resultado |
| --- | --- |
| MonoVM, mesma sessão do jogo | `domainInitialized=true`, `methodExecuted=true`, `result=42`, `phase=verified` |
| Eventos gerenciados habilitados automaticamente | `automatic=true`, `projectileType=1136` |
| Antes do limite | `enabled=true`, `queued=72`, `processed=72`, `hitProcessed=25`, `killProcessed=47`, `dropped=0`, `failures=0` |
| Após o limite | `enabled=false`, `phase=limit-reached`, `queued=512`, `processed=512`, `hitProcessed=187`, `killProcessed=325`, `dropped=9`, `failures=0` |
| Resultado gerenciado final | `lastEvent=OnKill`, `lastDamage=18`, `lastResult=345` |
| Estabilidade relatada | Gameplay permaneceu normal após o limite; o relatório da sessão não indica erro novo. Não houve medição quantitativa de FPS/memória. |

O código tem fila com uma única posição pendente e limite de 512 enfileiramentos. `dropped` contabiliza tentativas recusadas quando há evento pendente e/ou ao atingir o limite, **não** exceções gerenciadas. O limite desabilita o dispatcher por segurança; não deve ser interpretado como travamento ou falha de compilação. `failures=0` significa que nenhuma das 512 execuções processadas falhou no callback.

A validação **não** demonstra IA C# arbitrária por frame, multiplayer, múltiplos mods simultâneos nem compatibilidade universal com mods tModLoader. Recursos não testados continuam experimentais. [Release .62](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.62) · [Registro detalhado](mono-auto-live-world-signal-v62.md).

## Teste Android da .61 — bloqueio de detecção de mundo

No aparelho Xiaomi Android 14 (relatório de 10/10/2026, 21:57), o log confirmou que ExampleBolt foi registrado como tipo 1136 e que a curva C# e as políticas OnHitNPC/OnKill foram instaladas. Entretanto, apareceu `Main.get_gameMenu indisponivel; aguardando estado do mundo`: a ponte gerenciada automática não foi armada. O `result=42` preservado no relatório é histórico. A v.62 substitui a dependência obrigatória do getter por um sinal controlado de mundo carregado + jogador ativo em três quadros, mantendo o gate singleplayer. A `.62` ainda requer teste real. [Notas](mono-auto-live-world-signal-v62.md).

## Diagnóstico posterior — a .60 compilou e aplicou, mas não ativou o MonoVM

No teste das 21:28 (BRT), o worker concluiu `state=finished`, `phase=compile-verified`, `assemblyReady=1`, `executedTypes=7`; o loader registrou `auto.phase=ready`, `applied=true`, `curves=1/1`, `hitPolicies=1/1`, `killPolicies=1/1`, `netMode=0`, **`gameMenu=-1`**. O processo do jogo permaneceu `mono=idle` e `live=idle`.

A revisão revelou que a ponte automática condiciona o início a `gameMenu==0`, mas a .60 procura `Main.gameMenu` como campo, enquanto no Bunny Loader `Main.get_gameMenu()` é a propriedade correta. **O teste da .61 mostrou que nem o getter estava disponível nessa configuração; por isso a .62 adotou o sinal de mundo carregado e jogador ativo.** O worker apresentou um registro de `LOW_MEMORY` após a compilação ter sido verificada; investigar separadamente, sem classificar a compilação como falha.

Veja [hotfix e teste .61](mono-auto-live-menu-getter-v61.md).

## Teste Android inicial da .60 — 10/10/2026, 21:18 (BRT)

**Resultado: parcial / não aprovado ainda.** No Xiaomi 2109119DG, Android 14, com `ExampleModCSharp 0.1.7`, o status no editor mostrou:

- `auto[0].phase = "compiling"`, `cached = false`, `applied = false`; identidade do mod detectada: `68c3115f-62f4-49f2-a56b-388256781d5f`.
- `live.phase = "idle"`, `enabled = false`, `automatic = false`, `hitProcessed = 0`, `killProcessed = 0`, `failures = 0`.
- `mono.phase = "idle"` e `attempted = false` no **processo atual**. O relatório também contém um checkpoint Mono anterior com `phase = verified`, `result = 42` e um checkpoint do worker desta sequência com `phase = compiling-source`. **Não confundir checkpoints anteriores com a sessão atual.**
- `auto.error = ""`; não havia falha de compilação confirmada na leitura. `netMode = -1` e `gameMenu = -1` são valores iniciais do status; no código da .60 só são preenchidos após a compilação.
- O relatório inclui encerramentos históricos (`SINAL status=9` e ocorrência de memória insuficiente no processo `mono_probe`), mas a causa desses registros não pode ser atribuída a este teste sem correlacionar PID/tempo.

O `AutoSourceRuntime::compile` aguarda até **180 segundos** pelo arquivo de status do worker. **Diagnóstico pendente**: `bl.csharp.sourceCompileStatus()` com `status`, `diagnostics`, `workerPid`, `workerAlive`, `staleCompilingState`, `exitInfo` e `exitTrace`, de preferência após aguardar o prazo. Sem isso, não afirmar travamento definitivo nem erro do callback.

Atualização de referência: **10 de outubro de 2026**. Estado ancorado no código da branch `bunny-csharp-source-0.1.10.16.60-auto-live-events`, na release .60, nos testes automatizados e nos resultados de aparelho registrados no histórico do projeto.

## Matriz de validação

| Componente | Implementado | Teste automatizado | Teste real Android |
| --- | --- | --- | --- |
| Integração MonoVM ARM64 | Sim, experimental | Sim | **Confirmado na .58**: modos de diagnóstico, execução de método que retorna 42, sem fechamento no teste relatado |
| Compilação de fonte C# (Roslyn) | Sim, protótipo | Sim | Fluxo testado em exemplos específicos; não é um compilador universal de mods tML |
| Carregamento automático de conteúdo C# | Sim, no escopo do ExampleMod | Sim | Testes anteriores de auto loader confirmados; validade por recurso/versão |
| Projéteis, hit/buff/kill via ponte | Sim, em integrações específicas | Sim | Funcionalidade de exemplo já relatada; efeito pode ser aplicado por política nativa derivada do C# |
| Execução **gerenciada** de `OnHitNPC` | Sim, opt-in na .59 | Sim | **.59: 10/10 eventos**, sem falha registrada |
| Execução gerenciada automática `OnHitNPC` + `OnKill` | Sim, corrigida na .62 | **Sim**, build e verificações | **.62: 512 callbacks reais (187 hits + 325 kills), 0 falhas; limite seguro atingido** |
| Chamadas Mono por frame para AI arbitrária | Não liberado | — | Não validado |
| Múltiplos mods/projéteis com callbacks automáticos simultâneos | Não liberado | — | Não validado |
| Multiplayer com callbacks automáticos C# | Desativado no protótipo | — | Não validado |
| Compatibilidade com qualquer mod de PC/tModLoader | Não | — | Não |

## Restrições da ponte experimental (.60–.62)

- Só inicia o vínculo gerenciado automático em **singleplayer**, depois do carregamento e da compilação do mod.
- Convenção do exemplo: `BunnyLoader.Source.BunnyLiveEvents.OnHitNPC(int,int)` e `OnKill(int,int)`, métodos `public static int`.
- No máximo **um tipo de projétil por processo**, neste estágio.
- Fila de capacidade **1**, até **512 eventos por sessão** e desativação após **3 falhas**.
- A thread do jogo apenas enfileira os dados, não espera a chamada Mono.
- Os callbacks experimentais de .60 contabilizam execução gerenciada; **não demonstram** que qualquer mod C# controla livremente o gameplay. Dano, buffs e velocidades seguem as políticas existentes.
- 60 FPS é um alvo de otimização e precisa ser **medido no aparelho**, não presumido do CI.

## Evidências

- [Release e pacotes da .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60)
- [Build da .60 — GitHub Actions](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38095931817)
- [Documentação original do teste .59](mono-live-hit-onhit-0.1.10.16.59.md)
- [Notas da implementação automática .60](bunny-csharp-auto-managed-events-v0.1.10.16.60.md)

**Próxima evidência necessária:** benchmark de FPS, latência por callback, memória e descartes em sessões prolongadas; avaliação segura dos limites e reinicialização por mundo. O teste .62 dos dois callbacks já foi concluído no Android.
