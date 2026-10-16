# Bunny Loader C# .84 — contexto NPC + jogador

Experimento separado, partindo da versão .83 validada no Android a 59–60 FPS.

## C# novo (opcional)

```csharp
public static int OnHitNPCV4(
    int call, int damage,
    int npcIndex, int npcType, int npcLife, float npcX, float npcY,
    int playerIndex, int playerLife, int playerLifeMax,
    float playerX, float playerY)
{
    int healthPercent = playerLifeMax > 0 ? playerLife * 100 / playerLifeMax : 0;
    return healthPercent < 35 ? damage + 100 : damage;
}
```

O resultado do exemplo é um valor de retorno gerenciado, **não** um modificador de dano aplicado automaticamente ao NPC; efeitos de gameplay dependem de uma ponte de comandos separada. ExampleModCSharp 0.1.12 demonstra reação ao HP e à distância do jogador.

No hook original do Terraria, o Bunny resolve o dono (`Projectile.owner`) e copia os campos já resolvidos da classe `Player`, sem executar chamadas a métodos do jogo por hit. O `ManagedPlayerSnapshot` contém `playerIndex`, `life`, `lifeMax`, `posX`, `posY`; o `ManagedNpcSnapshot` existente continua contendo tipo, vida e posição do NPC. Tudo entra como números na mesma fila FIFO64, sem ponteiros inter-threads e sem locks adicionais na thread do jogo.

- **V4**: NPC e jogador válidos;
- **V3**: NPC válido e jogador inválido ou ausente;
- **V2**: somente índice do NPC válido;
- **OnHitNPC**: fallback de dois argumentos.
- **OnKill** permanece com dois parâmetros e sem snapshot.

## Diagnóstico

```js
JSON.stringify({
  principal: bl.csharp.sourceLiveHitStatus().hitContext,
  projeteis: bl.csharp.sourceLiveProjectileStatus().map(x => ({
    projetil: x.projectile, contexto: x.hitContext
  }))
}, null, 2)
```

Verificar `v4Calls`, `v4Successes`, `v4Failures`, `lastPlayerIndex`, `lastPlayerLife`, `lastPlayerLifeMax`, `lastPlayerX`, `lastPlayerY`, junto de `lastNpcType`, `lastNpcLife` e FPS. Os snapshots representam o instante do impacto, não o estado atualizado no momento do callback assíncrono. Não assumir V4 funcionando até testar no Android.
