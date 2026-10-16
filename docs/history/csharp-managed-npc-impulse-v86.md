# Bunny Loader C# .86 — empurrão de NPCs controlado por C#

Branch experimental isolada, baseada na .85.1 validada com 479 callbacks V4, 410 debuffs nativos e 60 FPS.

O mod .bmod continua contendo o arquivo `content/Common/Live/ExampleWaveBoltLiveEvents.cs` com a lógica `OnHitNPCV4`. O Bunny interpreta seu valor de retorno:
- `1000000 + buffId * 1000 + ticks`: solicita debuff vanilla existente.
- `2000000 + direction * 100 + strengthTenths`: solicita empurrão horizontal. `direction=1` para esquerda, `direction=2` para direita; intensidade entre `10` e `60` décimos (1.0 a 6.0).

Exemplo C# (retorno para um impacto):
```csharp
int direction = npcX < playerX ? 1 : 2;
int strengthTenths = 35;
return 2000000 + direction * 100 + strengthTenths;
```

Comandos são enviados à mesma FIFO de capacidade 64 sem ponteiros de jogo. Na thread de NPC.AI, o Bunny revalida índice, tipo, posição dentro de 128 pixels, idade até 1.500 ms, inimigo ativo e mundo singleplayer. A velocidade X é ajustada com limite absoluto de 12.0. Debuffs e os callbacks antigos permanecem.

## Diagnóstico no editor

```js
const s = bl.csharp.sourceLiveHitStatus();
JSON.stringify({
  ready: { enabled: s.enabled, phase: s.phase, v4: s.hitContext.v4Calls },
  efeitos: s.effects,
  mods: bl.csharp.sourceAutoStatus().map(x => ({ id: x.modId, phase: x.phase, cache: x.cached }))
}, null, 2)
```

`effects.impulsesApplied` deve aumentar ao acertar com ExampleWaveWand, e `effects.applied` conta buffs e impulsos. Confirme visualmente o empurrão, estabilidade na reentrada e FPS no Android antes de considerar a .86 validada.
