# Bunny Loader C# .76 — World Lifecycle e pausa seletiva

**Estado:** GitHub Actions [#38153843425](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38153843425) **success** e testes Android **aprovados funcionalmente** em 11/10/2026. A .76 é a nova referência para reentrada e pausa de callbacks; a .75 permanece disponível para rollback.

## Validação Android — reentrada e pausa seletiva (11/10/2026)

**APROVADA funcionalmente no cenário observado.** O usuário executou os dois ExampleMods C# na APK .76, pausou o segundo mod pelo editor, saiu e entrou novamente no mundo e confirmou que o mod pausado **voltou ativo normalmente**, com jogo fluido e **nenhuma queda de FPS observada**. É um relato visual, não um registro contínuo de frametimes ou uso de memória.

As capturas mostram:

| Medição | Resultado |
| --- | --- |
| Comando `sourceLiveModPause("2e508d4f-f192-4a21-83b2-496e6c482c75", "CONFIRM_PAUSE_MOD")` | `{"paused":true,"error":""}` |
| `SecondBolt` após pausa | `enabled=false` |
| `ExampleWaveBolt` após pausa | `enabled=true`, 66/66 eventos, 44 hits / 22 kills, 0 falhas e descartes |
| `SecondBolt` na captura após pausa | 58/58 eventos, 29 hits / 29 kills, 0 falhas e descartes, `lastResult=4066` |
| `SecondBolt` após sair e reentrar | **`enabled=true`**, binding gerenciado restaurado |
| Dois mods após reentrada | `phase=ready`, `cached=true`, `applied=true`, `worldReady=true`, `error=""` |
| Primeiro mod | 2/2 curvas ativas, políticas hit/kill 1/1 |
| Segundo mod | 0 curvas (esperado), políticas hit/kill 1/1 |
| Eventos globais na captura | **200/200** processados, 111 hits e 89 kills |
| Fila | Profundidade 0, máximo 3/64, `inFlight=false` |
| `dropped`, `lockDrops`, `failures` | **0 / 0 / 0** |
| FPS | Sem quedas percebidas durante os testes, segundo o usuário |

Os **66/66** eventos de `ExampleWaveBolt` e **58/58** de `SecondBolt` são contagens de vínculos adicionais já incluídas no total global **200/200**. A execução anterior do segundo mod é indicada pelo `lastResult=4066`, coerente com a callback independente de `SecondBolt`.

**Precisão do teste:** as capturas mostram claramente o vínculo `SecondBolt` desarmado (`false`) e posteriormente rearmado (`true`). Elas **não trazem uma comparação de contadores antes/depois de novos disparos após reentrada** para isolar essa subetapa quantitativamente; o usuário, porém, confirmou que todos os testes funcionaram. O teste de descarregamento real de assemblies (hot-unload), múltiplas ativações sob carga extrema, multiplayer e reinicialização sem perda de estado persistente permanece aberto.

**Decisão:** .76 é a nova baseline funcional Android para dois mods C# com pausa de callback por mod e reentrada de mundo. Manter .75 como rollback e não afirmar que a liberação de memória da DLL foi validada.

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

A .76 passou na validação funcional descrita acima. A .75 continua como fallback, e hot-unload verdadeiro permanece não implementado/não validado.
