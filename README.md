# Bunny Loader C# — desenvolvimento para Terraria Mobile

**Nosso objetivo é adicionar suporte a mods C# ao [Bunny Loader original de Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader).** Já comprovamos a execução de código C# em testes reais no Android e estamos ampliando essa integração por etapas.

> **Projeto independente, experimental e em desenvolvimento.** Não representa uma versão oficial do Bunny Loader, não usa a API do tModLoader como base do código e **ainda não roda qualquer mod de PC automaticamente**.

## Onde está o desenvolvimento C#

### **[Abrir a branch de desenvolvimento C# →](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development)**

A branch **`csharp-development`** contém os fontes do host C++/MonoVM, compilador Roslyn, ExampleModCSharp e documentação organizados. Ela foi criada a partir do snapshot **v0.1.10.16.60**, que permanece intocado para testes.

Este branch **`main`** também guarda o workspace e a cópia sincronizada da base Bunny Loader. **Para consultar ou alterar a integração C#, utilize `csharp-development`**, não presuma que o estado da `main` é a última versão experimental C#.

## Andamento real (10/10/2026)

| Parte do projeto | Estado |
| --- | --- |
| MonoVM ARM64 e execução gerenciada | **Confirmados no Android** no contexto dos testes da .58 (retorno gerenciado 42 e thread estabilizada) |
| Compilação experimental de arquivos `.cs` com Roslyn | **Implementada e testada** para exemplos específicos |
| Callback C# de impacto (`OnHitNPC`) | **10/10 eventos reais** no teste da .59, sem falhas/perdas relatadas |
| Callbacks automáticos `OnHitNPC` + `OnKill` | **Implementados na .60 e CI aprovado**; aguardando confirmação em jogo |
| API C# completa, IA arbitrária por frame e multiplayer | **Não validados / não liberados** |

**Versão de teste atual:** [Bunny Loader C# v0.1.10.16.60 + ExampleModCSharp 0.1.7](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60).

## Organização do repositório

| Acesso | O que encontrar |
| --- | --- |
| [`src/`](src/README.md) | Onde abrir o **código-fonte canônico** da extensão C# |
| [`examples/`](examples/README.md) | ExampleModCSharp e demonstrações suportadas |
| [`tests/`](tests/README.md) | Testes de regressão e protocolo Android |
| [`docs/`](docs/README.md) | Arquitetura, histórico, limitações e roadmap |
| [`Split/Bunny-Loader/`](Split/Bunny-Loader/) | Fonte do projeto Bunny Loader no workspace, conservada para builds |
| [`.github/workflows/`](.github/workflows/) | Build e verificações CI |

Os diretórios `src/`, `examples/` e `tests/` desta `main` apontam para **a branch de desenvolvimento C#**. Nela há atalhos de código sem cópias, preservando caminhos Gradle/CMake.

## Créditos, limites e responsabilidade técnica

- **Projeto original e referência:** [JonataOliveiraa/Bunny-Loader](https://github.com/JonataOliveiraa/Bunny-Loader).
- **Integração experimental C#:** [MantovaniDevTrue/Bunnyloader-Csharp](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp).
- **Terraria:** pertence aos seus respectivos titulares. O repositório não deve distribuir o runtime nem assets proprietários do jogo.

Aprovação do GitHub Actions **não garante** execução estável do Terraria, nem 60 FPS. Usamos o [quadro de validação da branch C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/docs/STATUS.md) para separar implementação, CI e resultados medidos no aparelho.

Para acompanhar o momento exato do desenvolvimento, leia o [teste Android da .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/docs/TESTE-ANDROID-v60.md).
