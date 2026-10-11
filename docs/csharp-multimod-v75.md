# Bunny Loader C# 0.1.10.16.75 — Multi-Mod Managed Callbacks (experimental)

**Base Android comprovada:** .74 Managed Default (60 FPS relatados, 559/559 callbacks totais, 477/477 secundários, sem falhas ou descartes).

**Objetivo da .75:** permitir que **dois mods C# distintos**, com UIDs, DLLs e classes `BunnyLoader.Source.BunnyLiveEvents` independentes, usem o mesmo worker MonoVM e a fila FIFO64 sem misturar o estado estático gerenciado.

**Estado:** em desenvolvimento/CI; **não validada no Android**. A .74 deve continuar instalada até a .75 compilar e ser testada.

## Arquitetura

1. O primeiro mod com uma política de `OnHitNPC`/`OnKill` e `BunnyLiveEvents` ativa o vínculo principal.
2. Cada outro mod apto recebe um binding próprio no array limitado de callbacks adicionais, contendo `modId`, caminho exclusivo da assembly, nome do projétil e classe de evento `BunnyLiveEvents`. Novos tipos do mesmo mod usam `<Projetil>LiveEvents`.
3. Eventos passam pela **mesma** FIFO64, capacidade 64 e limite agregado de 25.000 callbacks, e são executados pelo **mesmo worker**. Não há nova thread MonoVM por mod, nem callback C# por frame.
4. A identificação é pelo **tipo de projétil** + binding imutável enquanto armado. O worker resolve o caminho da DLL e a classe específica e mantém o cache por mod e classe.
5. `sourceLiveProjectileStatus()` expõe o modId de cada vínculo adicional e os contadores `queued`, `processed`, `hits`, `kills`, `failures`, `dropped` e `lastResult`.
6. `sourceLiveModPause(modId, "CONFIRM_PAUSE_MOD")` **somente desarma os callbacks adicionais do mod escolhido**, recusando a operação enquanto existir callback em execução; eventos já enfileirados são descartados antes da invocação. **Isso não descarrega a DLL da MonoVM nem remove o conteúdo registrado no Terraria.** Não chamar de hot-unload completo.

Limite desta etapa: até sete vínculos adicionais **ao todo**, inclusive de outros mods, com uma sessão primária. Não está provado isolamento completo de ciclo de vida, memória, interdependências, mod unload/reload ou multiplayer.

## Segundo pacote de teste

Além do `ExampleModCSharp-v0.1.9.bmod`, a release experimental inclui `SecondExampleModCSharp-v0.1.0.bmod`, com:

- UID diferente: `2e508d4f-f192-4a21-83b2-496e6c482c75`.
- `SecondWand` e `SecondBolt`, com sprites reaproveitados do SourceCSharpMod e efeito vanilla OnFire.
- Uma **segunda assembly gerenciada** com classe `BunnyLoader.Source.BunnyLiveEvents`, mesmo nome da primeira, mas com estado próprio e retornos na faixa **3000+ para hits** e **4000+ para kills**.
- A primeira arma original e o ExampleWaveBolt permanecem idênticos à .74.

## Passos para o Android

1. Instalar a APK .75 experimental **por cima da .74**, sem limpar dados, e manter backup de um mundo de teste.
2. Ativar **simultaneamente** ExampleModCSharp 0.1.9 e SecondExampleModCSharp 0.1.0, sem duplicar versões.
3. Entrar em singleplayer, observar ambos os mods em `sourceAutoStatus()` com `phase=ready`; esperar a compilação inicial, que pode demorar mais por haver uma segunda assembly.
4. Atirar com a `ExampleWand`, a `ExampleWaveWand` e a `SecondWand` contra inimigos. Verificar direção inicial do disparo conforme a mira, efeitos, movimento e FPS.
5. Executar este diagnóstico no editor:

```javascript
JSON.stringify({
  runtime: bl.csharp.hostInfo(),
  mono: bl.csharp.managedAvailable(),
  auto: bl.csharp.sourceAutoStatus(),
  global: bl.csharp.sourceLiveHitStatus(),
  vinculados: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

**Critérios para aprovar a carga:** `mono=true`, dois `auto` `ready`, segundo vínculo com `modId=2e508d4f-f192-4a21-83b2-496e6c482c75`, `callbackClass=BunnyLiveEvents`, `enabled=true`, `queued=processed > 0`, `lastResult` pelo menos na faixa de 3000 ou 4000 conforme o evento. `global.queued=global.processed` após drenar, `dropped=0`, `lockDrops=0`, `failures=0` e duas curvas originais preservadas. Registrar FPS visual separado do `updateHz` do profiler.

Se a segunda assembly falhar ao carregar, ou o primeiro mod perder callbacks, **não promover .75**. Voltar à .74 e enviar o log do Bunny Loader.

## Teste opcional de pausa

Apenas depois de confirmar os dois mods e quando a fila estiver vazia:

```javascript
bl.csharp.sourceLiveModPause(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_PAUSE_MOD"
)
```

Verificar que o binding da segunda assembly fica `enabled=false`, enquanto os callbacks originais continuam processando. A pausa é somente dos eventos gerenciados; os itens e projéteis permanecem registrados, sem reinicialização da assembly. É esperado que novos eventos da segunda arma deixem de ser contabilizados após a pausa.

**Preservação:** a .74 continua a referência estável até a .75 passar no Android. Nenhuma alteração de logs do editor foi incluída na .75.
