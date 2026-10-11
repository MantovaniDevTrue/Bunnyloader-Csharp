# Bunny Loader C# .76 — World Lifecycle e pausa seletiva

**Estado:** implementação experimental e CI pendente; **sem validação Android**. Referência aprovada: **.75** (dois mods simultâneos durante Blood Moon com 100 inimigos, 60 FPS relatados, 256/256 eventos processados, zero falhas).

## O que foi alterado

- Pausa `sourceLiveModPause(modId, "CONFIRM_PAUSE_MOD")` agora desarma **apenas os callbacks do mod adicional escolhido** e remove os eventos ainda pendentes **somente daquele mod** na FIFO64, preservando a ordem dos demais. Os eventos removidos são contabilizados em `dropped` global e individual. Se um callback do próprio mod estiver em execução, retorna erro e recomenda tentar novamente.
- O código do carregador registra as identidades dos mods aptos ao auto-live e os guarda durante a sessão do app.
- Cada entrada em mundo recebe um novo **world epoch**. Ao voltar ao menu, o carregador desarma os callbacks gerenciados e descarta eventos pendentes, mas mantém as DLLs Roslyn/MonoVM e os conteúdos já registrados.
- Após a próxima entrada em singleplayer, o carregador tenta vincular novamente os callbacks dos mods conhecidos sem recompilar os mods e sem criar um segundo worker MonoVM. A verificação do menu acontece a cada 30 updates, sem operação de reflexão em cada quadro.
- O rearmamento e o encerramento da sessão são sincronizados para evitar que o vínculo atrasado de um mundo antigo seja aplicado ao mundo novo.
- A arquitetura continua usando FIFO64 e limite agregado 25.000; duas classes independentes por assembly; NativeAOT fora do caminho principal; sprites e efeitos de armas mantidos.

**Isso NÃO é hot-unload:** a pausa não descarrega uma DLL, não apaga itens ou projéteis e não libera as assemblies do domínio MonoVM. A saída do mundo desarma apenas os callbacks. A liberação real de assemblies, alteração de mods habilitados no meio da sessão e multiplayer exigem outra arquitetura e testes adicionais.

## Teste Android — fase A: reentrada

1. Mantenha o backup do mundo de testes. Instale a APK .76 por cima da .75, sem desinstalar, e mantenha o `ExampleModCSharp 0.1.9` e o `SecondExampleModCSharp 0.1.0` ativados.
2. Entre no mundo em singleplayer. Com as três armas, ataque aproximadamente 20–40 inimigos.
3. Consulte o JSON abaixo, anotando `eventos.queued` e `projeteis[].processed`.
4. **Salvar e sair** para o menu do Terraria, aguardar dois segundos e entrar novamente no mesmo mundo. Repita o combate e obtenha um segundo JSON.
5. É esperado ver os dois `sourceAutoStatus()` com `phase=ready`, dois mods carregados, novos callbacks sendo processados na segunda entrada, `errors`/ `failures`/ `lockDrops` zerados e FPS próximo dos 60 relatados na .75.

```javascript
JSON.stringify({
  runtime: bl.csharp.hostInfo(),
  auto: bl.csharp.sourceAutoStatus(),
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

Os totais de `queued`/`processed` podem reiniciar a cada reentrada: o **que importa** é que cresçam novamente e continuem coerentes em cada sessão. `phase` pode exibir `armed` brevemente antes de `ready`.

## Teste Android — fase B: pausa seletiva (somente após fase A)

No mundo, depois de atacar com ambas as armas, execute:

```javascript
bl.csharp.sourceLiveModPause(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_PAUSE_MOD"
)
```

Um resultado `paused: true` significa somente que o **vínculo gerenciado** do segundo mod foi desarmado. Se aparecer `callback in flight`, aguarde um instante e repita. Consulte:

```javascript
JSON.stringify({
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

Verifique `SecondBolt.enabled=false`, mas `ExampleWaveBolt.enabled=true`, e continue atacando somente com as armas do primeiro mod. O contador da primeira deve crescer; o segundo não deve voltar a processar callbacks até sair e reentrar no mundo. Caso existissem eventos pendentes do segundo mod, `dropped` pode ficar **maior que zero de forma esperada**; isso representa cancelamento intencional, não perda por contenção ou fila cheia.

**Não desative os mods no gerenciador durante o combate.** Isso não está coberto por essa etapa.

## Critérios de aprovação

- Reentrada não trava, nem perde vínculos das duas assemblies;
- Pausa do segundo mod não interrompe o primeiro;
- Fila não mantém eventos do mundo anterior;
- Logs e `sourceLiveProjectileStatus()` permitem distinguir os modIds;
- FPS observado estável, sem regressão percebida.

A .75 continua o fallback estável até esses testes serem realizados.
