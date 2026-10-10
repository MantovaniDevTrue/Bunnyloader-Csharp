# Bunny Loader C# 0.1.10.16.60 — callbacks automáticos

## O que esta versão verifica

A base .59 executou 10/10 eventos OnHitNPC reais do ExampleBolt, sem perda ou falha. A .60 automatiza a vinculação de dois métodos C# declarados por convenção, sem comando de ativação no editor.

O ExampleModCSharp 0.1.7 adiciona `content/Common/Live/BunnyLiveEvents.cs`:

```csharp
namespace BunnyLoader.Source;
public static class BunnyLiveEvents
{
    public static int OnHitNPC(int call, int damage) => damage + call;
    public static int OnKill(int call, int damage) => damage + call;
}
```

O arquivo de exemplo real contém contadores distintos em cada método, e não altera o gameplay. O loader só tenta o vínculo automático quando encontra o arquivo de convenção e as políticas OnHitNPC e OnKill do mesmo projétil já estão registradas. Os dois métodos devem ser `public static int` com dois parâmetros `int`. O vínculo só inicia depois de entrar em um mundo singleplayer e a compilação automática ter produzido a DLL do mod.

A implementação atual pode vincular **um único tipo de projétil de um mod por processo**; isso é deliberado enquanto validamos o ciclo de vida. A descoberta de mods adicionais e callbacks NPC/AI gerenciados por frame ainda não foi liberada. As curvas de IA precomputadas existentes permanecem automáticas e não fazem chamada Mono por frame.

## Teste no Android

1. Faça backup de personagem/mundo.
2. Instale o APK 0.1.10.16.60 e importe `ExampleModCSharp-v0.1.7.bmod`, substituindo a versão 0.1.6.
3. Entre em um mundo singleplayer, aguarde o carregamento automático e observe o FPS.
4. Execute apenas a consulta no editor:

```javascript
JSON.stringify({
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus(),
  auto: bl.csharp.sourceAutoStatus()
}, null, 2)
```

Confirmação esperada: `mono.domainInitialized=true`, `mono.methodExecuted=true`, `mono.result=42`; `live.enabled=true`, `live.automatic=true`, `live.phase="armed"`.

5. Dispare a ExampleWand em NPCs hostis. Aguarde um projétil se destruir para que também ocorra OnKill. Consulte `bl.csharp.sourceLiveHitStatus()`: `hitProcessed>0`, `killProcessed>0`, `failures=0` e `lastEvent="OnHitNPC"` ou `"OnKill"`.
6. Continue jogando, salve o mundo e faça pausa/retomada. Interrompa a ponte com `bl.csharp.sourceLiveHitPause()` se necessário.

## Proteções

O método gerenciado é executado somente pelo owner Mono persistente. O hook apenas copia inteiros para uma fila de capacidade 1 e nunca espera a conclusão gerenciada na thread Unity. Limite de 512 eventos e 3 falhas, com desativação do callback. Não altera damage, buffs, velocidade ou hitbox; preserva a aplicação original das políticas C# pré-calculadas. O modo multiplayer não ativa o vínculo automático. Código não validado no aparelho até o teste manual.
