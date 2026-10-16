# Bunny Loader C# — escalabilidade de mods grandes

Base: branch `codex/integrate-csharp-sdk-lot-a-20261015`, ExampleMod CSharp 0.1.39, MonoVM isolado e SDK C# sem dependência de QuickJS.

## Problema observado no Android

No Xiaomi 2109119DG, a primeira compilação do ExampleMod levou aproximadamente 13,5 segundos, mas com cache verificado o preparo caiu para 745 ms. O indicador nativo `CompileMs` inclui a espera pelo worker isolado, instalação/verificação do framework, Roslyn e execução dos defaults. Ele não equivale ao tempo de `Emit` do Roslyn.

## Limites por pacote C#

| Recurso | Anterior | Atual |
|---|---:|---:|
| Arquivos `.cs` | 512 | 4.096 |
| Tamanho por fonte | 256 KiB | 1 MiB |
| Total de fontes | 16 MiB | 32 MiB |
| Tipos C# reconhecidos | 1.024 | 8.192 |
| Tipos por categoria | 256 | 4.096 |
| Buffer dos defaults nativos | 2 MiB | 8 MiB |
| Buffer por tabela de movimento/combate | 1 MiB | 4 MiB |
| Assembly gerenciado | 8 MiB | 32 MiB |
| Profundidade de pastas no parser nativo | 7 | 24 |

Esses limites são proteções de memória e validade de arquivos para Android, não uma restrição de design do SDK. Modders não devem precisar separar o mod só porque há mais de 512 classes ou mais de 256 itens. A capacidade real de IDs de conteúdo, RAM disponível e ponte com Terraria deve continuar sendo testada no dispositivo.

## Otimizações e medições

- O compilador Roslyn atualiza o diagnóstico de análise uma vez a cada 64 arquivos, em vez de abrir/gravar o arquivo de progresso para cada classe.
- Enumerar arquivos é limitado antes de materializar toda a listagem; o loader nativo lê em blocos limitados.
- O compilador escreve `sourcesMs`, `fingerprintMs`, `parsingMs`, `referencesMs`, `emitMs` e `totalMs`.
- O worker isolado registra `frameworkSetupMs`, `managedCompileMs`, `workerTotalMs` e mantém `roslynProfile` após a execução dos defaults.
- Os hashes SHA-256 da fonte, SDK e assembly continuam exigidos; nenhuma validação foi desativada para acelerar o primeiro carregamento.

## Benchmark de compilação host (GitHub Actions, Linux)

| Classes C# | Roslyn frio naquele caso | Verificação dos defaults | Cache de assembly |
|---:|---:|---:|---:|
| 100 | 1.622 ms | 143 ms | 129 ms |
| 300 | 493 ms | 84 ms | 15 ms |
| 600 | 561 ms | 158 ms | 24 ms |
| 1.000 | 906 ms | 382 ms | 22 ms |

O primeiro caso inclui aquecimento do Roslyn no runner; resultados seguintes usam o mesmo processo e não equivalem à inicialização do MonoVM no Xiaomi. Estes são testes de funcionalidades simples `BunnyItem`, não uma representação completa do Split com IA, efeitos e dependências.

Verificadores: `tests/csharp_scale/Program.cs`, `tests/csharp_scale/large_parser.cpp` e workflow `check-csharp-large-mod-benchmarks.yml`. O teste verifica 1.200 tipos no parser nativo, compila/executa até 1.000 tipos reais, valida a recompilação evitada pelo cache e rejeita 4.097 fontes antes de alocar os gráficos Roslyn.

## Próxima validação Android

Uma instalação nova precisa verificar que os tempos `frameworkSetupMs`, `managedCompileMs`, `roslynProfile` e `CompileMs` convergem. Não habilitar `concurrentBuild`, compartilhar worker entre mods, antecipar execução gerenciada ao menu ou enfraquecer sandbox sem medir p90/p99 de FPS, RAM e estabilidade do MonoVM. O objetivo é reduzir o primeiro carregamento sem trocar 60 FPS por tempos menores de compilação.

O benchmark Linux comprova correção e escalabilidade de compilação, não confirma desempenho do Split no aparelho.
