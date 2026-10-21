# AchievementLoader C# — validação de fontes e runtime gerenciado

Referência exclusiva para Bunny: código original de `ModAchievement.js`, `AchievementLoader.js`, `ExampleBossKilled.js` e `ManyWormsKilled.js` do ExampleModBunny.

## Implementado

- Classes C# `BunnyAchievement`, `BunnyAchievementCondition`, `BunnyAchievementRegistry`, `BunnyAchievementRuntime` e `BunnyAchievementStore` no SDK distribuído `BunnySourceSdk` (.NET 8).
- Condições de conclusão por flag, inteiro, float, NPC morto, item coletado, item fabricado e tile destruído.
- Progresso estável por UID/classe/chave, combinação de condições, contagem numérica, Hidden, Category e callback OnCompleted.
- Persistência `BACH1` por mod em área interna do Bunny, com limite de 1 MiB, UTF-8 estrito, escrita em `.tmp`, `Flush(true)` e rename atômico. O progresso anterior é lido antes do vínculo nativo. Save corrompido é recusado sem ser sobrescrito; conquistas vanilla não são modificadas.
- Autosave limitado por alterações reais (até três segundos nos eventos seguintes), imediato na conclusão e flush assíncrono em ordem FIFO ao sair do mundo.
- Metadados `BACH_META_V2` enviados somente quando uma condição muda: linhas `A` registram a conquista; linhas `C` preservam chave, tipo, limite e valor individual. O parser continua aceitando V1 para compatibilidade, sem mudar o `BACH1` persistido.
- Endpoint `BunnyAchievementRuntime.Dispatch` no worker MonoVM: bind, eventos, save/load, unload, metadata, configuração de pasta e flush. A fila nativa tem até 256 comandos, **2 MiB de payloads acumulados**, resposta limitada e diagnóstico de saturação com logs reduzidos.
- Descoberta de `BunnyAchievement` no parser do Bunny e no verificador das assemblies compiladas, incluindo assemblies que contenham apenas conquistas.
- Metadados gerenciados exportados em `source-achievement-values.txt`, separados de `source-runtime-values.txt` para não interferir em registros de itens.
- Registro nativo `NativeAchievementRegistry`: instancia `Terraria.Achievements.Achievement`, uma condição nativa por condição C# (`CustomIntCondition` ou `CustomFloatCondition`), `LocalizedText`, tracker apropriado, índice e categoria no `Main.Achievements` pela API IL2CPP com validação dos overloads antes da criação.
- Integração visual `NativeAchievementVisualHooks`: usa `ContentAssets::loadTextureAsset`, a mesma API nativa de imagens do Bunny, e exige os atlas 592 × 64 de Terraria Mobile; controla o `GUIAchievementsMenu.AchievementDraw`, a atualização do cache ao abrir e o `AchievementUnlockedPopup`. As trocas de nomes ocultos usam `LocalizedText` enraizado pelo GC.
- Callback entre worker MonoVM e parte nativa registrado por função ponte opcional; `libbunnymonoprobe` não depende diretamente de hooks de conteúdo IL2CPP. Registro/visualização só no thread do jogo.

## API do bridge gerenciado

`Dispatch(string modId, string assemblyPath, int operation, int value, int count, string payload)` recebe apenas tipos e quantidades, nunca objetos ou ponteiros do Terraria.

| Operação | Número | Finalidade |
|---|---:|---|
| Bind | 0 | Carrega assembly uma vez e registra classes |
| NpcKilled | 1 | Informa NPC morto |
| ItemPickup | 2 | Informa coleta e quantidade |
| ItemCraft | 3 | Informa fabricação e quantidade |
| TileDestroyed | 4 | Informa tile destruído |
| Save | 5 | Exporta BACH1 |
| Load | 6 | Importa BACH1 |
| Unload | 7 | Devolve snapshot e libera registro |
| Metadata | 8 | Retorna BACH_META_V2 com condições individuais |
| ConfigureStorage | 9 | Recebe pasta interna já criada e recupera BACH1 |
| Flush | 10 | Grava progresso atômico imediatamente |

Para eventos (1–4), `payload` deve ser `1` se for o jogador local ou `0` nos demais casos. Quando nada mudou, retorna `OK|...`. Se alguma condição progrediu, retorna `BACH_META_V2` para o lado nativo atualizar a UI sem polling. Erros retornam `ERROR|...`; eventos não são executados na thread GL.

## Exemplos e testes

