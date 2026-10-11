# Bunny Loader C# .80 — validação real Android (11/10/2026)

## Identificação e rastreabilidade

- **APK:** Bunny Loader `v0.1.10.16.80-csharp-projectile-profiler`, Android 14 arm64-v8a, Terraria 1.4.5.8.6 (301720).
- **Dispositivo:** Xiaomi 2109119DG (API 34).
- **Mods C#:** ExampleMod CSharp 0.1.9 (`examplemodcsharp`) e Second Example CSharp 0.1.0 (`secondexamplecsharp`).
- **Build:** [GitHub Actions #38171225259](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38171225259) — success.
- **APK publicada:** [release .80](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.80-projectile-profiler); release commit `ae787455f3f7adada861d211809cf8bf60c99438`.
- **Branch de código exato da APK testada:** [csharp-android-validated-v80](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-android-validated-v80).
- **Baseline anterior/rollback:** .79.1. Documentação da .80 mantida separada da branch principal porque `csharp-development` e o histórico experimental divergem.

## Resultado do profiler por arma — teste opt-in de 15 s

- `phase=completed`, `updateHz=59.77559069091243` (frequência de atualização da engine).
- **FPS visual** informado pelo usuário: 60, uma única queda breve para 59.
- `callbacksSampled=85`, `eventsProcessed=85`, `eventsFailed=0`, `eventsQueueDropped=0`, `lockDrops=0`.
- `1137`: 23 callbacks, 11 hits, 12 kills, wall médio 71,78 µs; pico 318 µs.
- `1136`: 35 callbacks, 18 hits, 17 kills, wall médio 148,63 µs; pico 2421 µs.
- `1138`: 27 callbacks, 13 hits, 14 kills, wall médio 97,78 µs; pico 1380 µs.
- **Conferência:** 23 + 35 + 27 = 85; hits + kills = total em cada projétil; nenhum erro informado.
- Tempos incluem a ponte de execução; a diferença de médias não mede a velocidade relativa da lógica C# isoladamente. Não confundir `updateHz` com FPS de renderização.

## Regressão funcional — seis verificações

**Todos os seis cenários foram confirmados como aprovados pelo usuário**, sem depender de console para o carregamento dos mods:

1. FPS estável em jogo (sem quedas relevantes).
2. Três armas dos dois mods operando com o profiler desligado.
3. Pausa seletiva do segundo mod mantendo o primeiro em funcionamento.
4. Retomada dos callbacks C# do segundo mod.
5. Salvamento, saída, reentrada e rebinding automático.
6. Ausência de erros ou eventos perdidos observados nos testes.

Esta confirmação do usuário é o resultado de uma sequência de teste funcional no aparelho; não substitui testes sintéticos de corrida, leak prolongado, multiplayer ou cobertura de todas as APIs.

## Relatório do Bunny Loader gerado em 2026-10-11 17:52

**MonoVM da sessão atual**: `timestampUnix=1791751544`, `pid=17650`, `phase=verified`, `libraryLoaded=1`, `symbolsReady=1`, `domainInitialized=1`, `methodExecuted=1`, `result=42`, `error=` vazio.

Worker isolado anterior: `state=finished`, `phase=verified`, `result=42`; last probe `phase=compile-verified`. Não confundir o worker anterior com a execução corrente de callbacks do processo de jogo.

**Log do Bunny, última partida:** `(nenhum erro ou aviso)`; rebinding automático 1137 (OnHitNPC/OnKill), 1136 (ExampleWaveBoltLiveEvents) e 1138 (mod adicional) após `mundo carregado e jogador ativo confirmados`; `callbacks pausados ao sair do mundo`. Salvamento do personagem e do mundo registrado normalmente; logcat da sessão atual termina no `UnityPlayer lifecycle: onUnityPlayerQuitted`.

**Histórico de encerramentos:** aparecem `SINAL status=9` e `SEM MEMORIA status=0`, e um `NATIVE_ABORT_SIGNAL pid=28566 tid=28672` legado em arquivos de trace. Não datam um novo SIGABRT da sessão 17:52; nem demonstram que o processo atual ficou sem memória. Eventuais encerramentos futuros precisam ser correlacionados a PID, horário e logcat da sessão correspondente antes de atribuir causalidade ao C#.

**Persistência:** houve restauração de 12 itens de mod no personagem, 7 indicados como `?`; no mundo existem 10 tiles de mod não carregado preservados no sidecar `.wld.tiles.bl`, além de 23 itens em baús mantidos no arquivo de mod auxiliar. Isso não prova perda, mas requer cautela: manter cópia do mundo, personagem e sidecars e evitar limpeza automática dos desconhecidos. Investigação específica de compatibilidade de itens poderá ser feita sem mexer no sistema de callbacks validado.

## Decisão

**.80 aprovada como baseline funcional do C# para os cenários acima**, preservando .79.1 como rollback. Não implementar mudanças experimentais diretamente na branch de código validado. Priorizar futura expansão da API baseada em fontes reais do Bunny Loader/ExampleModBunny com testes independentes; sem tModLoader e sem remoção de efeitos.

