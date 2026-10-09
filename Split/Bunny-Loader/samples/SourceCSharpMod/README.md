# Source C# Test 0.1.3

Teste de arma e projétil escritos em C# puro para o Bunny Loader Android, sem JavaScript, DLL nem .so no mod.

O Roslyn/MonoVM executa SetDefaults, homing, OnHitNPC e os novos métodos OnKill antes da partida. O Native/IL2CPP aplica os resultados durante as colisões e ao destruir projéteis, sem executar MonoVM a cada evento.

Nesta versão, SourceBolt aplica On Fire! (buff 24) a NPCs próximos ao desaparecer. O C# define raio e duração a partir do dano do projétil. O teste usa 256 valores pré-calculados, raio de 24 a 100 pixels, duração de 60 a 160 ticks, até seis NPCs por morte. Com dano 22: raio 68 pixels e duração 104 ticks. Uso apenas em singleplayer.

No console do Bunny Loader, compile o mod e aguarde terminar:

```js
bl.csharp.sourceCompile()
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
```

Depois ative:

```js
bl.csharp.sourceApplyCompiled("CONFIRM_MANAGED_DEFAULTS")
bl.csharp.sourceEnableHomingAI("CONFIRM_HOMING_CSHARP_AI")
bl.csharp.sourceEnableOnHit("CONFIRM_CSHARP_ONHIT")
bl.csharp.sourceEnableOnKill("CONFIRM_CSHARP_ONKILL")
JSON.stringify(bl.csharp.sourceKillStatus(), null, 2)
```

O OnKill registra mortes, buffs aplicados, dano, raio, duração e erros. Para interromper: `bl.csharp.sourcePauseOnKill()`.

O C# ainda prepara políticas antecipadamente; não é um callback gerenciado executado em tempo real.

Referência estável: Bunny Loader C# 0.1.10.16.29, Source C# Test 0.1.2.
