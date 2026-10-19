# LocalizationLoader C# — lote isolado, sem APK

Estado: **núcleo gerenciado funcional e testado; Loader C# ainda incompleto no Terraria Android**. PR #101 draft, separada da #98 (mundo) e da #100 (equipamentos).

Fontes de contrato: `Split/Bunny-Loader/app/src/main/cpp/script/js/mod/ModLocalization.js` e `Loaders/LocalizationLoader.js`; testes de fidelidade usam os arquivos originais `samples/ExampleMod/content/Localization/en-US.json` e `pt-BR.json`, copiados sem mudanças para `samples/ExampleModCSharp/Playable/content/Localization/`. O processamento gerenciado usa somente C#; não chama QuickJS, ExMod ou API tModLoader.

## Implementado

- `BunnyLocalizationCatalog` aceita cultura e registros por mod, chave pública `Mods.<id>.<chave>`, consulta relativa ao dono ou absoluta, detecção de ambiguidade, fallback da cultura ativa para `en-US` e depois a primeira disponível, registro de chave global e remoção por mod.
- Referências `{$chave}` relativas ou absolutas, ciclos, offsets `@N` para índices formatados. Resolução com varredura manual, sem Regex ou assemblies de Regex adicionais no MonoVM. Strings e mapas não são compartilhados em estado mutável com leitores.
- `ReplaceModCultures` valida todas as culturas e chaves antes de publicar um snapshot; uma falha não altera o catálogo anterior e uma recarga remove chaves obsoletas.
- `LoadFromContent(modId, contentDirectory)` lê `Localization/*.json` como UTF-8 estrito, com JSON aninhado/escape Unicode, bom comportamento para campos não textuais como no flatten da fonte JS e limite de profundidade, entradas, tamanho e culturas. Rejeita `Localization` e arquivos que são symlinks; ordena culturas antes de registrar e só comita após a leitura integral.
- JSONs originais da referência incluem **308 caminhos de tradução por idioma** (a quantidade corresponde à fonte do repositório, não necessariamente a cada `Item/NPC/Tile` jogável do port).
- Métodos públicos foram incluídos em `BunnySourceSdk.csproj` e atualizados em `docs/generated/sdk_public_reference.json` e `.md`.

## Ponte integrada neste lote

- `CompiledAssemblyProbe.Verify(assembly, diagnostics, modRoot)` lê os JSONs através do `BunnyLocalizationCatalog` próprio do SDK, mantendo a sobrecarga de duas entradas para os testes antigos. Exporta `LOCALE\tculture\thex(key)\thex(text)` para `source-runtime-values.txt`, que já é persistido junto às políticas no cache nativo.
- `ManagedRuntime.cpp` passa `modRoot` ao verificador; `SourceLoader.cpp` só aceita entradas com chave `Mods.source.*`, UTF-8 válido, cultura e identificador válidos, sem repetição e dentro do limite, e publica o lote após os outros defaults válidos.
- `AutoSourceRuntime.cpp` inclui os JSONs sob `content/Localization/` na impressão digital do cache C#. A alteração de uma tradução invalida os dados antigos do loader sem alterar a identidade do assembly Roslyn nem exigir recompilar todas as classes C# por cada tradução.
- `NativeLocalizationRuntime.cpp` usa apenas C++/IL2CPP para construir o formato de `LanguageManager.LoadLanguageFromFileText`, registrar as entradas a partir da thread do jogo, detectar idioma ativo e reprocessar mudanças com consulta amortizada a cada 30 updates (não há execução JS por tradução/frame).
- A ordem de fallback do publicador nativo é cultura exata → `en-US` → primeira cultura disponível. O código usa os contratos de `LanguageManager`, `GameCulture` e `Language` presentes nas fontes originais. Nenhum binário Android foi construído neste lote.

**A ponte existe em código, mas ainda não foi validada no Android.** Não confundir integração de fontes com uma afirmação de funcionamento real. O caminho de troca de idioma usa detecção periódica, não é equivalente ao hook original de recarga imediatamente após `LoadFilesForCulture`.

## Comprovação em host

`dotnet run --project Split/Bunny-Loader/tests/localization_loader/LocalizationLoader.csproj --configuration Release`

A Action `CSharp Localization Core` compilou e executou os testes para o commit `e5f43d2d2185e7012ff6a7ccea999c3d6293bab9`: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39308494591

Os testes cobrem fallback, namespaces entre mods, referências e ciclos, offsets de argumentos, snapshots, registro global, entrada inválida, concorrência, arquivos reais do ExampleMod, recarga, Unicode/surrogates e rollback por JSON incorreto. Nenhuma dessas verificações é um teste de gameplay, de ABI ARM64 ou de FPS.

## Verificação da ponte no host — 19/10/2026

A suíte CI sem APK comprovou que o SDK, o verificador Roslyn e o C++ da integração **compilam no host**, e que o decoder nativo rejeita culturas inválidas, UTF-8 malformado, chaves de outros mods e wire fora dos limites. A mesma execução confirmou o formato da referência pública e executou os 12 cenários gerenciados.

Execução aprovada no GitHub: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39312669920 (commit `542a022722999c76c0aaca5038f86969dcb949b4`).

Essa execução usa `dotnet build`, `dotnet run` e `g++ -fsyntax-only` / teste nativo de política. **Não compila bibliotecas Android, não invoca Gradle, não gera APK, não valida ABI do jogo e não mede FPS.** Código fonte nativo verificado sintaticamente não substitui o teste integrado do `LanguageManager`.

## Pendências obrigatórias para declarar completo

1. **Compilar e testar** a nova ponte C++ (incluindo ABI exata dos métodos IL2CPP, chamada e efeito real na thread do jogo) e a serialização de ida e volta de cache quente/frio. Os testes gerenciados anteriores não cobrem a integração nativa nova.
2. Garantir que `LoadLanguageFromFileText` preserva todas as instâncias `LocalizedText` existentes em 301720, invalida os tooltips e atualiza itens/NPCs/buffs que guardam caches próprios. Testar troca repetida pt-BR/en-US/fr-FR.
3. Resolver referências de texto cruzando mods e chaves vanilla antes da publicação, variantes `$...`, expansões recursivas, precedência de `Register` e os últimos detalhes do ciclo de vida `OnLocalizationsLoaded`.
4. Tratar desativação, unload e recarga em tempo de jogo: limpar textos de mods desativados, preservar mods terceiros e não deixar referências antigas na UI.
5. Testar Android em aparelho, concorrência de carregamento, multiplayer e FPS; nenhum desses testes foi feito neste lote. **Proibido gerar APK ou `.bmod` antes de completar o escopo exigido**.

## Bloqueio de entrega

**Não gerar APK, não exportar `.bmod` de entrega, não marcar o loader como pronto nem mesclar antecipadamente com as PRs #98/#100.** Qualquer falha nas suítes abrangentes deve ser explicada e corrigida na branch proprietária antes de avançar o gate.
