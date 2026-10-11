# Bunny Loader C# .68 — extração de assembly verificada na abertura do jogo

## Falha que motivou o hotfix

No teste Android da .67, o APK continha `assets/managed/BunnyVmBenchmark67.dll` (verificado por CI), mas o método `vmCompareStart(1000000)` terminou com `phase="failed"` e `error="BunnyVmBenchmark67.dll missing or invalid"`.

Os resultados observados foram `jsResult=1336190628`, `jsWallUs=338166` µs, `jsThreadCpuUs=336281` µs, `monoWallUs=0` e `monoThreadCpuUs=0`. O MonoVM geral continua verificado com `result=42`, portanto não há evidência de falha no carregamento do runtime ou benchmark de velocidade C# válido.

## Correção da .68

O código real tinha a lógica de extração e atualização completa do framework em `ManagedProbeService.installManagedFramework`, disparada no processo worker, mas **não na inicialização do processo do jogo**. Uma nova DLL entregue no APK podia não existir em `filesDir/bunny-csharp` até o worker ser executado.

`GameActivity.onCreate` da .68 garante, antes de `startNativeCore()`, que a DLL `BunnyVmBenchmark67.dll` exista na pasta `filesDir/bunny-csharp` usada pelo MonoVM, conferindo SHA-256 contra o `assets/managed/framework.sha256` do APK. Quando a DLL correta já está presente, não reescreve nem recarrega. Quando ausente/desatualizada, copia a partir do APK para arquivo temporário, verifica hash antes/depois da instalação e registra o resultado no log.

Não altera `BunnyManagedProbe.dll`, os algoritmos do benchmark, a compilação de mods C#, nem as proteções e recursos validados anteriormente.

## Resultado Android da .68 — paridade QuickJS × MonoVM validada

**VALIDADO no Android, carga de 1.000.000 de iterações, mesmo seed e checksum:** capturas do editor mostram `compare.finished=true`, `passed=true`, `phase="matched"`, `error=""`; o MonoVM conseguiu carregar e executar `BunnyVmBenchmark67.Kernel.Execute(int,int)` na .68. O probe principal permaneceu verificado (`result=42`).

| Métrica | QuickJS | MonoVM |
| --- | ---: | ---: |
| Cálculo determinístico, 1.000.000 iterações | `jsResult=1336190628` | `monoResult=1336190628` |
| Tempo decorrido | `jsWallUs=445536` (445,536 ms) | `monoWallUs=6040` (6,040 ms) |
| Tempo CPU da thread | `jsThreadCpuUs=439607` (439,607 ms) | `monoThreadCpuUs=5798` (5,798 ms) |
| RSS de todo o processo antes/depois | 759.780 → 758.728 KB | 758.728 → 758.728 KB |

Razão observada nesse **núcleo matemático isolado e nesta execução**: **73,764× menor tempo decorrido** no MonoVM (`jsOverMonoWallRatio=73.76423841059602`) e **75,820× menor tempo de CPU da thread** (`jsOverMonoCpuRatio=75.82045532942394`).

**Limitações essenciais:** esta é uma única amostra, medida em threads distintas, com runtimes, intérprete/JIT e estratégias de otimização diferentes; não constitui superioridade geral de C# em 74×, medição de FPS, custo de integração nativa, uso de memória exclusivo de cada linguagem ou comportamento de mods grandes. A queda de ~1 MB de RSS no QuickJS é ruído do processo inteiro, não uma prova de economia de memória do MonoVM. Não há medição de CPU/RSS em combate simultâneo. Próximas etapas: repetir 500.000/1.000.000/2.000.000 iterações, conferir integridade e dispersão das amostras e então comparar cenários de gameplay equivalentes.

## Segunda amostra Android — 500.000 iterações (11/10/2026)

O mesmo APK .68 produziu **`finished=true`, `passed=true`, `phase="matched"`, `error=""`** em uma segunda carga, 500 mil iterações, com checksums **idênticos** `jsResult=monoResult=-799234484`.

| Métrica | QuickJS | MonoVM |
| --- | ---: | ---: |
| Tempo decorrido | `jsWallUs=171763` (171,763 ms) | `monoWallUs=2462` (2,462 ms) |
| CPU da thread | `jsThreadCpuUs=170302` (170,302 ms) | `monoThreadCpuUs=2275` (2,275 ms) |
| RSS total do processo antes/depois | 225.816 → 226.088 KB | 227.020 → 227.020 KB |

Razões da segunda amostra: **69,7656× em tempo decorrido** e **74,8580× em tempo de CPU por thread**, a favor do MonoVM, **somente para essa rotina matemática**. A amostra de 1 milhão anterior indicou 73,7642×/75,8205×. Ambas têm resultado correto, porém são duas cargas diferentes, e não repetições controladas suficientes para significância estatística. As diferenças de RSS entre capturas são de condições do processo e não estabelecem custo de memória de uma linguagem. Continua pendente testar 2 milhões de iterações e múltiplas execuções de cada tamanho, sob condições semelhantes.

## Testar Android

Atualizar para o APK .68 e entrar em um mundo singleplayer com ExampleModCSharp 0.1.7. Consultar o log de boot procurando:

`csharp benchmark: BunnyVmBenchmark67.dll installed and SHA-256 verified`

ou:

`csharp benchmark: BunnyVmBenchmark67.dll cached and SHA-256 verified`

Depois executar:

```javascript
bl.csharp.vmCompareStart(1000000)
```

Em seguida:

```javascript
JSON.stringify({
  compare: bl.csharp.vmCompareStatus(),
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus()
}, null, 2)
```

O resultado correto exige `compare.phase="matched"`, `passed=true`, `monoWallUs>0` e `jsResult===monoResult===1336190628`. Se o erro mudar para abertura de DLL, classe ou método, enviar o texto exato; isso indicará que a instalação avançou mas existe outro problema independente.

Mesmo com resultado coincidente, o teste representa apenas um kernel de matemática isolado, não FPS ou desempenho geral de mods.

**Estado:** build e teste real de paridade no Android **aprovados**. A `.68` eliminou a falha de localização da DLL e ambas as linguagens retornaram checksum idêntico; comparação ainda limitada ao cálculo isolado e a uma única amostra.
