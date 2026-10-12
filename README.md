# Bunny Loader C# — desenvolvimento experimental

## Código-fonte navegável (corrigido em 12/10/2026)

**[Abrir src/ — pastas reais no GitHub](src/README.md)**. As antigas referências simbólicas foram substituídas por árvores Git navegáveis. Agora é possível abrir diretamente [Compiler.cs](src/roslyn-compiler/Compiler.cs), [SDK C#](src/csharp-api/BunnySourceScriptApi.cs), [host MonoVM](src/csharp-host/ManagedRuntime.cpp) e [ExampleMod C#](src/examplemod-csharp/). Os caminhos canônicos de build continuam em Split/Bunny-Loader/; src/ é um espelho para navegação.

**Estado atual:** [v.86 validada no Android](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-managed-npc-impulse-v86-experiment) com 473 V4, 87 empurrões visíveis, debuffs e 60 FPS; [catálogo de API .87](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-api-discovery-catalog-v87-experiment) em desenvolvimento. O histórico descrito abaixo documenta etapas anteriores.

**Objetivo:** adicionar suporte a mods escritos em **C#** ao [Bunny Loader original, desenvolvido por Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader), mantendo a arquitetura e os recursos do loader Android. **Já conseguimos compilar e executar código C# em testes reais no Terraria Mobile; a integração geral ainda está em desenvolvimento.**

> **Projeto independente de pesquisa e desenvolvimento.** Esta extensão **não é o Bunny Loader oficial**, não é uma implementação do tModLoader e ainda não oferece compatibilidade geral com mods de PC. O projeto original do Jonata continua sendo a referência do loader; os componentes de C# são experimentais.

## Estado atual — 11/10/2026

| Etapa | Situação comprovada |
| --- | --- |
| MonoVM ARM64 no Android | Testes reais confirmados na **v0.1.10.16.58**, incluindo execução gerenciada com resultado `42` e estabilização da thread persistente |
| Compilação de arquivos `.cs` com Roslyn | Fluxo experimental já implementado e exercitado no ExampleModCSharp; **não** significa suporte irrestrito a qualquer mod C# |
| Conteúdo C# (itens, projéteis e exemplos) | Integrações e testes específicos já funcionaram; parte do comportamento continua mediada por políticas nativas/precalculadas |
| `OnHitNPC` gerenciado na versão **.59** | Relato documentado: **10/10 eventos reais**, zero falhas/perdas |
| `OnHitNPC` e `OnKill` automáticos na **.62** | **Confirmados no Android**: 512 callbacks gerenciados (187 hits, 325 kills), 0 falhas; jogo continuou funcional após o limite de segurança |
| Compatibilidade universal, AI C# por frame, multiplayer | **Não comprovados/não liberados** |

**Baseline de combate validada:** [v0.1.10.16.63](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.63), 1.156 callbacks de combate real, 0 falhas, 60 FPS relatados, 12 descartes de contenção. **Benchmark MonoVM validado:** [v0.1.10.16.64](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.64), 25.000 callbacks C# reais, 0 falhas ou descartes, 12,336 segundos. **.65 validada no Android:** perfil de gameplay de 15 segundos (907 atualizações, ~60,5 Hz de `Main.DoUpdate`, sem intervalos acima de 25 ms), 30 callbacks gerenciados amostrados, fila média 50,2 µs, execução média 195,8 µs, 0 descartes/falhas e gameplay fluido confirmado. **.66 no Android:** QuickJS executou 1 milhão de iterações em 341 ms, mas a procura do método C# falhou e não há comparação válida. **.67 no Android:** QuickJS concluiu 1 milhão de iterações em ~338 ms, mas o processo do jogo não encontrou a nova DLL de benchmark; MonoVM não executou esse teste. **.68 validada no Android:** a DLL dedicada foi encontrada e a comparação matemática QuickJS × MonoVM terminou com `matched`, `passed=true` e checksum idêntico `1336190628`. Para 1 milhão de iterações, QuickJS levou **445,536 ms** e MonoVM **6,040 ms**, razão de **73,764× nessa única execução do núcleo matemático** — isso não representa comparação de FPS de jogo ou superioridade geral sobre QuickJS. [Relatório .68](docs/vm-parity-v68.md).

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

Na .62, o carregamento automático detecta um mundo carregado e um jogador ativo em singleplayer antes de ativar os callbacks de convenção do **ExampleModCSharp 0.1.7**. O teste atual vincula no máximo **um tipo de projétil de um mod por processo**. Os eventos são colocados em fila para execução no worker Mono, sem bloquear o frame do jogo. A IA por frame segue caminhos nativos/precalculados em vez de chamar Mono a cada frame.

Consulte a [arquitetura e os limites](docs/ARQUITETURA.md) antes de adicionar código.

## Teste atual

O [teste real da **.62**](docs/mono-auto-live-world-signal-v62.md) confirma a inicialização MonoVM e os dois callbacks automáticos. A [versão .60](docs/TESTE-ANDROID-v60.md) permanece documentada historicamente como teste anterior; a .59 exigia ativação manual. Próxima etapa: medir FPS/memória e rever com segurança o limite de 512 e a capacidade da fila.

## Créditos e origem

- **Bunny Loader original:** [JonataOliveiraa/Bunny-Loader](https://github.com/JonataOliveiraa/Bunny-Loader).
- **Esta iniciativa:** [MantovaniDevTrue/Bunnyloader-Csharp](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp), estudo e integração experimental de C# no Bunny Loader Android.
- **Terraria:** propriedade de seus respectivos titulares. O código, os binários e assets proprietários do jogo **não são disponibilizados como parte deste projeto**.

**Importante:** não confundir a confirmação do funcionamento de um recurso específico com suporte completo da API ou com compatibilidade com mods tModLoader. O [quadro de validação](docs/STATUS.md) distingue implementação, CI e teste real.
