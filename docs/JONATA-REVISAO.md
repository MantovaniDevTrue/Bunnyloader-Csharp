# Revisão do suporte C# — Jonata

Este documento acompanha a cópia do Bunny Loader com suporte C# preparada por Mantovani. O objetivo é facilitar uma revisão técnica e possível colaboração, sem pedir que o Bunny original seja substituído.

## Qual código estou enviando

- **Repositório:** [MantovaniDevTrue/Bunnyloader-Csharp](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp).
- **Branch de entrega:** `handoff/jonata-csharp-20261026`.
- **Snapshot de origem:** `qa/csharp-world-tree-gore-apk-20261024`, commit `6d96539f749b2ac8736c08bc38244cf88ec6cda0` (26/10/2026).
- **Bunny original usado no inventário histórico:** `a370a664aa3b808c87e4e4f570b27504d20df723` (15/10/2026).
- **Bunny original mais recente observado nesta preparação:** `c028c326ead08087f6a4f08048b256bc1a216954` (`main`, consultada em 26/10/2026).

A diferença entre os dois commits de Jonata **não foi reconciliada** nesta branch. Este é um pacote de fontes para revisão, não um patch seguro de aplicação automática sobre a `main` atual dele.

## O que já temos

O núcleo implementado integra compilação Roslyn, SDK para mods-fonte C#, MonoVM em worker Android, serviços JNI/Kotlin, comunicação com C++ e hooks IL2CPP. A execução C# não exige traduzir seus callbacks em JavaScript. O projeto mantém a parte JavaScript do Bunny original.

A infraestrutura MonoVM + Roslyn foi exercitada em várias builds e sessões Android durante o desenvolvimento, sem falha própria identificada nos testes relatados pelo desenvolvedor. Isso é distinto de certificar a integração de todo loader com o jogo.

Neste snapshot, os workflows a seguir concluíram com sucesso:
- [Bunny CSharp Unified Mobile — QA APK](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/40378683592).
- [CSharp localization rarity and angler lifecycle](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/40378683537).

São resultados de CI para esse commit, não prova de que todas as classes e efeitos dos loaders estejam completos no dispositivo.

## Onde começar a leitura

| Camada | Ponto de entrada |
| --- | --- |
| Descoberta/registro e cache de fontes | [CSharpModLoader.cpp](../app/src/main/cpp/experiments/csharp/CSharpModLoader.cpp) e [AutoSourceRuntime.cpp](../app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp) |
| Worker e runtime gerenciado | [ManagedRuntime.cpp](../app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp) e [BunnyManagedProbe](../experiments/csharp/BunnyManagedProbe/) |
| Roslyn e SDK | [BunnySourceCompiler](../experiments/csharp/BunnySourceCompiler/), [BunnySourceSdk](../experiments/csharp/BunnySourceSdk/) e [BunnyCsSdk](../experiments/csharp/BunnyCsSdk/) |
| Serviço Android | [ManagedProbeService.kt](../app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt) |
| Hooks, ABI e publicação no Terraria | [Host C++](../app/src/main/cpp/experiments/csharp/) e [contratos](csharp/LOADER_CONTRACTS.md) |
| Mods e exemplos C# | [ExampleModCSharp](../samples/ExampleModCSharp/) |

O catálogo de [cobertura do ExampleMod](csharp/EXAMPLEMOD_COVERAGE.md) é histórico e registra pendências; não deve ser lido como percentual atual de conclusão da base MonoVM/Roslyn. Os contratos de hooks e o port do ExampleMod são frentes separadas.

## O que ainda está em desenvolvimento

- Loaders e APIs específicos de conteúdo do Terraria; existem sistemas implementados e sistemas ainda incompletos.
- Contratos nativos IL2CPP de alguns loaders, especialmente quando precisam de arrays, campos ou callbacks dependentes da versão Android.
- Fidelity dos exemplos do ExampleModBunny, incluindo comportamento em mundo, desenho, interações e multiplayer.
- Revisão da independência de cada caminho C# em relação ao JavaScript legado, quando houver implementação visual compartilhada.

Exemplo de diferença: o traje `ExampleCostume` compila, mas um teste Android anterior registrou uma política de hooks nativos pendente. Isso exige trabalho no loader/integração, não reconstruir o compilador Roslyn.

## Onde a colaboração do Jonata ajudaria

1. Revisar integração no boot e JNI, sem alterar o fluxo JS já funcional.
2. Revisar os contratos IL2CPP compartilhados e os pontos que precisam coexistir com novos loaders.
3. Ajudar a conectar callbacks/classes do SDK ao Terraria Android usando os fontes reais do Bunny e do ExampleModBunny.
4. Avaliar uma integração gradual numa branch do Bunny oficial, após comparar com a `main` mais recente, mantendo os créditos e o histórico.

Não peço repetição da bateria geral de MonoVM/Roslyn como condição para revisar a fonte. Correções novas em loaders podem ter seus próprios testes específicos.

## Cuidados antes de importar

- **Não usar** o guia antigo `IMPORTAR-NO-GITHUB-DO-POTATO.md` como instrução atual: ele foi escrito para um snapshot anterior e menciona `v1.0` e o commit `a370a664`.
- **Não executar** `tools/importar_fonte_csharp_no_bunny.py --aplicar` sobre a `main` atual sem revisar o script e o commit-base esperado. O próprio roteiro anterior exigia um commit exato.
- Não sobrescrever a `main` do Bunny de Jonata, não usar `force push` e não importar dados proprietários/keystores.
- Fazer a comparação por pastas e por função; fontes do Bunny e extensões C# alteram alguns dos mesmos arquivos.
- Preservar o JS original, os arquivos de licença e a autoria: Bunny Loader por Jonata Oliveira, integração C# por Mantovani.

O trabalho diário continua na branch [QA C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/qa/csharp-world-tree-gore-apk-20261024). Esta branch de entrega é um ponto fixo de comparação; novas mudanças exigem atualização explícita da entrega.
