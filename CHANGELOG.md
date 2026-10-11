# Changelog — suporte experimental C#

As alterações abaixo referem-se ao **protótipo C# neste repositório**, não às versões oficiais do Bunny Loader de Jonata.

## 0.1.10.16.61 — hotfix em validação
- Corrigido o bloqueio da vinculação automática: `Main.gameMenu` é uma propriedade acessada por `get_gameMenu()`, não um campo disponível na .60.
- Retém verificação de singleplayer, fallback e todas as políticas de combate e o MonoVM.
- Requer teste Android real; não altera o pacote ExampleModCSharp 0.1.7.

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
