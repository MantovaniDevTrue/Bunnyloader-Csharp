# Bunny Loader C# .72 — Managed Ingress: testes reais no Android

**Estado:** APROVADO nos cenários observados: perfil de 15 segundos e combate prolongado estimado em 7–8 minutos, ambos **sem perdas detectadas**. Saturação, múltiplos mods e comportamento ao atingir o limite ainda não testados.

**Versão:** v0.1.10.16.72 (Managed Ingress) · [release](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.72-managed-ingress).

**Origem da evidência:** três capturas do profiler e uma captura adicional do teste prolongado do editor do Terraria, enviadas pelo testador em 11/10/2026. Valores abaixo transcritos diretamente dos JSONs apresentados. O relato de FPS no teste prolongado é uma observação do jogador. Ambiente de testes anteriores: Xiaomi 2109119DG, Android 14, arm64-v8a, Terraria 1.4.5.8.6, ExampleModCSharp 0.1.7; as capturas desta rodada não repetem a identificação do aparelho ou do APK.

## Janela de desempenho: `bl.csharp.sourcePerfStatus()`

| Campo | Resultado |
| --- | ---: |
| `phase` / `targetMs` / `elapsedMs` | `completed` / `15000` / `15010` |
| `updateSamples` / `updateHz` | 899 / 59.95830396832601 |
| `updateP90Ms` / `updateP99Ms` | 20 / 20 |
| `updatesOver25Ms` / `updatesOver33Ms` | 0 / 0 |
| `callbacksSampled` | 82 |
| `queueWaitAvgUs` / `queueWaitMaxUs` | 77.36585365853658 / 1044 |
| `queueWaitP90Us` / `queueWaitP99Us` | 100 / 1500 |
| `callbackAvgUs` / `callbackMaxUs` | 91.8048780487805 / 483 |
| `callbackP90Us` / `callbackP99Us` | 150 / 500 |
| `callbackCpuAvgUs` / `callbackCpuMaxUs` | 72.8780487804878 / 261 |
| `callbackCpuP90Us` | 150 |
| `eventsAccepted` / `eventsProcessed` | 82 / 82 |
| `eventsFailed` / `eventsQueueDropped` / `lockDrops` | 0 / 0 / 0 |
| `maxQueueDepth` | 2 |
| `projectileCurveUpdates` / `projectileVelocityWrites` | 69 / 69 |

Percentis baseados em buckets aproximados podem apresentar valores arredondados maiores que o máximo medido (por exemplo, `queueWaitP99Us=1500` contra máximo de 1044 µs). `updateHz` é a frequência de `Main.DoUpdate`, **não uma medida direta do FPS de renderização**. Os valores de CPU apresentados são do callback, não de toda a aplicação.

## Contadores acumulados: `bl.csharp.sourceLiveHitStatus()`

| Campo | Resultado |
| --- | --- |
| `enabled` / `automatic` / `phase` | `true` / `true` / `ready` |
| `projectileType` | 1136 |
| `queued` / `processed` | 224 / 224 |
| `hitProcessed` / `killProcessed` | 112 / 112 |
| `dropped` / `lockDrops` / `failures` | 0 / 0 / 0 |
| `queueDepth` / `maxQueueDepth` / `queueCapacity` | 0 / 2 / 64 |
| `eventLimit` / `inFlight` | 25000 / `false` |
| `lastEvent` / `lastDamage` / `lastResult` | `OnKill` / 18 / 134 |
| `error` | `""` |

**Atenção:** 82 eventos pertencem à janela perfilada de 15 segundos; 224 são os contadores acumulados do dispatcher no instante da captura. São escopos distintos, não uma divergência de contagem.

## Teste prolongado no Android — 11/10/2026

