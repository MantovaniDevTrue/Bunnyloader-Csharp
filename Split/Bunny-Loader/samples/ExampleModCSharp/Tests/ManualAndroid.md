# Checklist Android para os exemplos C#

Execute com o APK C# de teste e o `SourceCSharpMod-v0.1.5.bmod`. O teste automatico nao substitui estas verificacoes.

- [ ] Compilacao: `state=finished`, `assemblyReady=1`, sem worker travado
- [ ] `sourceApplyCompiled`: `applied=true`, `items=2`, `projectiles=2`
- [ ] SourceWand dispara SourceBolt
- [ ] SourceStaff dispara SourceSpark
- [ ] Homing SourceBolt: `sourceAIStatus().errors === 0` e `callbacks` aumenta
- [ ] OnHitNPC: `hitEvents` e `buffsApplied` aumentam com impactos validos
- [ ] OnKill: `killEvents` aumenta e `buffsApplied` so cresce quando ha NPC proximo
- [ ] Generic SourceSpark: `sourceProjectileAIStatus(...).registered === true`
- [ ] Generic SourceSpark: `callbacks` e `velocityWrites` crescem, `errors=0`
- [ ] SourceBolt e SourceSpark juntos, efeitos preservados, sem crash
- [ ] FPS comparado com a base estavel (60 FPS no aparelho de referencia)

Para parar apenas a IA generica:

```javascript
bl.csharp.sourcePauseProjectileAI(
  "57a98f20-7a35-4d19-8ce8-079e25bb36a1",
  "SourceSpark"
)
```

Nao ativar a politica generica e `sourceEnableSparkAI()` para o mesmo projetil ao mesmo tempo.

Ao validar uma API nova, atualizar o ExampleMod C#, o catalogo de compatibilidade, os testes do Roslyn e o checklist Android.
