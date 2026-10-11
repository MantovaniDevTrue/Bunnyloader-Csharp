# Bunny Loader C# .72 — Managed Ingress: primeiro teste real no Android

**Estado:** APROVADO no cenário observado, **sem perdas detectadas**, em coleta de 15 segundos; testes de saturação e longa duração ainda pendentes.

**Versão:** v0.1.10.16.72 (Managed Ingress) · [release](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.72-managed-ingress).

**Origem da evidência:** três capturas de tela do editor do Terraria enviadas pelo testador em 11/10/2026. Valores abaixo transcritos diretamente do JSON apresentado. Ambiente de testes anteriores: Xiaomi 2109119DG, Android 14, arm64-v8a, Terraria 1.4.5.8.6, ExampleModCSharp 0.1.7; as capturas desta rodada não repetem a identificação do aparelho ou do APK.

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

## Interpretação e limites

- **Comprovado nesta carga:** todos os 82 eventos aceitos foram processados no intervalo medido; o acumulado mostra 224/224, nenhum descarte por fila, nenhum `lockDrops`, nenhuma falha gerenciada e máximo de duas posições ocupadas na FIFO64.
- A nova arquitetura Managed Ingress foi projetada para reduzir a disputa com o mutex do MonoVM; os contadores zerados **são compatíveis com essa correção**, mas ainda não demonstram que jamais haverá perda em picos extremos, com vários mods, ou ao atingir 25 mil eventos.
- O ritmo de `Main.DoUpdate` ficou em 59,96 Hz e não houve atualização registrada acima de 25 ou 33 ms; é um bom sinal de fluidez durante o teste, **não** uma medida direta de renderização.
- A .71 havia sido relatada como fluida a 60 FPS, mas não há aqui uma janela A/B rigorosamente equivalente. Evitar afirmar ganho percentual de FPS, latência ou memória em relação à .71.
- Não foram coletadas medidas comparativas de RAM/RSS, energia, GC do processo ou FPS de renderização. Os resultados também não validam multiplayer, múltiplos mods nem suporte generalizado a tModLoader/PC.

## Próximas verificações sem mudar o código

1. Repetir o perfil de 15 segundos 3 vezes, em condições de combate similares, registrando `sourcePerfStatus()` e `sourceLiveHitStatus()` em cada rodada.
2. Executar combate prolongado de 5–10 minutos, inclusive boss e muitos projéteis, e conferir `dropped`, `eventsQueueDropped`, `lockDrops`, `failures`, pico da fila e estabilidade percebida de FPS.
3. Separar carga natural de gameplay de benchmark sintético de 25 mil callbacks, porque o editor pode suspender o jogo enquanto o script roda.
4. Se necessário, comparar a .71 e .72 com o mesmo mundo, mod e carga, incluindo FPS de renderização e RSS quando disponível.

**Conclusão:** primeiro teste Android da .72 **aprovado no cenário observado**. Manter a .72 como candidata estável desta rodada e não alterar a fila até haver evidência de regressão ou necessidade real de otimização.
