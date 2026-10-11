# Bunny Loader C# 0.1.10.16.62 — sinal real de carregamento do mundo

## Evidência no Android — versão .61

O ExampleModCSharp 0.1.7 carregou e seus registros de curva e políticas `OnHitNPC`/`OnKill` passaram, inclusive ExampleBolt tipo 1136. O log registrou `Main.get_gameMenu indisponivel; aguardando estado do mundo`. Não houve inicialização MonoVM automática naquela sessão. O checkpoint MonoVM com `result=42` era de sessão anterior.

## Mudança experimental .62

A detecção automática deixa de exigir a propriedade `Main.gameMenu` quando ela não existe:

1. O hook nativo já existente de `WorldFile.LoadWorldTiles` sinaliza **somente depois** da leitura real dos tiles de mundo e das restaurações de mods, com o mapa de tiles válido.
2. Na thread de jogo do próprio Bunny Loader (`Main.DoUpdate`), verifica **`Main.netMode==0`**, `ActiveWorldFileData`, `Main.player[myPlayer]` e o flag `Player.active`. Após três quadros consecutivos válidos, envia um sinal atômico de sessão pronta.
3. A thread auto live continua esperando o mundo. Somente pode iniciar MonoVM se singleplayer e `gameMenu==0` quando disponível, **ou** se a propriedade estiver indisponível e o sinal real de mundo/jogador estiver confirmado.
4. `sourceAutoStatus()` ganha `worldReady`; `gameMenu=-1` continua significando que a propriedade **não foi resolvida**. Não substituímos esse valor por um falso `0`.

Não há chamada Mono por quadro: a verificação de estado consiste em uma leitura atômica barata quando o mundo ainda não carregou; após o sinal, a verificação é desativada. O GC corrigido na .58, Roslyn, efeitos e ExampleModCSharp 0.1.7 permanecem como estavam. A ponte gerenciada continua limitada a um tipo de projétil, singleplayer e contadores de eventos.

## Teste Android

Instalar APK .62 e o pacote `ExampleModCSharp-v0.1.7.bmod`. Entrar no mundo, aguardar a compilação (se necessária) e executar:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus()
}, null, 2)
```

O esperado é `auto[0].worldReady=true`, `mono.domainInitialized=true`, `mono.result=42`, `live.enabled=true`, `live.automatic=true` e `live.phase="armed"`. **`gameMenu` pode continuar `-1`**, porque o novo sinal é independente desse getter.

Depois usar ExampleWand em NPC hostil e aguardar morte do projétil. Critério: `hitProcessed>0`, `killProcessed>0`, `failures=0`, sem regressão visual/FPS. Confirmar pausa/retomada e saída do mundo.

**Estado:** implementação pendente de aprovação GitHub Actions e de teste Android. Não afirmar que os callbacks foram comprovados antes dos contadores reais.