O usuário relatou **aproximadamente 7–8 minutos** de uso contínuo do livro de teste contra **3 bosses e 112 inimigos** durante uma **Blood Moon**. A duração foi estimada pelo testador, **não medida pelo profiler**. O usuário confirmou que o indicador de FPS permaneceu em **60 FPS durante todo o teste**; trata-se de observação em gameplay, não de captura frame a frame do renderizador. A captura foi realizada ao final por `JSON.stringify(bl.csharp.sourceLiveHitStatus(), null, 2)`.

| Campo | Resultado |
| --- | --- |
| `enabled` / `automatic` / `phase` | `true` / `true` / `ready` |
| `projectileType` | 1136 |
| `queued` / `processed` | 2897 / 2897 |
| `hitProcessed` / `killProcessed` | 1443 / 1454 |
| `dropped` / `lockDrops` / `failures` | 0 / 0 / 0 |
| `queueDepth` / `maxQueueDepth` / `queueCapacity` | 0 / 3 / 64 |
| `eventLimit` / `inFlight` | 25000 / `false` |
| `lastEvent` / `lastDamage` / `lastResult` | `OnKill` / 18 / 1474 |
| `error` | `""` |
| FPS relatado | **60 FPS continuamente durante a sessão estimada em 7–8 min** |

**Interpretação:** nesta carga real, o dispatcher processou todos os 2897 eventos aceitos, sem registros de perdas, disputa do mutex, falhas gerenciadas ou backlog ao fim da coleta. O pico de 3 posições na FIFO de 64 confirma folga para a carga testada, **não** ausência garantida de perdas sob saturação. Os 60 FPS foram observados pelo usuário durante a sessão inteira; o profiler não registrou medidas contínuas de renderização nesta rodada. Não assumir que '3 bosses e 112 inimigos' equivalem ao número exato de mortes/ações do callback. A contagem 2897 é acumulada na sessão, portanto não representa necessariamente apenas os últimos 7–8 minutos.

## Interpretação e limites

- **Comprovado nesta carga:** todos os 82 eventos aceitos foram processados no intervalo medido; o acumulado mostra 224/224, nenhum descarte por fila, nenhum `lockDrops`, nenhuma falha gerenciada e máximo de duas posições ocupadas na FIFO64.
- A nova arquitetura Managed Ingress foi projetada para reduzir a disputa com o mutex do MonoVM; os contadores zerados **são compatíveis com essa correção**, mas ainda não demonstram que jamais haverá perda em picos extremos, com vários mods, ou ao atingir 25 mil eventos.
- O ritmo de `Main.DoUpdate` ficou em 59,96 Hz e não houve atualização registrada acima de 25 ou 33 ms; é um bom sinal de fluidez durante o teste, **não** uma medida direta de renderização.
- A .71 havia sido relatada como fluida a 60 FPS, mas não há aqui uma janela A/B rigorosamente equivalente. Evitar afirmar ganho percentual de FPS, latência ou memória em relação à .71.
- Não foram coletadas medidas comparativas de RAM/RSS, energia, GC do processo ou FPS de renderização. Os resultados também não validam multiplayer, múltiplos mods nem suporte generalizado a tModLoader/PC.

## Próximas verificações sem mudar o código

1. Repetir o perfil de 15 segundos 3 vezes, em condições de combate similares, registrando `sourcePerfStatus()` e `sourceLiveHitStatus()` em cada rodada.
2. **Executado inicialmente:** combate prolongado estimado em 7–8 minutos com 2897 callbacks e 60 FPS observados; repetir com instrumentação temporal ou carga maior se necessário.
3. Separar carga natural de gameplay de benchmark sintético de 25 mil callbacks, porque o editor pode suspender o jogo enquanto o script roda.
4. Se necessário, comparar a .71 e .72 com o mesmo mundo, mod e carga, incluindo FPS de renderização e RSS quando disponível.

**Conclusão:** testes curtos e prolongado da .72 **aprovados nos cenários observados**, com 2897 eventos cumulativos e 60 FPS sustentados conforme relato. Manter a .72 como candidata estável desta rodada e não alterar a fila até haver evidência de regressão ou necessidade real de otimização.
