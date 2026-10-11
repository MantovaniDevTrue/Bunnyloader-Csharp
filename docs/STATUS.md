# Estado real do desenvolvimento

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
