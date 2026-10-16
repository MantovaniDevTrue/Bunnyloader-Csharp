# Bunny Loader C# .85.1 — hotfix de carregamento de efeitos

Observação em Android .85: `sourceLiveHitStatus().hitContext.v4Calls=0`, `effects.gameHookInstalled=false` e `effects.requested=0`. O MonoVM chegou a `result=42`, mas os números não confirmam callbacks ativos naquele mundo. Não há causa definitiva somente pelo log; estes são pontos frágeis identificados no fonte.

Modificações da .85.1:
- Aguarda até 60 reintentos de 500ms quando MonoVM ainda não está pronto ou binding falha temporariamente em `PINNED_MONO`/owner ocupado; o bloqueio da transição de mundo é liberado antes de esperar.
- Instala o hook de `NPC.AI` pela thread do update do Terraria, não pela thread auxiliar do auto bind.
- Permite novas tentativas de resolver referências `NPC.AI` quando `GameRefs` não estão disponíveis na primeira vez.
- Mantém a fila FIFO64 com vida útil 1,5s, validação de índice/tipo/posição e compatibilidade com .84.

Diagnóstico no editor **durante o mundo** e após atacar inimigos com a ExampleWaveWand:

```js
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  live: bl.csharp.sourceLiveHitStatus(),
  extra: bl.csharp.sourceLiveProjectileStatus(),
  world: bl.csharp.sourceMemoryStatus().world,
  compile: bl.csharp.sourceCompileStatus().status
}, null, 2)
```

Interpretar `live.enabled`, `live.phase`, `auto.phase`, `effects.gameHookInstalled`, `effects.requested`, `effects.applied`, `v4Calls`, `lastBuffId`, além de quedas de FPS. Teste da .85.1 no dispositivo permanece pendente.
