# Bunny Loader C# .80 — profiler opcional por arma / projétil

**Estado:** branch `csharp-per-projectile-profiler-v80-experiment` criada em cima da **.79.1 validada no Android**. A .79.1 continua sendo a baseline e rollback até a nova APK passar em testes reais.

## Primeiro resultado Android — 11/10/2026

**Validação inicial aprovada para a instrumentação individual.** Quatro capturas do editor, três armas C# durante uma janela de 15 segundos.

- phase=completed, updateHz=59,77559069 (atualizações/s, não FPS visual).
- callbacksSampled=85, eventsProcessed=85, eventsFailed=0, eventsQueueDropped=0, lockDrops=0.
- Três vínculos enabled=true: tipos 1137, 1136 e 1138; callbacks por vínculo 23+35+27=85; hits + kills = total em cada vínculo.

| Tipo | Amostras | Hits/kills | Fila média/máx. (µs) | Callback médio/máx. (µs) | P90 wall (faixa, µs) | CPU média/máx. (µs) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1137 (BunnyLiveEvents principal) | 23 | 11 / 12 | 30,43 / 208 | 71,78 / 318 | 200 | 67,04 / 314 |
| 1136 (ExampleWaveBoltLiveEvents) | 35 | 18 / 17 | 38,63 / 377 | 148,63 / 2421 | 200 | 87,4 / 863 |
| 1138 (BunnyLiveEvents segundo mod) | 27 | 13 / 14 | 68,74 / 1119 | 97,78 / 1380 | 75 | 76,89 / 878 |

**Interpretação:** os picos isolados elevam as médias e podem fazer média > P90. Amostras pequenas e cargas diferentes não permitem atribuir a diferença apenas ao código C#.

**FPS visual confirmado pelo usuário:** 60 FPS durante a medição, com apenas uma queda momentânea para 59 FPS. Uma oscilação isolada de 1 FPS não é evidência de regressão.

**Estado:** o profiler individual da .80, os callbacks das três armas e o FPS visual durante a janela instrumentada estão validados neste teste; ainda faltam desempenho com profiler desligado, pausa/retomada, reentrada e Blood Moon prolongada na .80. **.79.1 segue baseline e rollback.**
## Finalidade

O profiler `sourcePerfStart()`, `sourcePerfStatus()` e `sourcePerfStop()` já mediam uma janela de aproximadamente 15 segundos no total do C#. A .80 adiciona ao mesmo retorno o vetor `projectiles`, com as medições de cada registro de callback independentemente (incluindo as três armas de dois mods). Nenhum comando novo é obrigatório para obter esses dados.

Cada entrada possui `modId`, `callbackClass`, `projectileType`, `enabled`, `callbacksSampled`, `hitSamples`, `killSamples`, `failures`, `queueWaitSamples`, `queueWaitAvgUs`, `queueWaitMaxUs`, `callbackAvgUs`, `callbackMaxUs`, `callbackP90Us`, `callbackCpuAvgUs`, `callbackCpuMaxUs`.

- **Fila:** espera por evento antes do começo do callback, em microssegundos; amostras de espera podem ser menores que amostras de callback.
- **Wall time:** tempo entre o começo e o fim do callback, incluindo a ponte MonoVM e agendamentos da thread, em microssegundos.
- **Thread CPU:** tempo aproximado de CPU consumido pela thread no callback, em microssegundos. Não é a soma do consumo de todo o jogo.
- **P90:** limite da faixa de histograma que engloba ao menos 90% das amostras, não uma medição exata; os bins existentes do profiler são reaproveitados.
- **enabled:** estado do vínculo no instante em que se abre o relatório, e **não** significa que o vínculo esteve ativo durante toda a janela.
- **Falhas:** eventos cuja chamada terminou sem `ready`; pausas que rejeitam eventos antes do callback não são falhas de callback.
- Os números por vínculo são zerados em cada `sourcePerfStart()`; as estatísticas de `sourceMemoryStatus()` têm escopo diferente, por processo.

## Instrumentação

A medição por projétil é **somente opt-in**: acumula em contadores atômicos já que a janela de perfilamento está ativa, no worker que já atende a fila. Usa o slot do vínculo (`0` principal; `1`–`7` adicionais), sem lookup por string em todo evento, sem alterar FIFO64, sem criar thread nova, sem timer por frame adicional, sem GC forçado, hot-unload ou alteração do conteúdo dos mods.

**Possível custo:** profiler ligado sempre acrescenta leituras de relógio e contadores atômicos à execução medida. `sourcePerfStatus()` coleta metadados sob o mutex existente, apenas quando o usuário consulta, e não deve ser chamado a cada frame.

## Teste Android

Instalar a .80 sobre a .79.1 sem limpar os dados. Manter ExampleModCSharp 0.1.9 e SecondExampleModCSharp 0.1.0.

1. Entrar no mundo singleplayer e confirmar os dois mods em `ready`.
2. Iniciar a medição em um momento em que for possível disparar **as três armas dentro dos mesmos 15 segundos**:

```javascript
bl.csharp.sourcePerfStart()
```

3. Fazer disparos com as três armas durante os próximos 15 segundos, registrando FPS e efeitos. Depois:

```javascript
JSON.stringify(bl.csharp.sourcePerfStatus(), null, 2)
```

4. Esperado: três entradas em `projectiles` (tipos 1137, 1136 e 1138), idealmente cada uma com `callbacksSampled > 0`, `hitSamples + killSamples == callbacksSampled`, soma das amostras dos projéteis igual ao total, `eventsFailed=0`, `eventsQueueDropped=0`, `lockDrops=0` e FPS próximo à baseline anterior.
5. Se não for possível usar as três dentro dos 15 segundos, não é falha: basta repetir a medição uma vez para as outras armas. `callbacksSampled=0` só indica que não houve callback desse vínculo dentro da janela.

**Critérios de avanço:** nenhum erro de compilação/execução, sem regressão de FPS com profiler desligado, contador por projétil consistente com o agregado, diferenças de tempo reais por arma mensuráveis, preservação da pausa/retomada e reentrada. A .79.1 permanece rollback.

**Limitações:** não separa custo da lógica C# do overhead da ponte Mono, não estima o FPS do jogo por arma isoladamente, não prova ausência de vazamentos e não mede callbacks de NPCs/armaduras que não passam por esse dispatcher. Não usar para comparar desempenho absoluto C# vs QuickJS sem cargas equivalentes.
