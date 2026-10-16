# Bunny Loader C# .83 — snapshot seguro de NPC para callbacks

Implementação experimental baseada em .82 (validada com 59–60 FPS e 40 NPCs). Nenhuma mudança em .82.

## Callback C#
```csharp
public static int OnHitNPCV3(int call, int damage, int npcIndex, int npcType, int npcLife, float posX, float posY)
{
    return npcType + npcLife + (int)posX + (int)posY;
}
```

O hook original do Bunny Loader resolve Terraria.NPC.type/life e Terraria.Entity.position pelo Resolver. Na thread do jogo, depois de validar modo solo, índice 0–199, NPC ativo, não-amigável e vida positiva, copia quatro valores numéricos para o evento, sem guardar referência do NPC. O worker MonoVM consome cópia congelada, valida tipo/vida/coordenadas finitas e chama V3, V2 ou legado. OnKill preserva a assinatura de 2 argumentos e não recebe snapshot.

Validação: `managedNpcSnapshotIsValid` rejeita tipos negativos, NPC sem vida e coordenadas NaN/infinito/fora do intervalo. Em casos sem snapshot, V3 não executa: fallback para V2 (índice válido) ou OnHitNPC (legado).

## Console de diagnóstico
```js
JSON.stringify({
  principal: bl.csharp.sourceLiveHitStatus().hitContext,
  projeteis: bl.csharp.sourceLiveProjectileStatus().map(x => ({
    projetil: x.projectile, status: x.hitContext
  }))
}, null, 2)
```

Verificar `v3Calls`, `v3Successes`, `v3Failures`, `lastNpcType`, `lastNpcLife`, `lastNpcX`, `lastNpcY`. Os índices representam slots, não identidade persistente. Os dados de vida e posição pertencem ao instante em que foram capturados, e podem já ter mudado quando o callback assíncrono executa.

## Teste Android
Use BunnyLoader CSharp .83 e ExampleModCSharp 0.1.11, mantendo SecondExampleModCSharp 0.1.0. Dispare ExampleWaveWand contra vários NPCs, confira V3 > 0, falhas 0, índices válidos, posição plausível e FPS. Depois use outras armas, salve e reentre no mundo. Só classificar como estável após validar em dispositivo.
