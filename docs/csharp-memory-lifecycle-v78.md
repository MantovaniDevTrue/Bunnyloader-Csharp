# Bunny Loader C# .78 — monitoramento de memória e ciclo de vida

**Estado:** versão experimental em CI; Android ainda não testado. **Baseline funcional:** .77, dois mods C# com pausa e retomada no mesmo mundo e 379/379 eventos, 60 FPS relatados sem quedas.

## Escopo

A .78 acrescenta diagnósticos **somente sob demanda**, acessíveis pelo editor do Bunny Loader:

- `bl.csharp.sourceMemoryBaseline()` captura um novo valor de referência (RSS do processo) para comparar testes.
- `bl.csharp.sourceMemoryStatus()` captura os valores atuais de memória e as contagens de ciclo de vida **sem executar Mono GC e sem descarregar assemblies**.
- `rssKb`, `peakRssKb` (VmHWM), `virtualKb` (VmSize), `dataKb` (VmData), `swapKb` (VmSwap), `threads`: métricas de `/proc/self/status`, que representam o **processo inteiro**, não só a heap do MonoVM.
- `baselineRssKb` e `rssDeltaKb`: delta em kB desde a última chamada explícita de baseline (ou desde o primeiro status se não houver baseline). Um delta positivo isolado **não demonstra vazamento**.
- `cachedCallbackEntries` e `cachedAssemblyPaths`: quantidade de entradas C# do cache de métodos e quantidade de caminhos únicos de assemblies presentes nesse cache. Não equivalem ao número total de assemblies carregadas na VM, nem provam que uma assembly foi descarregada.
- `boundProjectileTypes`, `activeProjectileTypes`, `workerTid`: informações do dispatcher MonoVM.
- `world.epoch`, `world.worldLoads`, `world.worldExits`, `world.rebindSchedules`, `world.configuredBindings`, `world.worldReady`: contadores de entradas, saídas e solicitações de vínculo dentro do processo atual.

O parser de `/proc/self/status` é portátil e testado separadamente no CI. Essas leituras só ocorrem quando o usuário invoca os comandos, sem atualizar por frame ou adicionar trabalho ao worker de callbacks.

**Sem alterações:** ExampleModCSharp 0.1.9, SecondExampleModCSharp 0.1.0, lógica de tiros e curvas, buffs, FIFO64, limite agregado de 25.000 eventos, reentrada, pausa e retomada da .77, integração MonoVM/QuickJS, carregador legado de `libmod.so`.

## Roteiro Android — memória em várias sessões

1. Faça backup de um mundo de teste, instale .78 **por cima** da .77, sem apagar dados. Mantenha os dois mods C# ativos e sem versões duplicadas.
2. No primeiro mundo, aguarde os dois mods ficarem `ready`. Execute:

```javascript
JSON.stringify(bl.csharp.sourceMemoryBaseline(), null, 2)
```

3. Ataque aproximadamente 20–40 inimigos com as armas dos dois mods; confirme que o FPS não caiu. Salve e saia para o menu; entre novamente no mesmo mundo. Repita isso **5 vezes** sem fechar o Bunny Loader.
4. No editor, depois da última reentrada e de aguardar os projéteis terminarem, execute:

```javascript
JSON.stringify({
  memoria: bl.csharp.sourceMemoryStatus(),
  auto: bl.csharp.sourceAutoStatus(),
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

5. Registre os resultados após a primeira, terceira e quinta reentradas, se possível, para observar tendências de RSS e quantidade de entradas de cache. Observe se `world.worldLoads` e `world.rebindSchedules` aumentam conforme esperado. `cachedCallbackEntries` e `cachedAssemblyPaths` devem se estabilizar depois que os métodos do ExampleMod forem invocados em ambos os mods.
6. Repita os testes de `sourceLiveModPause()` e `sourceLiveModResume()` do guia .77, conferindo que continuam funcionando com 60 FPS.

## Interpretação

- Aumento moderado de RSS entre as primeiras reentradas pode representar cache, reservas de heap ou recursos do próprio Terraria; comparar a tendência estabilizada, não apenas dois pontos.
- RSS estável e cache sem crescimento contínuo são indícios favoráveis, **não prova** de ausência de leaks.
- `peakRssKb` é um máximo histórico do processo e **não deve diminuir** ao sair do mundo.
- `virtualKb` representa espaço de endereço virtual reservado, **não** memória física ocupada.
- `world.epoch` pode avançar tanto na entrada quanto na saída do mundo; não interpretá-lo como número de mundos independentes.
- Se `sourceMemoryStatus().available=false`, as métricas do proc não ficaram disponíveis; não usar valores zerados como medidas reais.

## Critérios de regressão

Dois mods C# ativos após várias reentradas, nenhum crash, fila drenada e sem `lockDrops`/falhas, cache com limite e contagem coerentes e 60 FPS relatados. **Não existe valor máximo garantido de RSS neste estágio**; se RSS crescer continuamente, coletar mais amostras antes de modificar GC ou MonoVM.

**Não implementado:** descarregamento real de assemblies, GC forçado, rastreamento da heap interna do MonoVM, desativação de mod pelo gerenciador em tempo real e multiplayer completo. A .77 continua como rollback estável até conclusão dos testes Android da .78.
