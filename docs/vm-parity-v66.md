# Bunny Loader C# .66 — QuickJS × MonoVM: mesmo algoritmo, mesma carga

O teste compara uma rotina determinística de inteiros **idêntica** nas linguagens JavaScript e C#, executada no QuickJS do Bunny Loader e no `BunnyManagedProbe.dll` do MonoVM do aplicativo.

## Escopo

- Mesmo estado inicial (`seed=0x12345678`), mesmo número de iterações (padrão 1.000.000), multiplicação de 32 bits, rotações, XOR e mesmo valor final.
- Um aquecimento de 4.096 iterações por runtime antes do trecho cronometrado.
- QuickJS executa no contexto do editor; MonoVM invoca `Probe.BenchmarkMath(int,int)` na sua thread persistente. Nenhum `OnHitNPC` é criado e não altera o ExampleModCSharp 0.1.7.
- Tempo decorrido: `jsWallUs` e `monoWallUs`; CPU da thread: `jsThreadCpuUs` e `monoThreadCpuUs`. `jsOverMonoWallRatio` acima de 1 indica MonoVM mais rápido nesse núcleo específico; abaixo de 1 favorece QuickJS.
- `rssBeforeJsKb`, `rssAfterJsKb`, `rssBeforeMonoKb`, `rssAfterMonoKb` são **memória RSS total do processo Android** — não atribuir todo o delta à linguagem.
- `passed=true` somente quando os inteiros de 32 bits produzidos pelos dois runtimes coincidirem.
- Até quatro medições por processo, entre 1.000 e 5.000.000 de iterações; apenas no mundo singleplayer com ExampleModCSharp auto live pronto.
- O comando não mede frame renderizado, custo da fila FIFO64, IPC entre threads ou desempenho real de mods com IA. Esses cenários são etapas futuras.

## Teste Android

Instalar APK .66 e usar o mesmo ExampleModCSharp 0.1.7. Entre em singleplayer e aguarde `bl.csharp.sourceLiveHitStatus().enabled === true`, sem iniciar o benchmark antigo de 25 mil (que desativa a ponte após a conclusão).

No editor:

```javascript
bl.csharp.vmCompareStart(1000000)
```

Após breve intervalo:

```javascript
JSON.stringify({
  compare: bl.csharp.vmCompareStatus(),
  live: bl.csharp.sourceLiveHitStatus(),
  mono: bl.csharp.managedRuntime()
}, null, 2)
```

Para comparar estabilidade, repetir com `500000`, `1000000` e `2000000` iterações, esperando a execução anterior terminar. De preferência sem outros aplicativos pesados abertos. Observar `phase="matched"`, `passed=true`, checksums iguais, os tempos e ausência de falha/crash.

Um único resultado pode variar por JIT, GC e frequência da CPU. Não declarar vantagem geral sem repetição e sem comparação com gameplay real.

**Estado:** CI e teste Android pendentes. A versão .65 segue preservada e comprovada em jogo.
