# Primeiro teste C# no Bunny Loader

O pacote usado para verificar os exemplos e o `SourceCSharpMod-v0.1.5.bmod` da versao 0.1.10.16.34. Os arquivos de `Examples/` sao espelhos educativos e nao substituem o pacote.

1. Instale o Bunny Loader C# 0.1.10.16.34 e o Source C# Test 0.1.5.
2. Abra um mundo singleplayer.
3. No console, execute `bl.csharp.sourceCompile()` e acompanhe `bl.csharp.sourceCompileStatus()` ate aparecer `state=finished` e `assemblyReady=1`.
4. Aplique `bl.csharp.sourceApplyCompiled("CONFIRM_MANAGED_DEFAULTS")`. O resultado deve conter `applied: true`, `items: 2` e `projectiles: 2`.
5. Ative o homing e os efeitos conhecidos:

```javascript
bl.csharp.sourceEnableHomingAI("CONFIRM_HOMING_CSHARP_AI")
bl.csharp.sourceEnableOnHit("CONFIRM_CSHARP_ONHIT")
bl.csharp.sourceEnableOnKill("CONFIRM_CSHARP_ONKILL")
```

6. Para ativar **a politica generica** do SourceSpark, nao ative `sourceEnableSparkAI()` ao mesmo tempo:

```javascript
bl.csharp.sourceEnableProjectileAI(
  "57a98f20-7a35-4d19-8ce8-079e25bb36a1",
  "SourceSpark",
  "CONFIRM_CSHARP_PROJECTILE_AI"
)
```

7. Dispare com SourceWand e SourceStaff. Consulte:

```javascript
JSON.stringify({
  mods: bl.csharp.sourceMods(),
  bolt: bl.csharp.sourceAIStatus(),
  spark: bl.csharp.sourceProjectileAIStatus(
    "57a98f20-7a35-4d19-8ce8-079e25bb36a1",
    "SourceSpark"
  ),
  hit: bl.csharp.sourceHitStatus(),
  kill: bl.csharp.sourceKillStatus()
}, null, 2)
```

O que confirma funcionamento: `enabled: true`, contadores crescendo, `errors: 0`, armas disparando corretamente e FPS estavel no aparelho. O GitHub Actions nao mede FPS real.

## Importante

- O sistema atual **nao compila qualquer mod C# com UUID arbitrario**.
- Os callbacks de impacto e destruicao ainda sao integracoes experimentais especificas do SourceBolt.
- O motor nativo aplica politicas preparadas pelo MonoVM; nao executa codigo gerenciado a cada frame.
- No multiplayer, essas politicas experimentais nao devem ser ativadas.

## ExampleModCSharp independente — teste experimental

A partir da branch 0.1.10.16.35, um segundo .bmod possui identificador proprio:
`68c3115f-62f4-49f2-a56b-388256781d5f`.

Instale `ExampleModCSharp-v0.1.0.bmod` e o APK correspondente. Dentro de um mundo solo, execute:

```javascript
bl.csharp.sourceCompileMod("68c3115f-62f4-49f2-a56b-388256781d5f")
```

Espere o status indicar `state=finished`, `assemblyReady=1`, `executedTypes=2` e o mesmo `modId`. Depois:

```javascript
bl.csharp.sourceApplyMod(
  "68c3115f-62f4-49f2-a56b-388256781d5f",
  "CONFIRM_MANAGED_DEFAULTS"
)
bl.csharp.sourceEnableProjectileAI(
  "68c3115f-62f4-49f2-a56b-388256781d5f",
  "ExampleBolt",
  "CONFIRM_CSHARP_PROJECTILE_AI"
)
```

Esperado: `applied=true`, `items=1`, `projectiles=1`. Dispare a ExampleWand e confira:

```javascript
JSON.stringify(
  bl.csharp.sourceProjectileAIStatus(
    "68c3115f-62f4-49f2-a56b-388256781d5f",
    "ExampleBolt"
  ), null, 2
)
```

Os contadores `callbacks` e `velocityWrites` devem aumentar, com `errors=0`. A confirmacao do desempenho em 60 FPS precisa ser feita no Android. Nunca aplique os resultados de compilacao de outro mod: o worker passa a registrar a identidade de origem e o host verifica essa identidade.
