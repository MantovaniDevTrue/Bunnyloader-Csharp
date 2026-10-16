# Changelog — suporte experimental C#

As alterações abaixo referem-se ao **protótipo C# neste repositório**, não às versões oficiais do Bunny Loader de Jonata.

## Consolidação do repositório — 16/10/2026

- Reúne o código da linha C# até a PR #34 (.90.29.62) como entrada pública integrada.
- Atualiza README, SDK/arquitetura, roadmap, catálogo de exemplos, build, testes e contribuição.
- Separa relatórios históricos; remove o espelho antigo da ponte JS e corrige o atalho para o SDK C#.
- Adiciona checkpoint verificável, guarda de links/caminhos/versões/pacotes e CI de organização.
- Preserva versionCode, fontes de produção e conteúdo dos pacotes da .62.

## 0.1.10.16.90.29.62 — primeiro tick de projétil

- Antecipa o primeiro tick C# sem remover cadência posterior ou validação de identidade/sessão.
- Build/CI aprovados; Mantovani confirmou: “.62 foi perfeita”. Contadores/FPS da sessão não informados.

## 0.1.10.16.90.29.59–.61 — combate e telemetria

- Hit/kill de NPC e projétil pelo worker, telemetria persistente e ciclo C# mesmo com movimento nativo prioritário.
- .61 tem eventos executados no Android, com rejeições registradas. [Evidências](docs/STATUS.md).

## 0.1.10.16.90.29.58 — IA de entidades e base nativa

- IA NPC/projétil por snapshot no MonoVM, preservando as correções nativas da .57.
- Validação Android confirmada pelo usuário; PR #31 registra 60 FPS.

## 0.1.10.16.90.29.57 — equipamento, Color e luz

- Corrige contratos reais de equipamento/animação, Color ABGR e getter de WorldItem.active.
- Carregamento de 2 mods C# sem erros/avisos e save/reabertura confirmados. [Detalhes](docs/csharp-pr28-native-contract-fixes.md).

---

As notas seguintes preservam checkpoints anteriores, com suas próprias versões e evidências.

## 0.1.10.16.70 — profiler de gameplay real (aguarda build/Android)
- Reutiliza `sourcePerfStart()`, `sourcePerfStatus()` e `sourcePerfStop()` da .65. Janela opt-in de 15 s, sem alterações em combate ou efeitos e sem geração artificial de 25 mil eventos.
- Adiciona `updateP99Ms`, percentis p90/p99 de fila e duração de callback C#, média/máximo/p90 de CPU da thread MonoVM, contadores de eventos aceitos/processados/falhos/descartados e pico de fila na janela.
- Instrumenta **curvas reais de projéteis C#** em `applyManagedCurve` somente quando o monitor está ativo, contando `projectileCurveUpdates` e `projectileVelocityWrites`, sem mudar trajetória, dano ou quantidade de efeitos.
- Histogramas de tamanho fixo e atomics sem alocação por evento; resultados detalhados só são construídos ao solicitar status. Proíbe iniciar o profiler junto com benchmark de VM.
- **O Hz aqui é frequência de updates, não FPS de renderização.** Não interpretar como prova de C# mais rápido que JS no gameplay; exige amostra Android em cena semelhante e contabilização de callbacks reais.
- Mantém .69 (simulações), .68 (matemática), limites de segurança, MonoVM e ExampleModCSharp 0.1.7.

## 0.1.10.16.69 — simulações de IA de projéteis e NPCs em QuickJS × MonoVM (Android validado)
- Adiciona `bl.csharp.vmGameplayCompareStart("projectile", ticks)` e `vmGameplayCompareStart("npc", ticks)`, mantendo `vmCompareStart` da .68.
- Ambos usam 64 estados de entidades simuladas, operações determinísticas de perseguição, movimentação/limites e decisão/cooldown, e checam igualdade exata do resultado calculado em QuickJS e na DLL C# gerenciada `BunnyVmGameplay69.dll`.
- Novos status `kind`, `entities`, `updates`, latência CPU/parede e checksums no `vmCompareStatus()`. Até quatro testes por sessão, com 64–5.000 ticks por teste de IA, sem mutar o mundo Terraria ou os mods instalados.
- Build verifica DLL contra as funções JS usando três durações e SHA-256 de entrega ao processo do jogo.
- **Android confirmado:** ambos os kernels (64 entidades simuladas, 2.000 ticks) passaram, sem erro. Projéteis: QuickJS 126.129 µs / MonoVM 4.598 µs (27,431×), checksum igual -639270423. NPCs: QuickJS 141.222 µs / MonoVM 6.566 µs (21,508×), checksum igual -698099503. Mais duas repetições por tipo confirmadas (total três execuções por modo); todas passaram.
- **Repetições Android (2.000 ticks, 128.000 atualizações):** projéteis #2 223.459/6.074 µs (JS/MonoVM), #3 131.655/4.811 µs; NPCs #2 165.476/7.737 µs, #3 162.301/7.388 µs. Checksums idênticos nas seis execuções; `phase=matched`, erro vazio. Razão de CPU por thread no modo projéteis entre 26,65×–28,97× e no modo NPCs entre 21,06×–21,90×. Outlier em tempo decorrido de QuickJS projéteis #2 acompanhado de aumento do tempo não-CPU da thread.
- **Não é IA executando dentro do Terraria nem medição de FPS/custo total da ponte de mod**. Próxima fase: medir callbacks e gameplay reais.

## 0.1.10.16.68 — correção e paridade QuickJS × MonoVM VALIDADAS no Android
- Corrige uma lacuna de instalação da .67: embora o APK e o CI contivessem `BunnyVmBenchmark67.dll`, o processo de jogo não garantia extrair novas DLLs caso o serviço/worker C# não fosse iniciado após atualização.
- `GameActivity` agora confere SHA-256 do manifesto, copia a DLL apenas quando ausente/desatualizada, valida após instalar e registra o resultado no log. Executa antes do início dos hooks nativos, sem depender de recompilar mods.
- **Android confirmado na .68:** `passed=true`, `phase="matched"`, 1.000.000 iterações e checksum `1336190628` em ambos os runtimes. QuickJS **445.536 µs** (CPU 439.607 µs); MonoVM **6.040 µs** (CPU 5.798 µs). Razões observadas de **73,764×** por tempo decorrido e **75,820×** por CPU da thread, exclusivamente nesse kernel de matemática.
- **Repetibilidade Android (mesmo tamanho, 1M × 3):** QuickJS 334,217/334,131/340,668 ms, MonoVM 4,532/4,499/4,491 ms; checksums 1336190628 idênticos nas três execuções; média 336,339 contra 4,507 ms, razão de médias 74,62× para o mesmo kernel matemático.
- Ainda não comparar como vantagem geral de FPS, memória ou execução de mods; resta investigar custos da ponte e callbacks, além de IA/projéteis no gameplay real.
- Sem mudanças na matemática do comparador, nos mods de exemplo, FIFO64, callbacks ou no comportamento de combate validado da .65.

## 0.1.10.16.67 — assembly compilada, mas faltando no processo Android
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
