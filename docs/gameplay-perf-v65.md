# Bunny Loader C# .65 — perfil de desempenho em gameplay

## Motivação

A .63 executou 1.156 callbacks de combate (515 OnHitNPC + 641 OnKill), sem falhas, mas houve **12 descartes de mutex**; o usuário relatou 60 FPS. A .64 concluiu benchmark sintético de 25.000 callbacks C# em 12.336 ms, sem falhas ou descartes. Falta medir o custo e a regularidade de callbacks durante uma batalha real.

## Mudanças da .65

- `sourcePerfStart()`, `sourcePerfStatus()`, `sourcePerfStop()`: profiler opcional por 15 segundos, sem alterar o limite de 25 mil ou a fila FIFO64.
- A contagem começa somente no primeiro `Main.DoUpdate` após fechar o editor, impedindo que o tempo em que o editor congela o Terraria consuma os 15 segundos da janela.
- `updateHz`: frequência observada de `Main.DoUpdate`, NÃO FPS do renderizador; `updateP90Ms` é uma estimativa do 90º percentil dos intervalos de atualização em buckets de 4 ms.
- `queueWaitAvgUs`/`queueWaitMaxUs`: espera entre enfileiramento e início da execução MonoVM; `callbackAvgUs`/`callbackMaxUs`: custo temporal de invocação C# na thread de execução; `callbacksSampled` informa a amostra.
- `lockDrops`: novas falhas de captura do mutex durante a janela. Se a fila não receber eventos, métricas de callbacks ficam em zero; isso não é erro.
- Otimização segura: o dispatcher evita copiar `modId` e caminho DLL a cada callback, utilizando referências estáveis enquanto `g_liveHitInFlight` protege a sessão; nenhuma alteração de efeitos, registro de itens ou códigos C# do mod de exemplo.

Sem o profiler, os pontos novos usam apenas uma verificação atômica barata e não fazem cálculos de tempo nem alocações. O profiler não mede RSS, CPU ou GPU diretamente. Para desempenho visual, use o indicador de FPS do jogo e envie o relato junto das métricas.

## Teste recomendado no celular

Instale o APK .65 e mantenha `ExampleModCSharp 0.1.7` ativado. Reinicie o app após qualquer benchmark de limite anterior. Entre em singleplayer, aguarde os callbacks automáticos, equipe a ExampleWand e posicione-se perto de inimigos.

1. Abra o editor e execute `bl.csharp.sourcePerfStart()`.
2. Feche o editor imediatamente, movimente-se e use a ExampleWand por **15 a 20 segundos**, observando o FPS mostrado no Terraria.
3. Abra o editor novamente e execute:

```javascript
JSON.stringify({
  perf: bl.csharp.sourcePerfStatus(),
  live: bl.csharp.sourceLiveHitStatus(),
  mono: bl.csharp.managedRuntime()
}, null, 2)
```

O esperado é `perf.phase="completed"`, `updateSamples > 0`, `callbacksSampled > 0` durante combate com ExampleWand, `live.failures=0` e FPS estável. `updateP90Ms` é aproximado e não deve ser confundido com o p90 real do renderer. Se quiser interromper antes de 15 segundos, use `bl.csharp.sourcePerfStop()`.

Uma segunda coleta em condições similares (movimento e inimigos) ajuda a separar variação do cenário de ganho no C#. Testar sem o profiler também, porque ele próprio introduz pequeno custo de medição.

**Estado:** .65 aguarda build e teste Android. Não afirmar que ficou mais rápido até comparar FPS, atrasos e CPU/memória em condições equivalentes.
