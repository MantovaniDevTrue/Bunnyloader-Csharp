# ExampleModCSharp 0.1.8 — protocolo de teste Android

**Estado:** CI aprovado; **aguardando teste de gameplay no dispositivo**. Não confundir sucesso no CI com 60 FPS reais.

**Base:** APK Bunny Loader C# **0.1.10.16.72** (Managed Ingress) previamente validada em Android. **Não é uma APK .73.** O arquivo novo é apenas o ExampleModCSharp `0.1.8`, com o mesmo UID do `0.1.7`.

**Pacote experimental:** [ExampleModCSharp-v0.1.8.bmod](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/download/bunny-csharp-examplemod-0.1.8-dual-projectile-test/ExampleModCSharp-v0.1.8.bmod).

## Mudanças, usando apenas API suportada do Bunny

- `ExampleWand` + `ExampleBolt` permanecem com os parâmetros, movimento, efeitos e callbacks existentes.
- `ExampleWaveWand`: segunda `BunnyWeapon`, `Damage=12`, `UseTime=16`, `ShootSpeed=9`, `Shoot=ExampleWaveBolt`.
- `ExampleWaveBolt`: `BunnyProjectile` separado (10×10, `AiStyle=1`, `Penetrate=2`, `TimeLeft=180`).
- `ExampleWaveBoltMotion`: curva senoidal independente `MathF.Sin(tick * 0.20f) * 0.018f`, com 120 amostras calculadas pelo Roslyn na compilação.
- O pacote reaproveita temporariamente os PNG validados do 0.1.7; não é uma mudança do visual final.
- Sem alteração do runtime MonoVM, sem tModLoader, sem nova libmod.so, sem mudanças na fila de eventos ou no limite de 25 mil.

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

Se `wave.registered=false`, faltar arma/projétil, ou `auto.phase=failed`, envie o JSON e o relatório do Bunny; não marque suporte multi-projétil como validado no Android até corrigir.

## Evidências CI

A suíte Roslyn compila ambos os `BunnyWeapon` e ambos os `BunnyProjectile`, verifica suas referências `Shoot`, confere as duas curvas independentes de 120 amostras, mantém o verificador dos efeitos de `ExampleBolt` e comprova que o novo arquivo `.bmod` preserva o UID e as texturas da release .72.

Nenhuma dessas verificações é equivalente a medir estabilidade de renderização ou da integração nativa no smartphone.
