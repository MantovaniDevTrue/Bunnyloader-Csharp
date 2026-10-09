# Source C# Test 0.1.2

Mod experimental em C# puro (`.cs`) para Bunny Loader Android, sem JavaScript, `.so` ou `.dll` no pacote.

O Roslyn compila `SourceBolt`, `SourceWand` e a classe de homing. O MonoVM isolado executa `SetDefaults`, curvas de movimento e `SourceBolt.OnHitNPC(int damage)` antes da entrada no jogo. O host Native/IL2CPP aplica os resultados validados durante o jogo.

Nesta versao o worker prepara 256 duracoes de queimadura por dano base do projetil (0 a 255). O hook seleciona a duracao por dano do projetil sem MonoVM por impacto. Dano acima de 255 usa a ultima entrada. Nao e ainda callback C# ao vivo nem usa o dano final aplicado ao NPC.

No console, dentro de um mundo solo:

```js
bl.csharp.sourceCompile()
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
bl.csharp.sourceApplyCompiled("CONFIRM_MANAGED_DEFAULTS")
bl.csharp.sourceEnableHomingAI("CONFIRM_HOMING_CSHARP_AI")
bl.csharp.sourceEnableOnHit("CONFIRM_CSHARP_ONHIT")
JSON.stringify(bl.csharp.sourceHitStatus(), null, 2)
```

Aguarde a compilacao terminar antes de aplicar os valores. `sourceHitStatus()` inclui `lastDamage` e `lastDuration`. O teste continua solo e preserva o homing. A versao 0.1.10.16.27 segue como referencia estavel.
