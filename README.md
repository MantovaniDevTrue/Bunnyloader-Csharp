# Bunny Loader C# — desenvolvimento experimental

## Código-fonte navegável (corrigido em 12/10/2026)

**[Abrir src/ — pastas reais no GitHub](src/README.md)**. As antigas referências simbólicas foram substituídas por árvores Git navegáveis. Agora é possível abrir diretamente [Compiler.cs](src/roslyn-compiler/Compiler.cs), [SDK C#](src/csharp-api/BunnySourceScriptApi.cs), [host MonoVM](src/csharp-host/ManagedRuntime.cpp) e [ExampleMod C#](src/examplemod-csharp/). Os caminhos canônicos de build continuam em Split/Bunny-Loader/; src/ é um espelho para navegação.

**Estado atual:** [v.86 validada no Android](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-managed-npc-impulse-v86-experiment) com 473 V4, 87 empurrões visíveis, debuffs e 60 FPS; [catálogo de API .87](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-api-discovery-catalog-v87-experiment) em desenvolvimento. O histórico descrito abaixo documenta etapas anteriores.

**Objetivo:** adicionar suporte a mods escritos em **C#** ao [Bunny Loader original, desenvolvido por Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader), mantendo a arquitetura e os recursos do loader Android. **Já conseguimos compilar e executar código C# em testes reais no Terraria Mobile; a integração geral ainda está em desenvolvimento.**

> **Projeto independente de pesquisa e desenvolvimento.** Esta extensão **não é o Bunny Loader oficial**, não é uma implementação do tModLoader e ainda não oferece compatibilidade geral com mods de PC. O projeto original do Jonata continua sendo a referência do loader; os componentes de C# são experimentais.

## Estado atual — 11/10/2026

**Baseline funcional Android: [v0.1.10.16.80](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.80-projectile-profiler).** Release/CI aprovados e seis verificações de gameplay relatadas como concluídas, com **dois mods C#, três armas, pausa/retomada seletiva, reentrada e carregamento automático**; 60 FPS visual (uma oscilação a 59) no teste instrumentado. O profiler mediu 85/85 callbacks por arma, zero falhas, descartes ou lockDrops. A source branch da APK comprovada é [csharp-android-validated-v80](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-android-validated-v80), ancorada no commit `ae787455f3f7adada861d211809cf8bf60c99438`. **.79.1 segue rollback**. [Relatório de validação](docs/csharp-v80-android-validation.md).

| Subsistema | Confirmação real | Limite atual |
| --- | --- | --- |
| Roslyn + MonoVM ARM64 | Worker e chamada gerenciada verificados (`result=42`); mods C# carregados automaticamente | APIs parciais; compilação de fontes condicionada à integração implementada |
| Armas/projéteis | Dois mods, três tipos de projéteis, curvas e callbacks reais | Não há cobertura geral de todos os métodos de C# ou dos mods de PC |
| OnHitNPC/OnKill | Fila de eventos assíncrona com 85/85 amostras, zero perdas/falhas no profiler .80 | Capacidade FIFO64 e teto experimental agregado de 25.000 eventos |
| Ciclo de vida | Pausa e retomada de callbacks do segundo mod; save/exit/reentry e rebind automáticos | Não é hot-unload real de assemblies ou conteúdo |
| Desempenho | FPS relatado estável e profiler por projétil sob demanda | Sem garantias de FPS em todas as cenas, memória ou multiplayer |
| QuickJS × MonoVM | Kernel matemático pareado testado na .68 com checksum idêntico | Não extrapolar para performance de mods em gameplay |

**Limites:** multiplayer não validado, sem compatibilidade tModLoader, unload gerenciado seguro ainda não disponível, e não há prova de ausência de vazamento em uso prolongado. Logs históricos de encerramento não devem ser confundidos com erros da sessão atual.

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

### Fluxo e teste atual

`arquivos .cs` → `compilador Roslyn em worker isolado` → `assembly gerenciado` → `MonoVM no processo do jogo` → `ponte nativa Terraria Android`. O loader ativa automaticamente os vínculos de callbacks após reconhecer o mundo e o jogador ativos. A fila FIFO64 evita bloquear o frame aguardando a lógica C#; IA de projétil também utiliza curvas pré-calculadas quando apropriado.

O teste mais recente é a [validação Android da .80](docs/csharp-v80-android-validation.md). O [STATUS](docs/STATUS.md) separa o que está disponível, o que o CI comprova e o que foi observado no dispositivo. Para ampliar a API, usar os fontes do loader e do ExampleModBunny, não reutilizar diretamente APIs do tModLoader.

## Créditos e origem

- **Bunny Loader original:** [JonataOliveiraa/Bunny-Loader](https://github.com/JonataOliveiraa/Bunny-Loader).
- **Esta iniciativa:** [MantovaniDevTrue/Bunnyloader-Csharp](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp), estudo e integração experimental de C# no Bunny Loader Android.
- **Terraria:** propriedade de seus respectivos titulares. O código, os binários e assets proprietários do jogo **não são disponibilizados como parte deste projeto**.

**Importante:** não confundir a confirmação do funcionamento de um recurso específico com suporte completo da API ou com compatibilidade com mods tModLoader. O [quadro de validação](docs/STATUS.md) distingue implementação, CI e teste real.