- `samples/ExampleModCSharp/Examples/Achievements/ExampleBossKilled.cs`
- `samples/ExampleModCSharp/Examples/Achievements/ManyWormsKilled.cs`
- `samples/ExampleModCSharp/Playable/content/Content/Achievements/ExampleBossKilled.cs` usa o NPC vanilla King Slime (50).
- `samples/ExampleModCSharp/Playable/content/Content/Achievements/ManyWormsKilled.cs` usa Giant Worm Head (10), com o contador original. Ambos possuem os PNGs 592 × 64 reaproveitados do ExampleModBunny, sem recriar imagens.
- `tests/achievements_csharp/Program.cs` testa isolamento de callbacks, limites, restore, DLL real, metadata, bind, unload e eventos.
- `tests/achievements_csharp/source_parser_test.cpp` testa o parser Bunny nativo (classe única e mod misto).
- `tests/achievements_csharp/transport_test.cpp` valida fila FIFO, limites de memória e sobrecarga.
- `tests/achievements_csharp/metadata_test.cpp` valida V1/V2, múltiplas condições, tipos, limites e rejeição de metadados malformados.
- `tests/achievements_csharp/native_progress_test.cpp` valida a conversão do progresso C# para o tracker inteiro utilizado na visualização; o tracker permanece um espelho, não substitui as condições C#.

Comandos de CI que não geram APK:

```sh
g++ -std=c++20 -O2 -Iapp/src/main/cpp tests/achievements_csharp/source_parser_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o /tmp/csharp-achievements-parser
/tmp/csharp-achievements-parser
dotnet run -c Release --project tests/achievements_csharp/AchievementCoreTests.csproj
```

Validação de 21/10/2026: GitHub Actions [39652266785](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39652266785) — parser, fila, metadados V1/V2 e progresso C++ passaram; verificação de sintaxe do registro nativo, GUI e MonoVM passou; **69 verificações .NET** passaram. Isso **não** valida o comportamento dentro do Terraria Android.

## Pendências que impedem concluir ou gerar APK

1. **Implementado em fonte, pendente QA Android:** despacho por fila FIFO limitada de 256 comandos no worker MonoVM já existente, com limite de resposta de 1 MiB, eventos assíncronos e controles com timeout/resultado desconhecido explícito.
2. **Implementado em fonte, pendente QA Android:** resolução tipada das quatro assinaturas IL2CPP do AchievementsHelper (NotifyNPCKilledDirect, NotifyItemPickup, NotifyItemCraft e NotifyTileDestroyed). Os ganchos só ficam ativos no mundo e entregam dados do jogador local a cada mod registrado. Ainda falta conferir antiduplificação por sessão e rejeição de eventos atrasados em transições de mundo.
3. **Implementado em fonte, pendente QA Android:** criação de `Achievement` + condições nativas individuais, limites e valores próprios para inteiro/float, `Register`, `RegisterIconIndex`, `RegisterAchievementCategory` e tracker adequado para conquistas de condição única ou múltiplas no `Main.Achievements`. Condições de eventos usam espelhos numéricos para evitar contagem duplicada. **Falta** verificar o comportamento Android de `CustomIntCondition`/`CustomFloatCondition`, tracking, callbacks e preservação das conquistas vanilla.
4. **Implementado em fonte, pendente QA Android:** atlas do Bunny 592 × 64 e `Asset<Texture2D>`, ícones de menu/popup, categorias, nomes e descrições, placeholders `???` ocultos, renovação do menu. **Falta** testar assinaturas/ABI do draw/popup no aparelho e sincronizar traduções na troca de idioma.
5. **Implementado para armazenamento próprio C#:** `BACH1` atômico, restauração pré-evento, autosave e flush na saída; **pendente** integrar à identidade do perfil Terraria/nuvem, sem conflito com conquistas vanilla.
6. **Pendente:** lifecycle completo na desativação/recarregamento dos mods, deduplicação e geração de mundo, isolamento do save por personagem (`Main.ActivePlayerFileData` + `_path` do Bunny já confirmados na fonte, ainda não ligados ao AchievementLoader), compatibilidade com nuvem, sincronização com `LanguageManager` na troca de idioma, qualidade de imagens/ABI ARM64, verificações de progresso e popups no menu e multiplayer, testes de carga e QA Android.

**ESTADO: WIP — SEM APK, SEM MERGE.** A branch permite avançar com CI estática e gerenciada, mas ainda não é loader jogável.
