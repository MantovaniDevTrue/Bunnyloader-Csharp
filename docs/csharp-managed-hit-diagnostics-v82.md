# Bunny Loader C# .82 — diagnósticos dos hits

Branch experimental: csharp-managed-hit-diagnostics-v82-experiment. A .81 foi validada no Android com 40 NPCs e mínimo de 59 FPS.

Após cada callback MonoVM, o worker registra qual assinatura foi realmente invocada, sem logs por golpe, alocações ou locks adicionais na thread do jogo.

## Comando no console

```js
JSON.stringify({
  principal: bl.csharp.sourceLiveHitStatus().hitContext,
  projeteis: bl.csharp.sourceLiveProjectileStatus().map(x => ({
    mod: x.modId,
    projetil: x.projectile,
    contexto: x.hitContext
  }))
}, null, 2)
```

Campos: v2Calls, v2Successes, v2Failures, legacyCalls, legacyFailures, lastNpcIndex, minNpcIndex, maxNpcIndex, uniqueNpcIndices e invalidV2Indices.

v2Calls registra chamadas reais a OnHitNPCV2(int,int,int), não apenas hits com índice presente. uniqueNpcIndices conta slots distintos entre 0 e 199, que podem ser reutilizados entre NPCs. invalidV2Indices deve ser 0. O índice é uma fotografia do slot de colisão e não uma referência gerenciada de NPC. OnKill mantém a assinatura antiga e não entra nos contadores.

Testar com ExampleModCSharp 0.1.10 e SecondExampleModCSharp 0.1.0, atingir 40 inimigos usando ExampleWaveWand, verificar v2Calls > 0 em ExampleWaveBolt, legacyCalls > 0 nas armas legadas, FPS, erros e salvamento. Contadores zeram quando a sessão é reativada.
