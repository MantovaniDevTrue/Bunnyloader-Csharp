# Source C# Test 0.1.4

Teste de mod Bunny Loader com duas armas e dois projeteis escritos em C# puro.

A versao inclui SourceWand + SourceBolt e SourceStaff + SourceSpark. SourceSpark e SourceStaff reutilizam os sprites existentes e possuem SetDefaults independentes, compilados pelo Roslyn e executados pelo MonoVM. O host Native valida o conjunto e aplica os valores ao Terraria em lote, sem exigir DLL ou .so dentro do mod.

SourceBolt preserva homing, OnHitNPC e OnKill. Os hooks especificos continuam afetando somente SourceBolt. SourceSpark usa o comportamento normal de projetil configurado pelo Bunny Loader; ainda nao tem IA e efeitos C# individuais.

No console, em mundo singleplayer:

```js
bl.csharp.sourceCompile()
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
```

Espere aparecer state=finished e assemblyReady=1. Em seguida:

```js
bl.csharp.sourceApplyCompiled("CONFIRM_MANAGED_DEFAULTS")
JSON.stringify(bl.csharp.sourceMods(), null, 2)
```

O retorno de sourceApplyCompiled deve ter items=2 e projectiles=2. As duas armas devem aparecer no jogo e disparar seus projeteis separados. Para testar os efeitos antigos:

```js
bl.csharp.sourceEnableHomingAI("CONFIRM_HOMING_CSHARP_AI")
bl.csharp.sourceEnableOnHit("CONFIRM_CSHARP_ONHIT")
bl.csharp.sourceEnableOnKill("CONFIRM_CSHARP_ONKILL")
```

O teste usa somente a base Bunny Loader e ExampleModBunny. A arquitetura de callbacks C# continua baseada em politicas antecipadas e hooks nativos, sem chamadas gerenciadas a cada frame de projeteis.

Referencia estavel anterior: v0.1.10.16.30 com Source C# Test 0.1.3.
