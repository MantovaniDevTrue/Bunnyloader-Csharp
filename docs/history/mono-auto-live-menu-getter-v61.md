# Bunny Loader C# v0.1.10.16.61 — correção de ativação automática

## Diagnóstico confirmado pela .60 no Android

O ExampleModCSharp 0.1.7 foi compilado pelo worker Roslyn (`state=finished`, `phase=compile-verified`, `assemblyReady=1`, 11 arquivos e 7 tipos). O loader aplicou as configurações, ativou 1 curva, 1 política OnHitNPC e 1 política OnKill (`auto.phase=ready`, `applied=true`). **Mesmo assim**, `mono.phase=idle` e `live.phase=idle`; os callbacks gerenciados não iniciaram.

A causa lógica foi identificada nos fontes da .60: `readActivationContext()` tenta ler `Terraria.Main.gameMenu` como um campo, mas esse valor é uma propriedade acessada pelo método `get_gameMenu()`. O status real mostrava `netMode=0` e `gameMenu=-1`; a ponte só é ativada com `gameMenu==0`. Por isso a thread aguardava sem iniciar o MonoVM.

## Correção .61

A consulta agora chama `Main.get_gameMenu()` pela mesma API IL2CPP usada em `menu/Cheats.cpp`, valida retorno sem exceção e lê o bool boxed. Se necessário, tenta o campo como fallback. Se não for possível verificar o estado, continua **sem iniciar Mono no menu** e registra um aviso após 10 segundos. Mantém o gate `netMode==0 && gameMenu==0` e não mexe nas políticas e efeitos, compilador Roslyn, código gerenciado ou GC.

O GitHub Actions inclui uma checagem para a regressão específica. Ela **não substitui** o teste Android.

## Teste Android a executar

1. Instale o APK .61 no mesmo pacote `com.bunnyloader.csharptest`; use `ExampleModCSharp 0.1.7`.
2. Entre em **singleplayer**, aguarde o auto-loader terminar e consulte:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus()
}, null, 2)
```

3. O esperado antes do combate é `auto[0].phase="ready"`, `auto[0].gameMenu=0`, `mono.methodExecuted=true`, `mono.result=42`, `live.automatic=true` e `live.phase="armed"`.
4. Com a ExampleWand, acerte um inimigo hostil e aguarde a destruição dos projéteis. Consulte novamente. A etapa passa quando `live.hitProcessed>0`, `live.killProcessed>0`, `live.failures=0`, jogo sem travamentos e FPS observado.
5. Se não ativar, capture `bl.csharp.sourceCompileStatus()`, `sourceAutoStatus()` e relatório. Não ative manualmente: o objetivo é validar a automação.
6. `LOW_MEMORY` do processo isolado **após** `compile-verified` não prova falha na compilação; acompanhar consumo de memória separadamente.

**Estado:** hotfix preparado para build/CI, ainda não validado em aparelho.
