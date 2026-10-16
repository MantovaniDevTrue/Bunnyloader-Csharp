# Validação Android e evidências

O [estado atual](STATUS.md) registra a `.62` confirmada pelo usuário. Este guia
serve para novos recursos e regressões; não invalida os testes já concluídos.
As suítes reproduzíveis estão em [tests/README.md](../tests/README.md).

## Registrar uma sessão

Informe versão completa/commit do APK, pacote/hash de cada mod, aparelho,
Android/ABI, cenário e resultado esperado/observado. Use APK e mods da mesma
entrega. Guarde tempo de carregamento, cold/warm start, FPS medido, duração,
memória e o relatório do loader quando aplicável. Não invente métricas ausentes.

| Área alterada | Teste no jogo |
| --- | --- |
| Carregamento/cache | Primeiro boot e reentrada; conteúdo pronto antes do mundo; cache frio/quente e recuperação após atualização |
| Itens/receitas | Crafting, consumo, tooltip, inventário, baú, personagem e save/reabertura |
| Equipamento/armaduras | Equipar/remover, vaidade, loadout, morte/respawn, conjunto completo/parcial, asas/gravidade, shield/shoes e rastro |
| Visuais/ABI | Hair tint, manto, animação, Color/alpha e Soul com luz no mundo; metadata inválida deve manter causa observável |
| Player/dano | NPC e projétil; ref/out, esquiva, notificações, vários mods e timeout sem resultado antigo aplicado |
| NPC/projétil C# | Slime e Wave Wand contra alvos; trajetória, dano, hit/kill, reuso de slot, save/saída e outro mundo |
| Independência | Só mods C#; depois convivência com JS; nenhum comando JS necessário para ativar C# |
| Desempenho | Cena comparável, carga real, sessão prolongada, FPS, RAM e percentis de latência |

## Telemetria de combate

Desde `.60`, `BootLog` inclui `entity-combat-status.txt`: sessão, mods,
`queuePending`, `queuedTotal`, `droppedTotal`, `npcAiJobsProcessed` e
`projectileAiJobsProcessed`. Cada evento (`NPC.OnHit`, `NPC.OnKill`,
`Projectile.OnHitNPC`, `Projectile.OnKill`) distingue `queued`, `executed`,
`rejected`, `error`, `unbound`, `dropped` e `stale`.

Confira a sessão e a idade do arquivo: ele pode ser anterior. `queued` não é
sinônimo de `executed`. Rejeições não identificam sua causa sozinhas: podem
envolver geração/owner/sessão, contexto ainda ausente ou fila indisponível.
A `.62` antecipa o primeiro tick sem remover essas proteções. Quantifique a
melhora com novas contagens, não apenas com o registro de um callback.

`NPC.OnHit` observa queda de vida entre atualizações de IA, incluindo dano
contínuo. Os eventos são best-effort; não equivalem a todos os hooks do PC.
Dano síncrono do jogador usa scheduler com prazo e tem métricas próprias.

## Interpretar o relatório

- Separe sessão atual e anteriores. Color antigo em sessão anterior não indica regressão atual.
- `phase=verified` e `result=42` confirmam o probe, não todas as APIs do mod.
- Instalação do pacote, sinal 9 e memória precisam de contexto para explicar encerramento.
- Host com metadados simulados não comprova layout/renderização ARM64 nem FPS.
- Uma confirmação geral de funcionamento não fornece contadores, duração ou FPS ausentes.

Novas evidências entram no status e no checkpoint, com seu alcance. A
[matriz de exemplos](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
precisa de evidência por recurso antes de mudar estados individuais.

[Snapshot anterior de validação](history/snapshot-SDK-VALIDACAO.md)
