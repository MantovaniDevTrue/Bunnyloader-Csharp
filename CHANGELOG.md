# Changelog — suporte experimental C#

As alterações abaixo referem-se ao **protótipo C# neste repositório**, não às versões oficiais do Bunny Loader de Jonata.

## 0.1.10.16.67 — correção de assembly para benchmark QuickJS × MonoVM (aguarda Android)
- Corrige a procura do método da .66: passa a usar uma assembly versionada `BunnyVmBenchmark67.dll`, `Kernel.Execute(int,int)`, com namespace independente do probe principal.
- GitHub Actions compila o novo DLL, carrega a assembly resultante por reflexão e **executa 1 milhão de iterações em C#**, exigindo checksum `1336190628`, antes de empacotar o APK e gerar o manifesto SHA-256.
- Mantém as fórmulas QuickJS/C#, os métodos de gameplay, o ExampleModCSharp 0.1.7 e as medições anteriores; nenhum resultado de velocidade MonoVM foi validado no Android ainda.

## 0.1.10.16.66 — benchmark executado parcialmente no Android (sem comparação)
- `vmCompareStart(iterations)` e `vmCompareStatus()` executam o mesmo algoritmo de inteiros em QuickJS e no BunnyManagedProbe.dll através da thread MonoVM persistente.
- Aquecimento fora da janela; resultado exato obrigatório; tempos de parede/CPU da thread e RSS do processo, sem alterar o fluxo de combate e o ExampleModCSharp 0.1.7.
- **Android da .66:** QuickJS concluiu 1.000.000 de iterações (`jsWallUs=341098`, `jsThreadCpuUs=338308`, checksum `1336190628`), mas o MonoVM falhou na busca de `BenchmarkMath(int,int)` (`monoWallUs=0`). Comparação inconclusiva; runtime inicializou com `result=42`.

## 0.1.10.16.65 — gameplay performance profiler VALIDADO no Android
- Profiler opt-in de 15 segundos: taxa/p90 aproximado de `Main.DoUpdate`, atraso na fila FIFO64 e duração média/máxima de callbacks gerenciados, sem logs por evento.
- Comandos `sourcePerfStart()`, `sourcePerfStatus()` e `sourcePerfStop()`; armamento adiado para o retorno ao jogo após fechar o editor.
- Otimização: evita copiar `modId` e caminho da DLL em toda chamada C#, mantendo proteção de `inFlight` contra troca da sessão.
- Todo o monitoramento pesado fica inativo por padrão. FIFO64, teto de 25 mil e benchmark .64 preservados; ExampleModCSharp 0.1.7 inalterado.
- **Android confirmado:** 907 atualizações em 15.007 ms (`updateHz=60.504`), p90 aproximado por bucket até 20 ms, 0 intervalos acima de 25/33 ms, 30 callbacks perfilados (espera média 50,2 µs; invocação média 195,8 µs), 0 falhas/descartes e pico FIFO 1/64. Usuário confirmou gameplay fluido.
- A medição `updateHz` refere-se a `Main.DoUpdate`, **não é FPS medido no renderer**. Falta comparação controlada com .64/QuickJS e CPU/RSS.

## 0.1.10.16.64 — teste de 25 mil callbacks gerenciados VALIDADO no Android
- `sourceLiveBenchmarkStart('CONFIRM_25000_CSHARP_CALLBACKS')` inicia gerador C++ independente que enfileira callbacks reais no FIFO64 e na thread proprietária MonoVM.
- `sourceLiveBenchmarkStatus()` informa contadores e tempo; `sourceLiveBenchmarkStop()` cancela. Timeout de 120s.
- Nenhum inimigo/projétil artificial; o benchmark ocupa a ponte C# desta sessão até a reinicialização do aplicativo, preservando os efeitos nativos e a versão .63.
- **Android comprovado:** `passed=true`, 25.000 callbacks reais executados (`OnHitNPC=12500`, `OnKill=12500`), 0 falhas/descartes, em **12.336 ms**, fila máxima 13 de 64, drenagem e desligamento esperados. O jogo continuou funcional.
- O teste foi sintético e rodou pelo editor (jogo congelado): **FPS e memória sob carga com gameplay ativo ainda não foram aferidos**.

## 0.1.10.16.63 — execução acima de 512 validada no Android
- Fila circular FIFO de capacidade 64 para eventos gerenciados e limite experimental de 25 mil callbacks aceitos.
- Teste real Android: **1.156 callbacks gerenciados** (515 OnHitNPC, 641 OnKill), **0 falhas**, `enabled=true` e `phase=ready`, usuário relatou **60 FPS**.
- Maior ocupação da fila: **2 de 64**. **12 descartes por mutex** (`dropped=12`, `lockDrops=12`), nenhum por fila cheia. O teto de 25 mil **ainda não foi atingido no aparelho**.
- Em caso de limite, bloqueia a entrada e drena eventos aceitos; mantém a thread de jogo não bloqueante e falhas limitadas a 3.
- Novas métricas de ocupação/descartes; os efeitos, a compilação Roslyn e o ExampleModCSharp 0.1.7 não foram modificados.
- CI não substitui medição real de FPS/memória nem verificação de 25 mil execuções no Android.

## 0.1.10.16.62 — VALIDADA no Android (callbacks experimentais)
- Sinal de mundo carregado via WorldFile.LoadWorldTiles + jogador ativo por três quadros de Main.DoUpdate.
- Suporte a detecção do mundo quando Main.get_gameMenu está indisponível.
- O status informa `worldReady` independentemente de `gameMenu`, sem remover gates de singleplayer, efeitos ou proteções GC.
- Mantido ExampleModCSharp 0.1.7. Teste real no Xiaomi Android 14: **512 callbacks gerenciados** (187 `OnHitNPC`, 325 `OnKill`), **0 falhas**, 9 tentativas descartadas por fila/limite. Após `phase=limit-reached`, jogo continuou funcionando; FPS não medido quantitativamente.

## 0.1.10.16.61 — hotfix insuficiente no Android
- Corrigido o bloqueio da vinculação automática: `Main.gameMenu` é uma propriedade acessada por `get_gameMenu()`, não um campo disponível na .60.
- Retém verificação de singleplayer, fallback e todas as políticas de combate e o MonoVM.
- O teste Android mostrou `Main.get_gameMenu indisponivel`; os callbacks automáticos ainda não ativavam. Corrigido na .62 com sinal real de mundo/jogador.

## 0.1.10.16.60 — aguardando validação Android
- Inclusão de registro automático experimental dos eventos gerenciados `OnHitNPC` e `OnKill`.
- ExampleModCSharp **0.1.7** com `BunnyLiveEvents.cs`.
- GitHub Actions aprovado. Funcionamento no aparelho **ainda em teste**.

## 0.1.10.16.59 — teste real relatado
- Execução gerenciada opt-in de `OnHitNPC` pelo MonoVM, com fila não bloqueante.
- Registro de teste: **10/10 eventos**, sem falha/perda.

## 0.1.10.16.58 — baseline de estabilidade
- Transições `GC safe/unsafe` corrigidas ao aguardar na thread proprietária MonoVM.
- Relatados `DOMAIN_ONLY` e `PINNED_MONO` com execução gerenciada `42` no Android.

Histórico técnico completo e links: [docs/HISTORICO.md](docs/HISTORICO.md).
