# Bunny Loader C# .68 — extração de assembly verificada na abertura do jogo

## Falha que motivou o hotfix

No teste Android da .67, o APK continha `assets/managed/BunnyVmBenchmark67.dll` (verificado por CI), mas o método `vmCompareStart(1000000)` terminou com `phase="failed"` e `error="BunnyVmBenchmark67.dll missing or invalid"`.

Os resultados observados foram `jsResult=1336190628`, `jsWallUs=338166` µs, `jsThreadCpuUs=336281` µs, `monoWallUs=0` e `monoThreadCpuUs=0`. O MonoVM geral continua verificado com `result=42`, portanto não há evidência de falha no carregamento do runtime ou benchmark de velocidade C# válido.

## Correção da .68

O código real tinha a lógica de extração e atualização completa do framework em `ManagedProbeService.installManagedFramework`, disparada no processo worker, mas **não na inicialização do processo do jogo**. Uma nova DLL entregue no APK podia não existir em `filesDir/bunny-csharp` até o worker ser executado.

`GameActivity.onCreate` da .68 garante, antes de `startNativeCore()`, que a DLL `BunnyVmBenchmark67.dll` exista na pasta `filesDir/bunny-csharp` usada pelo MonoVM, conferindo SHA-256 contra o `assets/managed/framework.sha256` do APK. Quando a DLL correta já está presente, não reescreve nem recarrega. Quando ausente/desatualizada, copia a partir do APK para arquivo temporário, verifica hash antes/depois da instalação e registra o resultado no log.

Não altera `BunnyManagedProbe.dll`, os algoritmos do benchmark, a compilação de mods C#, nem as proteções e recursos validados anteriormente.

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

**Estado:** compilação e teste no Android pendentes.
