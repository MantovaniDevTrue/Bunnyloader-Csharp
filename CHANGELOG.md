# Changelog — suporte experimental C#

As alterações abaixo referem-se ao **protótipo C# neste repositório**, não às versões oficiais do Bunny Loader de Jonata.

## 0.1.10.16.63 — aguardando validação Android
- Fila circular FIFO de capacidade 64 para eventos gerenciados e limite experimental de 25 mil callbacks aceitos.
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
