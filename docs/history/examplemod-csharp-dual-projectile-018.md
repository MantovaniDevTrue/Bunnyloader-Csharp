# ExampleModCSharp 0.1.8 — validação de dois projéteis C# no Android

**Estado:** **aprovado no Android no cenário observado**. CI aprovado, dois projéteis com curvas ativas e sem erros, eventos da arma original preservados, **60 FPS relatados pelo usuário**. Testes com vários mods e callbacks gerenciados individuais por tipo continuam pendentes.

**Base:** APK Bunny Loader C# **0.1.10.16.72** (Managed Ingress) previamente validada em Android. **Não é uma APK .73.** O arquivo novo é apenas o ExampleModCSharp `0.1.8`, com o mesmo UID do `0.1.7`.

**Pacote experimental:** [ExampleModCSharp-v0.1.8.bmod](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/download/bunny-csharp-examplemod-0.1.8-dual-projectile-test/ExampleModCSharp-v0.1.8.bmod).

## Mudanças, usando apenas API suportada do Bunny

- `ExampleWand` + `ExampleBolt` permanecem com os parâmetros, movimento, efeitos e callbacks existentes.
- `ExampleWaveWand`: segunda `BunnyWeapon`, `Damage=12`, `UseTime=16`, `ShootSpeed=9`, `Shoot=ExampleWaveBolt`.
- `ExampleWaveBolt`: `BunnyProjectile` separado (10×10, `AiStyle=1`, `Penetrate=2`, `TimeLeft=180`).
- `ExampleWaveBoltMotion`: curva senoidal independente `MathF.Sin(tick * 0.20f) * 0.018f`, com 120 amostras calculadas pelo Roslyn na compilação.
- O pacote reaproveita temporariamente os PNG validados do 0.1.7; não é uma mudança do visual final.
- Sem alteração do runtime MonoVM, sem tModLoader, sem nova libmod.so, sem mudanças na fila de eventos ou no limite de 25 mil.

## Resultado real no Android — 11/10/2026

O testador instalou o ExampleModCSharp 0.1.8 sobre a APK C# .72 e enviou cinco capturas do status do editor após combate com as duas armas. Informou que o Terraria permaneceu **a 60 FPS** na sessão; a duração não foi informada e não há medição contínua de FPS pelo profiler nesta rodada.

### Compilação automática e registro

- `sourceAutoStatus()[0]`: `modId=68c3115f-62f4-49f2-a56b-388256781d5f`, `phase=ready`, `cached=false`, `applied=true`, `curves=2`, `enabledCurves=2`, `hitPolicies=1`, `killPolicies=1`, `enabledHits=1`, `enabledKills=1`, `netMode=0`, `worldReady=true`, `error=""`.
- `ExampleBolt` (`type=1137`): `registered=true`, `installed=true`, `enabled=true`, `samples=120`, `spawns=47`, `callbacks=360`, `velocityWrites=360`, `errors=0`, `lastTurn=17`.
- `ExampleWaveBolt`: `registered=true`, `installed=true`, `enabled=true`, `samples=120`, `spawns=107`, `callbacks=2393`, `velocityWrites=2393`, `errors=0`, `lastTurn=-18`. O campo `type` do novo projétil não ficou legível nas capturas enviadas, portanto não registrar um ID numérico deduzido.
- `ExampleBolt.autoOnHit`: `registered=true`, `enabled=true`, `events=79`, `buffsApplied=79`, `errors=0`, `buffId=24`.
- `ExampleBolt.autoOnKill`: `registered=true`, `enabled=true`, `events=82`, `buffsApplied=344`, `errors=0`, `buffId=24`.
- `ExampleWaveBolt.autoOnHit` e `autoOnKill`: `registered=false`, `enabled=false`, `events=0`. **Esperado:** esse projétil foi criado sem políticas de buff ou callbacks gerenciados próprios.
- `sourceLiveHitStatus()`: `projectileType=1137`, `queued=161`, `processed=161`, `hitProcessed=79`, `killProcessed=82`, `dropped=0`, `lockDrops=0`, `maxQueueDepth=2`, `queueDepth=0`, `failures=0`, `eventLimit=25000`, `phase=ready`, `error=""`.

**Conclusão:** **dois tipos de projétil C# com curvas independentes foram registrados, habilitados e executaram milhares de atualizações no mesmo mod**, preservando eventos e efeitos do projétil antigo. Todos os contadores de erro apresentados foram zero, e o usuário observou 60 FPS. A curva por quadro é pré-calculada em C# pelo Roslyn e aplicada pelo hook nativo; `callbacks` de curva não equivalem a chamadas do MonoVM por frame.

**Limitações:** as capturas não cobrem uma medição quantitativa de frametime/RSS, efeitos visuais detalhados da curva, nem multiplayer ou múltiplos mods. A arquitetura automática continua com **um único tipo de projétil vinculado a callbacks live gerenciados por processo**; o passo seguinte é generalizar o vínculo por mod e tipo com isolamento e testes de saturação.

## Teste no aparelho

1. Mantenha a APK .72. Faça backup do mundo, use preferencialmente um mundo de teste e **substitua** o mod ExampleModCSharp 0.1.7 pelo 0.1.8; não habilite as duas versões simultaneamente.
2. Entre em singleplayer, espere o compilador automático concluir, obtenha `ExampleWand` e `ExampleWaveWand` no menu de itens do mod.
3. Atire com a **arma original** e confirme que os impactos/efeitos funcionam, sem travamento.
4. Atire com a **arma nova** em inimigos, de preferência variando distância e direção. Confirme que `ExampleWaveBolt` aparece e que sua trajetória oscila de modo distinto.
5. Observe o indicador de FPS com tiros simultâneos e combate contra inimigos/boss, e abra o editor para consultar o status:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  original: bl.csharp.sourceProjectileAIStatus(
    "68c3115f-62f4-49f2-a56b-388256781d5f", "ExampleBolt"
  ),
  wave: bl.csharp.sourceProjectileAIStatus(
    "68c3115f-62f4-49f2-a56b-388256781d5f", "ExampleWaveBolt"
  ),
  live: bl.csharp.sourceLiveHitStatus()
}, null, 2)
```

## Aprovação esperada

- `auto` indica `curves=2` e `enabledCurves=2`, sem mensagem de erro.
- `original` e `wave` exibem `registered=true`, `enabled=true`, `installed=true` e `type` **diferentes**.
- Após atirar, `callbacks` e `velocityWrites` crescem para ambos os tipos; `errors=0`.
- `live` continua com `phase=ready`, `failures=0`, `dropped=0` e `lockDrops=0` nos cenários observados. **A .72 ainda possui um vínculo live automático por processo**, não espere callbacks gerenciados separados para o novo projétil.
- FPS visual próximo de 60, sem regressões percebidas.

**Estado da rodada:** aprovado na carga observada; mantenha esse roteiro para regressões futuras. Se `wave.registered=false`, faltar arma/projétil, ou `auto.phase=failed`, envie o JSON e o relatório do Bunny antes de marcar uma futura revisão como aprovada.

## Evidências CI

A suíte Roslyn compila ambos os `BunnyWeapon` e ambos os `BunnyProjectile`, verifica suas referências `Shoot`, confere as duas curvas independentes de 120 amostras, mantém o verificador dos efeitos de `ExampleBolt` e comprova que o novo arquivo `.bmod` preserva o UID e as texturas da release .72.

Nenhuma dessas verificações é equivalente a medir estabilidade de renderização ou da integração nativa no smartphone.
