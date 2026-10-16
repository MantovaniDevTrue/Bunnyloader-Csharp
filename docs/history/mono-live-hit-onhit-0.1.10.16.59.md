# Bunny Loader C# v0.1.10.16.59 — Live OnHitNPC opt-in

## Base e limite

A versão .58 foi confirmada no Xiaomi 2109119DG Android 14: `DOMAIN_ONLY` e `PINNED_MONO` retornaram corretamente; o método de teste retornou 42 e o jogo permaneceu estável. A .59 é experimental, e ainda depende de validação no aparelho.

O teste aproveita o ExampleModCSharp 0.1.6 já existente. Seu `ExampleBolt` possui `OnHitNPC`, e o fonte `content/Common/Live/ExampleLiveRuntime.cs` já contém `BunnyLoader.Source.BunnyLiveEntry.Tick(int,int)`.

A arquitetura de itens/projéteis do Bunny segue os exemplos do `ExampleModBunny`; não usa classes ou hooks tModLoader.

## Procedimento Android

1. Faça backup de personagem/mundo e instale o APK .59.
2. Importe ou mantenha ligado `ExampleMod CSharp 0.1.6` e entre em singleplayer; aguarde o auto loader terminar.
3. No editor, execute:

```javascript
JSON.stringify(bl.csharp.managedGameProbe("CONFIRM_IN_PROCESS", "PINNED_MONO"), null, 2)
```

Verifique `domainInitialized: true`, `methodExecuted: true` e `result: 42`.

4. Ative explicitamente o teste:

```javascript
JSON.stringify(bl.csharp.sourceLiveHitEnable(
  "68c3115f-62f4-49f2-a56b-388256781d5f",
  "CONFIRM_LIVE_ONHIT"
), null, 2)
```

A resposta deve ter `enabled: true`, `phase: "armed"`.

5. Saia do editor e acerte um NPC hostil vivo usando `ExampleWand` que dispara `ExampleBolt`. O `OnHitNPC` já aplica seu efeito normalmente. A .59 apenas enfileira o evento validado e executa `BunnyLiveEntry.Tick` na thread fixa MonoVM, sem modificar dano, velocidade, buff ou hitbox.
6. Volte ao editor e confira:

```javascript
JSON.stringify({
  event: bl.csharp.sourceLiveHitStatus(),
  managed: bl.csharp.sourceLiveStatus("68c3115f-62f4-49f2-a56b-388256781d5f")
}, null, 2)
```

Esperado: `event.processed >= 1`, `event.failures = 0`, `event.phase = "ready"`. Compare `queued`, `dropped`, `lastDamage` e `lastResult`. O contador `processed` não confirma alteração de comportamento do projétil, apenas a execução de C# ao receber o evento.

7. Encerre o teste sem reiniciar usando `bl.csharp.sourceLiveHitPause()`. Jogue por alguns minutos, observe FPS e pause/retome o aplicativo.

## Proteções

Somente ExampleModCSharp/ExampleBolt; somente efeito automático OnHitNPC previamente registrado; comando de ativação explícito. Limitado a 512 eventos por sessão, 1 evento pendente e desligamento após 3 falhas gerenciadas. Quando ocupado, o hook não espera o MonoVM. Não há efeito novo no gameplay nem execução de Mono por frame.

O GitHub Actions verifica compilação e mock, mas não prova estabilidade real com Unity/IL2CPP. A aplicação C# completa e multiplayer não estão validados por este teste.
