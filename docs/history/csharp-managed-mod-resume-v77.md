# Bunny Loader C# .77 — pausa e retomada no mesmo mundo

**Estado:** APK assinada, [CI #38157993987](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38157993987) **success** e validação Android **aprovada** para pausa/retomada de callbacks de dois mods no mesmo mundo, a 60 FPS relatados pelo usuário. A **.77 passa a ser baseline funcional do cenário testado**; .76 preservada para rollback.

## Validação Android da .77 — pausa e retomada no mesmo mundo (11/10/2026)

**APROVADA funcionalmente no cenário testado.** O usuário relatou **60 FPS durante todo o processo, sem quedas**, com ExampleModCSharp 0.1.9 e SecondExampleModCSharp 0.1.0 habilitados. Capturas do editor mostram o comando `sourceLiveModPause("2e508d4f-f192-4a21-83b2-496e6c482c75", "CONFIRM_PAUSE_MOD")` retornando `{"paused":true,"error":""}` e, **sem sair do mundo**, `sourceLiveModResume(..., "CONFIRM_RESUME_MOD")` retornando `{"resumed":true,"error":""}`. Depois da retomada, ambos os mods estão `phase=ready`, `cached=true`, `applied=true`, `worldReady=true` e sem erro; `SecondBolt.enabled=true`.

| Contadores da captura final | Valor |
| --- | --- |
| Globais `queued/processed` | **379 / 379** |
| Globais hit/kill | 219 / 160 |
| Globais `dropped/lockDrops/failures` | **0 / 0 / 0** |
| Fila `queueDepth/maxQueueDepth/queueCapacity` | 0 / 3 / 64 |
| `inFlight/phase` | false / ready |
| ExampleWaveBolt (mod principal, tipo 1136) | **195/195**, 128 hits, 67 kills, `lastResult=2091`, 0 falhas, 0 descartes |
| SecondBolt (mod adicional, tipo 1138) | **135/135**, 67 hits, 68 kills, `lastResult=4085`, `enabled=true`, 0 falhas, 0 descartes |
| ExampleBolt (principal, tipo 1137) | **49** eventos, 24 hits, 25 kills, inferidos por subtração dos totais globais |
| FPS | **60 durante toda a sequência**, conforme observação do usuário (sem trace de frametimes) |

**Interpretação:** os 195 e 135 eventos adicionais já estão incluídos nos 379 globais. O retorno 4085 confirma execução de callback próprio da segunda assembly e não apenas uma ativação aparente. A sequência de pausa e retomada é diretamente demonstrada pelos retornos do editor; a captura final não contém dois snapshots do contador `processed` com diferença temporal para medir apenas os novos eventos após o comando, mas o usuário confirmou sucesso no jogo.

**Decisão:** promover a .77 à baseline funcional Android dos dois mods C# com pausa e retomada no mesmo mundo, com a .76 preservada para rollback. Isso **não valida** hot-unload real da assembly, liberação de memória, alterações de mods ativos pelo gerenciador, multiplayer, benchmark de RSS/p90 ou stress prolongado.

## Objetivo

Pausar os callbacks de um mod C# **adicional**, reativá-los no **mesmo mundo**, e continuar processando o outro mod com a mesma FIFO64 e worker MonoVM. Isso não envolve descarga da assembly gerenciada, retirada de itens do Terraria ou ativação/desativação de mods no gerenciador.

## Mudanças

- `sourceLiveModPause(modId, "CONFIRM_PAUSE_MOD")` permanece, mas agora pausa o tipo no **mesmo mutex da fila de eventos**, descartando os eventos pendentes apenas daquele vínculo. Isso impede que um produtor antigo insira eventos atrás do descarte e os faça reaparecer após uma reativação rápida.
- O novo `sourceLiveModResume(modId, "CONFIRM_RESUME_MOD")` reativa todos os vínculos pausados do mod adicional, **somente se** a sessão primária ainda estiver ativa, não houver limite de eventos atingido, e a identidade original dos vínculos e assemblies estiver preservada.
- A retomada é recusada se o mod já está ativo, se houver um vínculo em estado parcial ou com falhas, ou se a sessão foi encerrada. A reentrada no mundo da .76 permanece o modo seguro de recuperar uma sessão expirada.
- Os contadores de `sourceLiveProjectileStatus()` são mantidos durante pausa/retomada no mesmo mundo. Os eventos **intencionalmente removidos** aumentam `dropped`; isso é diferente de perda por fila cheia ou `lockDrops`.
- Não foram alterados os mods de teste, o movimento das armas, os efeitos ou o limite de 25.000 eventos compartilhados.

## Teste no Android (singleplayer)

1. Instale a APK .77 por cima da .76, sem apagar dados. Ative `ExampleModCSharp 0.1.9` e `SecondExampleModCSharp 0.1.0`.
2. Entre no mundo e use `ExampleWand`, `ExampleWaveWand` e `SecondWand` contra inimigos. Verifique ambos os mods `ready` e FPS.
3. Com os projéteis encerrados e a fila vazia, execute:

```javascript
bl.csharp.sourceLiveModPause(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_PAUSE_MOD"
)
```

4. Confirme `{"paused":true,"error":""}` e `SecondBolt.enabled=false`. Atire com a arma do **primeiro** mod e confirme que continua processando callbacks.
5. **Sem sair do mundo**, execute:

```javascript
bl.csharp.sourceLiveModResume(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_RESUME_MOD"
)
```

6. Confirme `{"resumed":true,"error":""}` e `SecondBolt.enabled=true`, depois ataque inimigos com `SecondWand` e observe **novos aumentos** de `processed` e `lastResult` na faixa 3000+ ou 4000+.
7. Repita a pausa e a retomada algumas vezes, sem sair do mundo, e depois faça também um teste de salvar/sair/reentrar.
8. Captura resumida:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  geral: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

**Critérios de aprovação:** os dois mods `ready`, secundário alternando `enabled` falso/verdadeiro, callbacks do primeiro nunca interrompidos, `processed` do segundo voltando a subir depois da retomada, nenhum erro de MonoVM, nenhum `lockDrops`, jogo fluido e FPS estável.

## Limites

Esta versão **não** oferece hot-unload, limpeza real de memória de DLL, multiplayer ou troca de mods ativos em tempo real. O comando atua **apenas** nos vínculos de callbacks adicionais já registrados. A .77 foi validada no Android no cenário acima. A .76 permanece como rollback; unload real de assembly continua fora do escopo.
