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

## Lote de fidelidade: referências e recarga de idioma

- Novo `app/src/main/cpp/experiments/csharp/LocalizationResolve.h`: usa o mesmo contrato de referências do `LocalizationLoader.js`, com busca por escopo de dentro para fora, nomes completos `Mods.<id>.*`, dependências entre mods, referências de texto vanilla por `Language.Exists/GetTextValue`, detecção de ciclos e deslocamento de índices de formatação `@N` sem Regex.
- O publicador reúne todas as traduções escolhidas por cultura antes de processar as referências. Assim referências cruzadas não ficam dependentes da ordem de carregamento dos mods.
- O importador nativo converte `{$Mods.source.*}` em `{$Mods.<modid>.*}` depois de substituir a identidade temporária do compilador pela identidade real do mod.
- `LanguageManager.LoadFilesForCulture` ganhou um hook C++ que executa primeiro o método original do Terraria, reaplica os textos do C# e invalida `Terraria.UI.ItemTooltip` após êxito. A checagem de 30 atualizações permanece como fallback para carga tardia e não roda o resolvedor em cada frame.
- Testes host incluem `native_resolver_test.cpp` (escopos, vanilla, cross-mod, ciclos, argumentos, referências ausentes) e `native_wire_test.cpp` (rebasing da identidade temporária e UTF-8). **Não há evidência de validação do hook em dispositivo Android.**

**Interferência de PR paralela:** as Actions disparadas em `pull_request` usam uma árvore de merge com a base mais recente. Quando a PR #98 acrescenta novas propriedades do SDK e mais um campo no wire de Tile, verificações de referência/documentação e o teste rígido de comprimento de Tile podem falhar na árvore combinada, mesmo com os testes específicos da branch de localização passando. Não alterar fontes de Tile na #101; reconciliar as bases e regenerar documentação somente quando a integração dos lotes ocorrer.

## Compatibilidade restaurada e ciclo de vida

- A branch da PR #101 foi reconciliada com a baseline atual de Tile (PR #98), preservando a propriedade `BunnyTile.CacheDrawData`, a serialização de 21 campos, a implementação de `NativeTile` e as alterações de MonoVM. O SDK público JSON e Markdown foi atualizado sem eliminar a classe `BunnyLocalizationCatalog`.
- A checagem combinada de localização, SDK e auditoria de ExampleMod ficou verde nessa baseline; os erros antigos de 20/21 campos e documentação de Tile não foram suprimidos com testes permissivos.
- `NativeLocalizationSession.h` mantém catálogos por mod e geração, substitui versões sem reter entradas obsoletas e retira da tabela ativa os mods não carregados no novo início de sessão.
- A remoção guarda as chaves antigas num conjunto de *retired* e as neutraliza por `LanguageManager.LoadLanguageFromFileText` com seu próprio caminho como texto. Isso impede a exibição da tradução antiga sem invalidar abruptamente referências a `LocalizedText` guardadas pelo jogo. O código agora tenta remover as chaves de `_localizedTexts` após neutralizar o texto, mas isso ainda exige validação no jogo. Descarregamento integral ainda precisa de política nativa de remoção segura e validação em jogo.
- O reconciliador roda só no início do lote C# e reaplica o catálogo na thread do jogo. `native_session_test.cpp` cobre reinício, desativação, reativação, substituição e não alteração da revisão quando os mods ativos não mudam.
- Compatibilidade da PR #100 (equipamentos) permanece **separada**: não incorporamos classes experimentais dessa PR nem sobrescrevemos as alterações de Prefix.

Teste de regressão de localizações + ciclo de vida (CI, sem APK): https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39317201257

## Lote de encerramento de sessão, variantes e JSON estrito

