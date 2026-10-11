# Bunny Loader C# .67 — benchmark com assembly gerenciada dedicada

## Problema confirmado na .66

O teste no Xiaomi Android 14, com ExampleModCSharp 0.1.7, executou 1.000.000 iterações no QuickJS (341.098 microssegundos, CPU 338.308 microssegundos), checksum `1336190628`. O MonoVM não localizou `BunnyManagedProbe.Probe.BenchmarkMath(int,int)`, embora sua inicialização `Run(19,23)=42` tenha funcionado. Isso prova uma falha na resolução do método de teste, não uma falha geral do C# e **não constitui comparação de velocidade entre runtimes**.

## Mudanças

- Nova `BunnyVmBenchmark67.dll`, assembly com identidade separada e classe `BunnyLoader.VmBenchmark67.Kernel`. Método público estático `Execute(int iterations, int seed)` implementa a mesma sequência de operações de 32 bits que o QuickJS.
- Compilação e validação real via reflexão .NET durante GitHub Actions: invoca o DLL resultante com 1 milhão de iterações e compara com checksum `1336190628`. Verificação explícita do APK por `unzip -l` e manifesto SHA-256.
- MonoVM continua usando a thread persistente; arquivo carregado do mesmo diretório verificado `assets/managed` onde o probe original é instalado pelo APK.
- Erros separados: DLL ausente, assembly não abre, classe não encontra, método não encontra ou falha ao executar.
- Não altera gameplay, ExampleModCSharp 0.1.7, FIFO64, teto de 25.000 nem .65.

A causa exata da ausência de `BenchmarkMath` na .66 em runtime não foi comprovada exclusivamente pelos screenshots. O novo identificador de assembly e a validação binária eliminam dois caminhos possíveis de erro; somente o teste em Android confirma a resolução.

## Procedimento de teste

Instale o APK .67 quando o GitHub Actions concluir, mantendo ExampleModCSharp 0.1.7. Entre num mundo singleplayer, aguarde `bl.csharp.sourceLiveHitStatus().enabled === true` e execute no editor:

```javascript
bl.csharp.vmCompareStart(1000000)
```

Após a execução:

```javascript
JSON.stringify({
  compare: bl.csharp.vmCompareStatus(),
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus()
}, null, 2)
```

Critérios: `phase="matched"`, `passed=true`, `jsResult===monoResult===1336190628`, `monoWallUs>0` e `monoThreadCpuUs>0`, sem falhas de callbacks. Depois repetir com 500.000 e 2.000.000 iterações para comparar resultados sem confundir aquecimento/CPU com superioridade geral de linguagem.

**Resultado Android da .67:** a compilação do GitHub passou, mas a comparação falhou com `BunnyVmBenchmark67.dll missing or invalid`. QuickJS executou 1.000.000 iterações em `338166` µs, checksum `1336190628`, MonoVM continuou verificado (`result=42`) e não executou o kernel. A .68 corrige a extração durante `GameActivity.onCreate` e ainda precisa de validação Android.
