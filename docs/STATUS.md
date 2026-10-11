# Estado real do desenvolvimento

## Diagnóstico posterior — a .60 compilou e aplicou, mas não ativou o MonoVM

No teste das 21:28 (BRT), o worker concluiu `state=finished`, `phase=compile-verified`, `assemblyReady=1`, `executedTypes=7`; o loader registrou `auto.phase=ready`, `applied=true`, `curves=1/1`, `hitPolicies=1/1`, `killPolicies=1/1`, `netMode=0`, **`gameMenu=-1`**. O processo do jogo permaneceu `mono=idle` e `live=idle`.

A revisão revelou que a ponte automática condiciona o início a `gameMenu==0`, mas a .60 procura `Main.gameMenu` como campo, enquanto no Bunny Loader `Main.get_gameMenu()` é a propriedade correta. **A .61 corrige essa consulta; ainda requer teste Android.** O worker apresentou um registro de `LOW_MEMORY` após a compilação ter sido verificada; investigar separadamente, sem classificar a compilação como falha.

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
| Execução gerenciada automática `OnHitNPC` + `OnKill` | Sim, na .60 | **Sim**: workflow de build e verificações da thread | **Pendente na .60** |
| Chamadas Mono por frame para AI arbitrária | Não liberado | — | Não validado |
| Múltiplos mods/projéteis com callbacks automáticos simultâneos | Não liberado | — | Não validado |
| Multiplayer com callbacks automáticos C# | Desativado no protótipo | — | Não validado |
| Compatibilidade com qualquer mod de PC/tModLoader | Não | — | Não |

## Restrições da .60

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

**Próxima evidência necessária:** relatório `bl.csharp.sourceLiveHitStatus()` da **.60 no Android**, com `hitProcessed > 0`, `killProcessed > 0`, `failures = 0`, e observação de estabilidade/FPS.