- A remoção de um catálogo agora neutraliza os valores de `LocalizedText` via `LoadLanguageFromFileText` e procura `LanguageManager._localizedTexts` para remover fisicamente apenas suas antigas chaves `Mods.<id>.*`. O método nativo é validado por assinatura `bool Remove(string key)` antes de invocar; sem essa assinatura, não é considerado descarregamento confirmado. Os objetos já referenciados mantêm seu valor neutro e não são liberados diretamente.
- `NativeLocalizationRetirePolicy.h` verifica que as chaves descartadas não estão presentes em nenhum catálogo ativo e que pertencem ao namespace de um mod. Inclui regressão para chave vanilla, chave de outro mod, referências ativas e identificadores inválidos.
- A validação C# e nativa aceita a convenção `ItemName.Foo$Variant`, usada pelo loader original, sem liberar variantes vazias ou sufixos que mudem o namespace. Os testes cobrem serialização, decode e remoção dos mesmos IDs.
- O parser de `Localization/*.json` passou a rejeitar números inválidos mesmo em propriedades ignoradas, mantendo catálogos anteriores intactos quando a carga falha.
- A baseline atual da PR #98 foi integrada preservando os contratos de Tile, inclusive `CacheDrawData` e os novos métodos de desenho nativo. Os arquivos de referência gerados foram combinados em vez de reverter a base.

O método de descarregamento agora é *fail-safe*: se a API nativa de remoção do dicionário não estiver disponível, ele neutraliza os textos sem repetir invocações de erro a cada 30 atualizações. A primeira falha estrutural é registrada uma única vez; não elimina traduções de outros mods.

**Limite de certificação:** a API interna `_localizedTexts`, os métodos genéricos `Remove` e a preservação de referências são verificados estaticamente por contrato e política de host; ainda não existe teste real na libil2cpp do Terraria 1.4.5.8.6. Esse lote não declara descarregamento físico nem atualização de tooltips como validados em gameplay.

## Paridade adicional da API gerenciada

- `Register` tem precedência por cultura sobre entradas do JSON na mesma chave. Uma recarga de `RegisterCulture` ou `ReplaceModCultures` não perde essas substituições; `RemoveMod` limpa os registros pertencentes ao namespace desse mod. O comportamento é validado sob troca de idioma, snapshots e desativação.
- `BunnyLocalizationCatalog.Register(fullKey, cultures)` aceita um mapa de culturas como `ModLocalization.Register(key, { cultura: texto })` do Bunny JS: valida todas as entradas antes de publicar o snapshot, mantém outras culturas da chave e atualiza os valores por idioma sem publicar estado parcial.
- `BunnyLocalizationCatalog.Key(path, callerModId)` retorna `Mods.<id>.<path>` somente para uma chave já declarada pelo mod chamador, como o `ModLocalization.Key` do ExampleModBunny; chaves ausentes continuam inalteradas.
- `BunnyLocalizationCatalog.Texts(path, callerModId)` retorna uma cópia das traduções nas culturas realmente declaradas, com referências `{$...}` resolvidas em cada cultura sem alterar a cultura ativa.
- A suíte gerenciada verifica registro de várias culturas, colisão, fallback `en-US`, entrada inválida, atomicidade, resolução `Key`, expansão de `Texts` e identidade do idioma.

**Limitação importante:** esses métodos funcionam no catálogo C# gerenciado e estão expostos no SDK, mas alterações dinâmicas feitas dentro de uma sessão MonoVM ainda não são transmitidas ao publicador IL2CPP. O despacho do `OnLocalizationsLoaded` foi integrado em código, porém não foi testado no jogo ou em Android.

## Registro de traduções em classes C# e callback no MonoVM

- `BunnyLocalizationModule` foi adicionado ao SDK e pode ser implementado diretamente como `public sealed class MinhaLocalizacao : BunnyLocalizationModule`. O método obrigatório `RegisterTranslations(BunnyLocalizationCatalog catalog)` é chamado uma vez durante o verificador da montagem C#.
- O verificador usa a mesma descoberta por tipos de conteúdo do ExampleModCSharp, com limite de 32 módulos, ordem por nome completo e exigência de construtor público sem parâmetros. Ele junta JSON e traduções dos módulos em um único catálogo, preservando a precedência do `Register` e exportando as culturas ao protocolo nativo já existente.
- Neste lote, só se aceitam entradas sob `Mods.source.*`, posteriormente adaptadas ao identificador real do mod. Tentativas de alterar chaves vanilla/globais pelo módulo são rejeitadas antes de publicar. O wire suporta assemblies que contenham exclusivamente módulos de localização.
- Exemplo canônico: `samples/ExampleModCSharp/Playable/content/Common/Localization/ExampleLocalizationModule.cs`, com `en-US` e `pt-BR`. O teste `compiled-localization-module-publishes-wire-without-json` chama diretamente `CompiledAssemblyProbe.Verify` usando uma montagem C# gerenciada com um módulo, sem APK. A matriz oficial de exemplos recebeu o novo arquivo, sem promoção artificial de status de validação Android.
- O registro de textos ainda ocorre na compilação/verificação. A notificação `OnLocalizationsLoaded()` possui uma ponte separada para o worker MonoVM: o método não executa no verificador, não acessa ponteiros IL2CPP e recebe a cultura aplicada por `BunnyLocalizationRuntime.ActiveCultureName`. Nenhuma chamada ao callback no Terraria Android está certificada até o momento.

