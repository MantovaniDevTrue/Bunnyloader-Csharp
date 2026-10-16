# Source C# Test 0.1.5

Mod de teste com duas armas, dois projeteis e seis arquivos C# para o Bunny Loader Android.

SourceWand dispara SourceBolt: homing, OnHitNPC e OnKill continuam funcionando sem alterações.
SourceStaff dispara SourceSpark, que agora possui sua própria curva C# em SourceSparkMotion.cs. O Roslyn compila os arquivos e o MonoVM gera 120 amostras de movimento. O hook nativo Projectile.AI aplica a curva ao SourceSpark sem invocar MonoVM por frame. Os dois projeteis usam estados, contadores e tipos independentes. Os sprites são reutilizados, sem imagens novas.

Teste em um mundo singleplayer:

```js
bl.csharp.sourceCompile()
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
```

Aguarde estado finished e assemblyReady=1 e execute:

```js
bl.csharp.sourceApplyCompiled("CONFIRM_MANAGED_DEFAULTS")
bl.csharp.sourceEnableHomingAI("CONFIRM_HOMING_CSHARP_AI")
bl.csharp.sourceEnableOnHit("CONFIRM_CSHARP_ONHIT")
bl.csharp.sourceEnableOnKill("CONFIRM_CSHARP_ONKILL")
bl.csharp.sourceEnableSparkAI("CONFIRM_SPARK_CSHARP_AI")
```

Dispare com SourceWand e SourceStaff e verifique:

```js
JSON.stringify({
  mods: bl.csharp.sourceMods(),
  bolt: bl.csharp.sourceAIStatus(),
  spark: bl.csharp.sourceSparkStatus(),
  hit: bl.csharp.sourceHitStatus(),
  kill: bl.csharp.sourceKillStatus()
}, null, 2)
```

No SourceSpark, callbacks e velocityWrites devem crescer. Verifique errors=0 e FPS durante os disparos. É possível desligar apenas a nova curva com `bl.csharp.sourcePauseSparkAI()`.

Este é o primeiro comportamento C# de movimento independente do SourceBolt; os hooks ainda possuem integração específica por nome de classe e não são um sistema genérico de callbacks MonoVM ao vivo.

Base estável anterior: Bunny Loader C# v0.1.10.16.32 e Source C# Test 0.1.4.
