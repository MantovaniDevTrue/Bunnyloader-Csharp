# Bunny Loader C#

Mods C# em **Terraria Mobile**, com fontes `.cs` compilados no Android por
Roslyn e executados pelo MonoVM. O host C++ integra o código ao Terraria IL2CPP.
Não precisa de compilador externo nem de uma `.so` por mod.

Extensão independente do [Bunny Loader de Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader),
mantida por **Mantovani**. O caminho C# implementado de carregamento, receitas,
equipamentos e gameplay usa código nativo/gerenciado, sem adaptadores JavaScript.
Mods JS continuam disponíveis pelo motor original.

**Checkpoint atual: `0.1.10.16.90.29.62`**, a API segue
em evolução. [Versões, evidências e limites](docs/STATUS.md).

## Comece aqui

| Quero… | Caminho |
| --- | --- |
| **Integrar o C# ao Bunny original (Potato)** | **[Guia passo a passo, caminhos exatos e auditoria](docs/INTEGRACAO-BUNNY-ORIGINAL.md)** |
| Encontrar exemplos para adaptar | [Catálogo C#](examples/README.md) |
| Entender hooks, policies e threads | [Contratos do SDK](docs/SDK-CSharp.md) |
| Consultar classes e assinaturas | [Referência gerada](Split/Bunny-Loader/docs/generated/sdk_public_reference.md) |
| Compilar ou contribuir com o loader | [Build](docs/BUILD.md) · [Contribuição](CONTRIBUTING.md) · [Testes](tests/README.md) |
| Ver progresso e próximos lotes | [Estado atual](docs/STATUS.md) · [Roadmap](docs/SDK-ROADMAP.md) |

## Organização

| Diretório | Responsabilidade |
| --- | --- |
| [Split/Bunny-Loader/](Split/Bunny-Loader/) | Projeto canônico Android, C++, SDK, ferramentas e samples |
| [src/](src/README.md) | Navegação para os fontes canônicos, sem cópias paralelas |
| [examples/](examples/README.md) | Escolha de exemplos para mods independentes |
| [tests/](tests/README.md) | Comandos reproduzíveis e mapa das suítes |
| [docs/](docs/README.md) | Guias atuais e histórico separado |
| [.github/workflows/](.github/workflows/) | CI, build e manutenção; [mapa](docs/BUILD.md#workflows) |
| [Split/](Split/) | Integração histórica com o port Split e ferramentas associadas |

Os caminhos de compilação permanecem em `Split/Bunny-Loader`.
[Mapa detalhado do repositório](docs/REPOSITORIO.md).

## Suporte e próximos passos

Há carregamento automático antes do mundo, cache verificado, itens/receitas,
equipamentos, armaduras/conjuntos, prefixos, desenho limitado, hooks de dano do
jogador e IA/eventos de NPCs e projéteis via worker MonoVM. Policies nativas
e callbacks gerenciados têm contratos diferentes. As filas são limitadas e
eventos podem ser recusados: consulte os contratos e a telemetria antes de portar.

Ainda faltam categorias da matriz de exemplos, APIs de mundo/UI e um contrato
completo de multiplayer e hot-unload. Esta extensão não executa automaticamente
mods tModLoader. Presença de uma classe no SDK não comprova todos os seus usos
no jogo. Benchmarks do host não medem FPS no Android.

## Créditos e distribuição

O Bunny Loader original é de **Jonata Oliveira**. Consulte a
[licença do loader](Split/Bunny-Loader/LICENSE.md), o
[escopo de licenciamento](Split/Bunny-Loader/LICENSING.md) e os
[avisos de terceiros](Split/Bunny-Loader/THIRD_PARTY_NOTICES.md).
Terraria pertence aos seus respectivos titulares. Este repositório público
não fornece os binários ou assets proprietários do jogo; as builds de teste
com runtime integrado permanecem em distribuição restrita.
