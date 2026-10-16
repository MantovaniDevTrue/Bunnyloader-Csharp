# Bunny Loader C# .70 — perfil de desempenho em gameplay real

## O que foi instrumentado

Extensão do profiler .65, janela fixa de **15 segundos**, operando sobre o mundo singleplayer com ExampleModCSharp 0.1.7. Não é teste sintético .64 e **não altera a lógica do mod**. A cada callback real gerenciado `OnHitNPC`/`OnKill`, registra, quando ligado, tempo de fila, duração de execução (com custos do invocador), CPU da thread MonoVM, percentis aproximados p90/p99 e eventos concluídos. `projectileCurveUpdates` e `projectileVelocityWrites` contam somente execuções reais da curva C# que foi preparada a partir do source, no hook nativo do Terraria. Não contam `AI` gerenciada executada em todos os frames.

O ritmo `updateHz`, `updateP90Ms` e `updateP99Ms` mede **intervalos entre atualizações observadas no jogo** via hook já existente da .65; não é medida de FPS renderizado da GPU. Use o indicador visual de FPS do Terraria como dado separado, não invente medição por estimativa.

As métricas `queueWaitP90Us`, `callbackP90Us`, `callbackCpuP90Us` e similares representam **limites superiores aproximados de bins de histograma**, sem alocar vetor por evento. `callbackCpuAvgUs` mede o tempo de CPU desta thread durante a chamada gerenciada, e não o consumo total do app. `eventsAccepted`, `eventsProcessed`, `eventsFailed`, `eventsQueueDropped` e `maxQueueDepth` cobrem apenas a janela ativada. `lockDrops` registra tentativas descartadas por mutex congestionado. A média de espera em fila usa `queueWaitSamples` como denominador.

## Protocolo Android para teste sem alterar mods

Instalar APK .70, entrar em mundo singleplayer com **ExampleModCSharp 0.1.7**, aguardar `sourceLiveHitStatus().enabled === true`.

### Cena 1: referência leve

Em lugar relativamente calmo, abrir Editor e executar:

```javascript
bl.csharp.sourcePerfStart()
```

**Fechar o Editor imediatamente**, porque em tela cheia ele pausa/congela o jogo e invalidaria a amostra. Jogar por ao menos 15 s; usar a arma `ExampleWand` contra alguns inimigos para produzir callbacks reais, observando FPS visual. Quando o período terminar, abrir Editor e executar:

```javascript
JSON.stringify({
  perf: bl.csharp.sourcePerfStatus(),
  live: bl.csharp.sourceLiveHitStatus(),
  curves: bl.csharp.sourceProjectileAIStatus("68c3115f-62f4-49f2-a56b-388256781d5f", "ExampleBolt"),
  npc: bl.csharp.sourceLiveNpcStatus()
}, null, 2)
```

Guarde captura completa dos valores. Se `callbacksSampled=0`, o profiler não mediu callbacks e essa amostra **não valida latência**; atacar com `ExampleWand` enquanto o monitor está ativo, em uma nova janela.

### Cena 2: combate mais intenso

Em uma condição reproduzível, repetir `sourcePerfStart()`, fechar o Editor, jogar 15 s com mais inimigos/projéteis, observar FPS, e recuperar os status. Não precisa nem deve executar `sourceLiveBenchmarkStart` (25 mil eventos artificiais), pois essa etapa é sobre eventos **que ocorrem no gameplay**.

### Campos esperados

- `phase="completed"`, `elapsedMs` próximo de 15.000, `updateSamples>0`.
- `callbacksSampled>0` **somente se houve OnHitNPC/OnKill real enquanto o monitor estava ativo**.
- `eventsProcessed` de acordo com as chamadas concluídas, `eventsFailed=0`, `eventsQueueDropped=0`, `lockDrops=0` na sessão saudável.
- `projectileCurveUpdates>0` se foram disparados projéteis do ExampleMod com curva automática durante o monitor; `projectileVelocityWrites` pode ser inferior se a velocidade não era válida.

`sourcePerfStop()` está disponível para cancelar antes dos 15 s. Não iniciar o profiler com benchmark de VM em andamento.

**Estado:** code review/CI e validação no Android ainda necessários. A .69 permanece baseline para simulações até os testes da .70.
