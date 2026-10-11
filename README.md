# Bunny Loader C# — desenvolvimento experimental

**Objetivo:** adicionar suporte a mods escritos em **C#** ao [Bunny Loader original, desenvolvido por Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader), mantendo a arquitetura e os recursos do loader Android. **Já conseguimos compilar e executar código C# em testes reais no Terraria Mobile; a integração geral ainda está em desenvolvimento.**

> **Projeto independente de pesquisa e desenvolvimento.** Esta extensão **não é o Bunny Loader oficial**, não é uma implementação do tModLoader e ainda não oferece compatibilidade geral com mods de PC. O projeto original do Jonata continua sendo a referência do loader; os componentes de C# são experimentais.

## Estado atual — 10/10/2026

| Etapa | Situação comprovada |
| --- | --- |
| MonoVM ARM64 no Android | Testes reais confirmados na **v0.1.10.16.58**, incluindo execução gerenciada com resultado `42` e estabilização da thread persistente |
| Compilação de arquivos `.cs` com Roslyn | Fluxo experimental já implementado e exercitado no ExampleModCSharp; **não** significa suporte irrestrito a qualquer mod C# |
| Conteúdo C# (itens, projéteis e exemplos) | Integrações e testes específicos já funcionaram; parte do comportamento continua mediada por políticas nativas/precalculadas |
| `OnHitNPC` gerenciado na versão **.59** | Relato documentado: **10/10 eventos reais**, zero falhas/perdas |
| `OnHitNPC` e `OnKill` automáticos na **.60** | Implementados e aprovados no CI; **aguardando teste real no aparelho** |
| Compatibilidade universal, AI C# por frame, multiplayer | **Não comprovados/não liberados** |

**Build experimental mais recente desta linha:** [v0.1.10.16.60 — APK e ExampleModCSharp 0.1.7](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60). A compilação bem-sucedida no GitHub Actions não substitui testes no Android.

## Navegação do repositório

| Local | Conteúdo |
| --- | --- |
| [`src/`](src/README.md) | Atalhos **sem cópias** para os fontes do host C++, compilador Roslyn, ponte de API e exemplos `.cs` |
| [`Split/Bunny-Loader/`](Split/Bunny-Loader/) | Árvore canônica do projeto Android/NDK/Gradle; caminhos originais preservados para não quebrar os builds |
| [`examples/`](examples/README.md) | ExampleModCSharp, arquivos `.cs`, APIs realmente demonstradas |
| [`tests/`](tests/README.md) | Testes automáticos, verificações de regressão e teste manual no aparelho |
| [`docs/`](docs/README.md) | Arquitetura, andamento, limitações, histórico e planejamento |
| [`.github/workflows/`](.github/workflows/) | Builds e verificações do CI |
| [`Split/build/`](Split/build/) e [`Split/mod/`](Split/mod/) | Scripts e artefatos históricos do workspace Split, mantidos separados do suporte C# |

### Fluxo atual

`arquivos .cs` → `compilador Roslyn (worker isolado)` → `assembly gerenciado` → `MonoVM` → `ponte C++/Terraria Android`.

Na .60, o carregamento automático procura os callbacks de convenção do **ExampleModCSharp 0.1.7** após entrar em singleplayer. O teste atual vincula no máximo **um tipo de projétil de um mod por processo**. Os eventos são colocados em fila para execução no worker Mono, sem bloquear o frame do jogo. A IA por frame segue caminhos nativos/precalculados em vez de chamar Mono a cada frame.

Consulte a [arquitetura e os limites](docs/ARQUITETURA.md) antes de adicionar código.

## Teste atual

O procedimento reproduzível da **.60** está em [TESTE-ANDROID-v60.md](docs/TESTE-ANDROID-v60.md). Use o APK da .60 junto ao **ExampleModCSharp 0.1.7**; a .59 usa o ExampleMod 0.1.6 e um comando manual diferente.

## Créditos e origem

- **Bunny Loader original:** [JonataOliveiraa/Bunny-Loader](https://github.com/JonataOliveiraa/Bunny-Loader).
- **Esta iniciativa:** [MantovaniDevTrue/Bunnyloader-Csharp](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp), estudo e integração experimental de C# no Bunny Loader Android.
- **Terraria:** propriedade de seus respectivos titulares. O código, os binários e assets proprietários do jogo **não são disponibilizados como parte deste projeto**.

**Importante:** não confundir a confirmação do funcionamento de um recurso específico com suporte completo da API ou com compatibilidade com mods tModLoader. O [quadro de validação](docs/STATUS.md) distingue implementação, CI e teste real.