## Continuação de 19/10 — ciclo de vida do callback gerenciado

- `BunnyLocalizationModule.OnLocalizationsLoaded()` tem implementação virtual vazia por padrão. O módulo do ExampleModCSharp mostra uma implementação que lê `BunnyLocalizationRuntime.ActiveCultureName` somente dentro do callback.
- `BunnyLocalizationRuntime.Dispatch(modId, assemblyPath, culture)` descobre até 32 módulos concretos da montagem C#, armazena instâncias isoladas por mod e invoca os callbacks na sequência determinada pelos nomes completos. `Unload(modId)` descarta as instâncias ao desativar o mod.
- `NativeLocalizationRuntime` agenda as notificações **depois** de publicar `LanguageManager.LoadLanguageFromFileText`, tanto no hook `LoadFilesForCulture` quanto na checagem de fallback, enviando apenas strings para a ponte. Módulos sem entradas `LOCALE` ainda não possuem detecção específica para ativar o callback.
- `ManagedRuntime` processa as notificações na thread MonoVM fixa, com fila de até 64 eventos, priorizando o despacho síncrono existente de receitas. O worker não executa a cada frame por causa da localização; uma fila cheia recebe tentativa de reenvio no polling nativo posterior, sem republicar todas as traduções.
- `AutoSourceRuntime` liga as montagens C# depois da aplicação dos defaults e prepara o MonoVM uma vez quando há registros de localização. `retainActiveMods` desregistra callbacks de mods inativos.
- A suíte C# aumentou para **20 cenários**, com teste de execução, cultura ativa, descarregamento e recarga do módulo. O compilador e o catálogo ainda são testados separadamente.
- CI de host: as verificações de C# e C++ podem validar assinaturas, sintaxe, protocolos e contratos de fila, mas **não** ABI IL2CPP/ARM64, comportamento real das chamadas MonoVM, consumo de memória ou FPS.

**Ainda incompleto:** o resultado do callback não registra novas traduções dinamicamente no `LanguageManager`; callbacks em andamento durante um unload e o comportamento com vários módulos/idiomas exigem testes adicionais. Nenhum APK foi construído ou autorizado. A PR permanece draft.

## Pendências obrigatórias para declarar completo

1. **Validar no Terraria Android** a nova ponte C++ (incluindo ABI exata dos métodos IL2CPP, chamada e efeito real na thread do jogo) e a serialização de ida e volta de cache quente/frio. Os testes gerenciados anteriores não cobrem a integração nativa nova.
2. Garantir que `LoadLanguageFromFileText` preserva todas as instâncias `LocalizedText` existentes em 301720, invalida os tooltips e atualiza itens/NPCs/buffs que guardam caches próprios. Testar troca repetida pt-BR/en-US/fr-FR.
3. Confirmar em gameplay referências entre mods e vanilla, variantes `$...`, expansões recursivas, precedência de `Register`, callbacks `OnLocalizationsLoaded` e seu comportamento quando o worker estiver indisponível.
4. Tratar desativação, unload e recarga em tempo de jogo: limpar textos de mods desativados, preservar mods terceiros e não deixar referências antigas na UI.
5. Testar Android em aparelho, concorrência de carregamento, multiplayer e FPS; nenhum desses testes foi feito neste lote. **Proibido gerar APK ou `.bmod` antes de completar o escopo exigido**.

## Bloqueio de entrega

**Não gerar APK, não exportar `.bmod` de entrega, não marcar o loader como pronto nem mesclar antecipadamente com as PRs #98/#100.** Qualquer falha nas suítes abrangentes deve ser explicada e corrigida na branch proprietária antes de avançar o gate.
