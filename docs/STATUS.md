# Estado real do desenvolvimento

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
