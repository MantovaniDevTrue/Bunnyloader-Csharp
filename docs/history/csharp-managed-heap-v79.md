# Bunny Loader C# .79 — estatísticas opcionais de heap MonoVM e reutilização de callbacks

**Estado:** [GitHub Actions #38163553681](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38163553681) **success**, [APK .79 publicada](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.79-managed-heap) (versionCode 104, 223.295.337 bytes). **Android validado para os recursos testados (ver resultados abaixo).** **Referência anterior:** .78 (5 entradas, 4 saídas, cache C# estável em 3 entradas/2 caminhos de DLL, 60 FPS relatados e 104/104 eventos na captura). A .78 permanece como rollback.

## Validação Android .79 — uma arma utilizada (11/10/2026)

O usuário esclareceu que **usou apenas uma arma** durante o teste, a que gera os callbacks de `SecondBolt`. Portanto, é **esperado** que `ExampleWaveBolt` tenha zero eventos e que o cache observado contenha **uma entrada e um caminho de DLL**, embora ambos os mods C# apareçam em `ready`. Isso **não representa falha ou ausência de carregamento do outro mod**.

**Resultado funcional:** o usuário observou **60 FPS em todos os testes**; 3 entradas e 2 saídas do mundo, `world.epoch=5`, `rebindSchedules=3`, dois mods `ready`, `configuredBindings=2`, 3 tipos de projéteis vinculados e ativos e worker TID 11711. Na captura, `SecondBolt` processou **367/367 eventos**, com 176 hits e 191 kills, `dropped=0`, `lockDrops=0`, `failures=0`, pico de fila 2/64 e estado `ready`.

| Telemetria MonoVM | Capturas |
| --- | --- |
| `monoGcStatsAvailable` | `true` |
| `monoGcHeapBytes` | 21495808 (aprox. 20,5 MiB), constante |
| `monoGcUsedBytes` | 4199632 (aprox. 4,0 MiB), constante |
| `liveMethodCacheHits` | 641 → 938 → 1305 |
| `liveMethodCacheMisses` | 2 → 2 → 2 |
| `liveAssemblyOpenAttempts/Successes` | 2/2, constantes |
| `cachedCallbackEntries` / `cachedAssemblyPaths` | 1 / 1, esperados para a arma usada |
| `boundProjectileTypes` / `activeProjectileTypes` | 3 / 3 |

No snapshot de memória da terceira entrada: RSS **350320 KiB**, swap **434856 KiB**. Outras capturas mostram mudanças grandes de RSS e swap por gerência de páginas do Android. O usuário refez `sourceMemoryBaseline()` em momentos diferentes, portanto não devemos interpretar os `rssDeltaKb` como uma série com baseline única. A estabilidade observada da heap **não prova ausência de vazamento de longo prazo**.

**Conclusão:** a .79 foi validada no Android para telemetria gerenciada disponível, reutilização de métodos de **uma arma** entre mundos e processamento sem perda de eventos a 60 FPS. Para comprovar reutilização de callbacks de ambas as DLLs numa mesma sessão, basta disparar também com uma arma do primeiro mod e capturar o status; isso **não é necessário para aceitar o teste atual**. A .78 permanece como fallback. As correções do [PR #15](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/15) ainda não estão incluídas no APK .79 testado.

## Objetivo da .79

Além dos dados de memória **do processo inteiro** registrados na .78, `bl.csharp.sourceMemoryStatus()` e `bl.csharp.sourceMemoryBaseline()` apresentam, quando o runtime disponibiliza as funções necessárias, dados da heap do coletor do Mono e o reaproveitamento dos métodos C#.

| Campo novo | Significado |
| --- | --- |
| `monoGcStatsAvailable` | Indica se `mono_gc_get_heap_size` e `mono_gc_get_used_size` foram resolvidas e a VM está pronta |
| `monoGcHeapBytes` | Tamanho da heap controlada pelo GC da MonoVM, em bytes, **não** RAM total ou tamanho do domínio |
| `monoGcUsedBytes` | Estimativa de bytes usados na heap de GC, em bytes |
| `liveMethodCacheHits` | Invocações OnHitNPC/OnKill em métodos C# previamente resolvidos |
| `liveMethodCacheMisses` | Tentativas de resolução inicial de OnHitNPC/OnKill |
| `liveAssemblyOpenAttempts` | Quantidade de chamadas a `mono_domain_assembly_open` para resolver esses callbacks |
| `liveAssemblyOpenSuccesses` | Quantidade dessas chamadas que retornaram um ponteiro válido |
| `distinctLiveOpenedAssemblyPaths` | Quantos caminhos de DLL diferentes foram abertos com sucesso pelo dispatcher |

**Precisão:** hits de cache indicam reutilização de ponteiros de métodos já resolvidos. Não representam por si só dados do loader interno da MonoVM, compilação JIT por chamada, número total de assemblies carregadas ou liberação de memória. Os contadores são acumulados **desde que o processo foi iniciado**, não zeram a cada mundo. Funções opcionais não existentes resultam em `monoGcStatsAvailable=false`; heap e used zerados nesse caso **não são medidas reais**.

Os números de RSS (`rssKb`) e swap (`swapKb`) continuam mostrando o **processo inteiro**, não apenas o MonoVM; a soma de RSS + swap ou heap do Mono não deve ser interpretada como consumo exclusivo do C#.

## Alterações técnicas

- Símbolos `mono_gc_get_heap_size` / `mono_gc_get_used_size` resolvidos via `dlsym` depois do carregamento de `libmonosgen-2.0.so`. A ausência deles **não bloqueia** inicialização do MonoVM.
- Consulta da heap com proteção existente de `g_mutex`, apenas sob solicitação do editor; nunca por frame.
- Contadores no dispatcher `invokeLiveSourceEvent`, que já executava sob `g_mutex`. Nenhum novo lock por evento, thread nova, GC forçado ou `mono_domain_unload`.
- Fila FIFO64, limite total de 25.000 eventos, dois mods C#, pausa e retomada, reinício de mundo, sprites, sons e efeitos mantidos.

## Teste Android — comparar crescimento real

Instalar a .79 por cima da .78, sem limpar dados. Continuar usando ExampleModCSharp 0.1.9 e SecondExampleModCSharp 0.1.0.

1. Entre no mundo, espere os dois mods em `ready` e ataque alguns inimigos com as duas armas para preencher os caches.
2. Execute a referência inicial no editor:

```javascript
JSON.stringify(bl.csharp.sourceMemoryBaseline(), null, 2)
```

3. Ataque aproximadamente 20–40 inimigos, saia do mundo para o menu e volte ao mesmo mundo **quatro vezes**, sem fechar o aplicativo.
4. No primeiro, terceiro e quinto carregamentos, capture:

```javascript
JSON.stringify({
  memoria: bl.csharp.sourceMemoryStatus(),
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

5. Comparar `monoGcStatsAvailable`, `monoGcUsedBytes`, `monoGcHeapBytes`, `liveMethodCacheHits/Misses`, `liveAssemblyOpenAttempts/Successes`, `distinctLiveOpenedAssemblyPaths`, `cachedCallbackEntries`, `world.worldLoads` e FPS. Espera-se que os hits aumentem com os ataques, enquanto misses e tentativas de abertura não cresçam a cada reentrada com as mesmas armas.
6. Pausar e retomar o segundo mod no mesmo mundo; verificar que o callback volta a funcionar sem crescimento inesperado de miss/open.

**Não reinicie o aplicativo entre as amostras**; os contadores de cache começam do zero com um novo processo. Se a API do GC não estiver disponível, ainda há dados válidos de cache, RSS e ciclo de vida para examinar.

## Critérios e limites

**Critérios:** MonoVM e dois mods prontos, nenhum erro, filas drenadas, callbacks ativos, `lockDrops=0` e FPS estável, sem crescimento contínuo de cache ao alternar mundos. Aumento pontual de memória não demonstra vazamento; comparar tendência com atividade semelhante.

**Ainda não implementado:** unload real de assemblies, GC manual, troca de mods ativos em tempo real, medição completa da memória nativa da MonoVM, multiplayer. A .79 foi validada para os diagnósticos e eventos efetivamente testados; a .78 permanece como fallback.
