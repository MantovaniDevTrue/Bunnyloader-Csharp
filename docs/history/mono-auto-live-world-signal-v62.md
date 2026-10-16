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

## Resultado observado no Android — 10/10/2026, 22:25 (BRT)

**VALIDADO para o escopo dos callbacks automáticos do ExampleModCSharp 0.1.7 no singleplayer.** O GitHub Actions da .62 concluiu com sucesso, mas a prova principal veio do aparelho Xiaomi 2109119DG (Android 14, ARM64).

- No próprio processo do jogo: `mono.domainInitialized=true`, `mono.methodExecuted=true`, `mono.result=42`, `phase=verified`.
- Antes do limite, a ponte estava `automatic=true`, `enabled=true`, `phase=ready`. Havia **72 callbacks executados**, divididos em 25 `OnHitNPC` e 47 `OnKill`, com 0 falhas e 0 descartes.
- No final: `automatic=true`, `queued=512`, `processed=512`, `hitProcessed=187`, `killProcessed=325`, `failures=0`, `dropped=9`, `phase=limit-reached`, `enabled=false`.
- O usuário relatou que o jogo **continuou rodando normalmente depois de atingir o limite**. O desligamento foi o comportamento previsto do sistema de segurança.

A fila possui um único evento pendente; `dropped` significa descarte por ocupação/limite, não erro gerenciado. Os 512 eventos processados não significam compatibilidade com qualquer mod C# ou que o código altera livremente o combate. **Não houve medição numérica de FPS.**

Próxima etapa: medir desempenho e custo da ponte em sessões maiores, propor retomada segura por sessão/mundo e eventualmente uma fila limitada com política explícita de descarte. Não remover as proteções nem alterar efeitos de gameplay apenas para alcançar números maiores.

[APK .62 e ExampleModCSharp 0.1.7](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.62).
