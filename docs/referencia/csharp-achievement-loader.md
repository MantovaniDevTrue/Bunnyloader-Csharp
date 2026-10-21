# AchievementLoader C# — validação de fontes e runtime gerenciado

Referência exclusiva para Bunny: código original de `ModAchievement.js`, `AchievementLoader.js`, `ExampleBossKilled.js` e `ManyWormsKilled.js` do ExampleModBunny.

## Implementado

- Classes C# `BunnyAchievement`, `BunnyAchievementCondition`, `BunnyAchievementRegistry`, `BunnyAchievementRuntime` e `BunnyAchievementStore` no SDK distribuído `BunnySourceSdk` (.NET 8).
- Condições de conclusão por flag, inteiro, float, NPC morto, item coletado, item fabricado e tile destruído.
- Progresso estável por UID/classe/chave, combinação de condições, contagem numérica, Hidden, Category e callback OnCompleted.
- Persistência `BACH1` por mod em área interna do Bunny, com limite de 1 MiB, UTF-8 estrito, escrita em `.tmp`, `Flush(true)` e rename atômico. O progresso anterior é lido antes do vínculo nativo. Save corrompido é recusado sem ser sobrescrito; conquistas vanilla não são modificadas.
- Autosave limitado por alterações reais (até três segundos nos eventos seguintes), imediato na conclusão e flush assíncrono em ordem FIFO ao sair do mundo.
- Metadados `BACH_META_V1` enviados apenas quando o progresso muda: a camada nativa valida UID, campos, estados, índices, caminhos e atualização de conclusão/visibilidade.
- Endpoint `BunnyAchievementRuntime.Dispatch` no worker MonoVM: bind, eventos, save/load, unload, metadata, configuração de pasta e flush. A fila nativa tem até 256 comandos, **2 MiB de payloads acumulados**, resposta limitada e diagnóstico de saturação com logs reduzidos.
- Descoberta de `BunnyAchievement` no parser do Bunny e no verificador das assemblies compiladas, incluindo assemblies que contenham apenas conquistas.
- Metadados gerenciados exportados em `source-achievement-values.txt`, separados de `source-runtime-values.txt` para não interferir em registros de itens.

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
| Metadata | 8 | Retorna BACH_META_V1 |
| ConfigureStorage | 9 | Recebe pasta interna já criada e recupera BACH1 |
| Flush | 10 | Grava progresso atômico imediatamente |

Para eventos (1–4), `payload` deve ser `1` se for o jogador local ou `0` nos demais casos. Quando nada mudou, retorna `OK|...`. Se alguma condição progrediu, retorna `BACH_META_V1` para o lado nativo atualizar a UI sem polling. Erros retornam `ERROR|...`; eventos não são executados na thread GL.

## Exemplos e testes

- `samples/ExampleModCSharp/Examples/Achievements/ExampleBossKilled.cs`
- `samples/ExampleModCSharp/Examples/Achievements/ManyWormsKilled.cs`
- `tests/achievements_csharp/Program.cs` testa isolamento de callbacks, limites, restore, DLL real, metadata, bind, unload e eventos.
- `tests/achievements_csharp/source_parser_test.cpp` testa o parser Bunny nativo (classe única e mod misto).
- `tests/achievements_csharp/transport_test.cpp` valida fila FIFO, limites de memória e sobrecarga.
- `tests/achievements_csharp/metadata_test.cpp` valida protocolo nativo e rejeição de estados/identidades inválidos.

Comandos de CI que não geram APK:

```sh
g++ -std=c++20 -O2 -Iapp/src/main/cpp tests/achievements_csharp/source_parser_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o /tmp/csharp-achievements-parser
/tmp/csharp-achievements-parser
dotnet run -c Release --project tests/achievements_csharp/AchievementCoreTests.csproj
```

Validação de 21/10/2026: GitHub Actions [39640816557](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39640816557) — parser C++, fila C++, decoder de metadados e sintaxe nativa passaram; **61 verificações .NET** passaram. Isso **não** valida o comportamento dentro do Terraria Android.

## Pendências que impedem concluir ou gerar APK

1. **Implementado em fonte, pendente QA Android:** despacho por fila FIFO limitada de 256 comandos no worker MonoVM já existente, com limite de resposta de 1 MiB, eventos assíncronos e controles com timeout/resultado desconhecido explícito.
2. **Implementado em fonte, pendente QA Android:** resolução tipada das quatro assinaturas IL2CPP do AchievementsHelper (NotifyNPCKilledDirect, NotifyItemPickup, NotifyItemCraft e NotifyTileDestroyed). Os ganchos só ficam ativos no mundo e entregam dados do jogador local a cada mod registrado. Ainda falta conferir antiduplificação por sessão e rejeição de eventos atrasados em transições de mundo.
3. **Pendente:** registrar conquistas e conditions/trackers no `Terraria.Main.Achievements`, incluindo reload seguro e preservação do progresso vanilla.
4. **Pendente:** implementar textura de ícones no layout mobile, categorias, nome e descrição localizados, conquistas ocultas, menu e popup nativos.
5. **Implementado para armazenamento próprio C#:** `BACH1` atômico, restauração pré-evento, autosave e flush na saída; **pendente** integrar à identidade do perfil Terraria/nuvem, sem conflito com conquistas vanilla.
6. **Pendente:** registrar os metadados validados como objetos `Terraria.Achievements.Achievement` e condições reais, icon atlas 592×64, categorias, textos localizados, menu e popup; completar desativação de mods, deduplicação/geração de mundo, multiplayer, testes de carga e QA Android.

**ESTADO: WIP — SEM APK, SEM MERGE.** A branch permite avançar com CI estática e gerenciada, mas ainda não é loader jogável.
