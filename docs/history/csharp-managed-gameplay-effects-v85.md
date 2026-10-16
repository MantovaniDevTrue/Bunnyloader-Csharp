# Bunny Loader C# .85 — efeitos reais escolhidos pelo C#

Experimento isolado baseado na versão .84 validada com 416 callbacks V4 e FPS entre 58 e 60.

O OnHitNPCV4 continua recebendo tipo, vida, posição do NPC e vida/posição do jogador. Seu retorno inteiro, que nas versões anteriores servia somente como diagnóstico, agora pode **solicitar** um debuff vanilla usando uma codificação opcional.

## Exemplo

```csharp
public static int OnHitNPCV4(int call, int damage, int npcIndex, int npcType,
    int npcLife, float npcX, float npcY, int playerIndex, int playerLife,
    int playerLifeMax, float playerX, float playerY)
{
    int buffId = playerLifeMax > 0 && playerLife * 2 < playerLifeMax ? 39 : 44;
    int ticks = 180;
    return 1000000 + buffId * 1000 + ticks;
}
```

Valores de buff permitidos neste primeiro teste: `24` On Fire!, `39` Cursed Inferno, `44` Frostburn e `69` Ichor. Duração permitida 30–600 ticks. Um retorno normal fora da faixa codificada **não** altera o jogo, preservando os mods antigos. A aplicação acontece de maneira assíncrona; um alvo que tenha morrido, sido removido ou reutilizado não receberá o debuff.

A fila nativa é SPSC com capacidade 64 e descarta pedidos excedentes. Antes do `NPC.AddBuff`, o hook de `NPC.AI` confere mundo solo, NPC ativo e não amigável, tipo, índice e posição até 128 pixels do momento da colisão. Pedidos mais antigos que 1.500 ms ou de sessão anterior são descartados. O worker MonoVM não acessa objetos do Terraria.

## Comando de diagnóstico no editor

```js
JSON.stringify({
  hit: bl.csharp.sourceLiveHitStatus().hitContext,
  efeitos: bl.csharp.sourceLiveHitStatus().effects,
  projeteis: bl.csharp.sourceLiveProjectileStatus().map(x => ({
    projetil: x.projectile,
    hit: x.hitContext
  }))
}, null, 2)
```

Verificar `efeitos.gameHookInstalled`, `efeitos.requested`, `efeitos.applied`, `efeitos.rejected`, `efeitos.stale`, `efeitos.queueDrops`, `efeitos.lastBuffId` e FPS. A aplicação no jogo precisa ser confirmada visualmente, pois `applied` apenas informa que NPC.AddBuff não retornou exceção. Testar com ExampleModCSharp 0.1.13 e SecondExampleCSharp 0.1.0. Preservar a versão .84 como fallback.
