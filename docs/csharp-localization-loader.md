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

## Comprovação em host

`dotnet run --project Split/Bunny-Loader/tests/localization_loader/LocalizationLoader.csproj --configuration Release`

A Action `CSharp Localization Core` compilou e executou os testes para o commit `e5f43d2d2185e7012ff6a7ccea999c3d6293bab9`: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39308494591

Os testes cobrem fallback, namespaces entre mods, referências e ciclos, offsets de argumentos, snapshots, registro global, entrada inválida, concorrência, arquivos reais do ExampleMod, recarga, Unicode/surrogates e rollback por JSON incorreto. Nenhuma dessas verificações é um teste de gameplay, de ABI ARM64 ou de FPS.

## Pendências obrigatórias para declarar completo

1. Ligar o importador ao ciclo real de carregamento automático C# no MonoVM e à persistência de runtime, tanto em cache quente quanto em compilação fria, incluindo descarte por mod/desativação.
2. Publicar os textos no `Terraria.Localization.LanguageManager` na thread adequada, preservar instâncias já existentes de `LocalizedText`, reprocessar ao alterar a cultura e invalidar caches de tooltips sem VM JS.
3. Resolver referências a traduções vanilla e cross-mod que ainda não estão no catálogo C#, variantes `$...`, expansão exata de templates e a precedência de `Register` em alterações de idioma.
4. Validar todos os consumidores: nomes/descrições de itens, NPCs, projéteis, buffs, mapa, comandos, configurações e interfaces; checar entrada tardia, troca de mundo, unload e isolamento por mod.
5. Criar testes de bridge C++ e de integração, compilar sem APK primeiro e **somente quando todo o lote exigido estiver completo** realizar a etapa Android autorizada pelo usuário. Sem afirmar 60 FPS sem teste do aparelho.

## Bloqueio de entrega

**Não gerar APK, não exportar `.bmod` de entrega, não marcar o loader como pronto nem mesclar antecipadamente com as PRs #98/#100.** Qualquer falha nas suítes abrangentes deve ser explicada e corrigida na branch proprietária antes de avançar o gate.
