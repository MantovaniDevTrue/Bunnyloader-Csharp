# AchievementLoader C# — validação de fontes e runtime gerenciado

Referência exclusiva para Bunny: código original de `ModAchievement.js`, `AchievementLoader.js`, `ExampleBossKilled.js` e `ManyWormsKilled.js` do ExampleModBunny.

## Implementado

- Classes C# `BunnyAchievement`, `BunnyAchievementCondition`, `BunnyAchievementRegistry` e `BunnyAchievementRuntime` no SDK distribuído `BunnySourceSdk` (.NET 8).
- Condições de conclusão por flag, inteiro, float, NPC morto, item coletado, item fabricado e tile destruído.
- Progresso estável por UID/classe/chave, combinação de condições, contagem numérica, Hidden, Category e callback OnCompleted.
- Persistência versionada `BACH1` com limites, validação de entrada, recuperação e preservação de conquistas de mods ausentes.
- Endpoint `BunnyAchievementRuntime.Dispatch` para o worker MonoVM, com operações Bind, NpcKilled, ItemPickup, ItemCraft, TileDestroyed, Save, Load, Unload e Metadata.
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

Para eventos (1–4), `payload` deve ser `1` se for o jogador local ou `0` nos demais casos. Valores inválidos produzem `ERROR|...` sem alterar o registro. O consumidor nativo precisa tratar erro explicitamente.

## Exemplos e testes

- `samples/ExampleModCSharp/Examples/Achievements/ExampleBossKilled.cs`
- `samples/ExampleModCSharp/Examples/Achievements/ManyWormsKilled.cs`
- `tests/achievements_csharp/Program.cs` testa isolamento de callbacks, limites, restore, DLL real, metadata, bind, unload e eventos.
- `tests/achievements_csharp/source_parser_test.cpp` testa parser Bunny nativo (classe única e mod misto).

Comandos de CI que não geram APK:

```sh
g++ -std=c++20 -O2 -Iapp/src/main/cpp tests/achievements_csharp/source_parser_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o /tmp/csharp-achievements-parser
/tmp/csharp-achievements-parser
dotnet run -c Release --project tests/achievements_csharp/AchievementCoreTests.csproj
```

Validação de 21/10/2026: GitHub Actions [39637092215](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39637092215) — parser C++ passou; fila C++ passou; C++ ManagedRuntime/NativeAchievementHooks/AutoSourceRuntime passaram na verificação de sintaxe; 42 verificações .NET passaram. Isto **não** valida o comportamento dentro do Terraria Android.

## Pendências que impedem concluir ou gerar APK

1. **Implementado em fonte, pendente QA Android:** despacho por fila FIFO limitada de 256 comandos no worker MonoVM já existente, com limite de resposta de 1 MiB, eventos assíncronos e controles com timeout/resultado desconhecido explícito.
2. **Implementado em fonte, pendente QA Android:** resolução tipada das quatro assinaturas IL2CPP do AchievementsHelper (NotifyNPCKilledDirect, NotifyItemPickup, NotifyItemCraft e NotifyTileDestroyed). Os ganchos só ficam ativos no mundo e entregam dados do jogador local a cada mod registrado. Ainda falta conferir antiduplificação por sessão e rejeição de eventos atrasados em transições de mundo.
3. **Pendente:** registrar conquistas e conditions/trackers no `Terraria.Main.Achievements`, incluindo reload seguro e preservação do progresso vanilla.
4. **Pendente:** implementar textura de ícones no layout mobile, categorias, nome e descrição localizados, conquistas ocultas, menu e popup nativos.
5. **Pendente:** integrar `BACH1` ao save real de conquistas do perfil, com gravação atômica e restauração antes dos eventos, evitando sobrescritas e duplicações.
6. **Pendente:** revisar ativação/desativação de mod, transições de mundo, multitarefa, multiplayer, desempenho, reconciliação do estado do worker e QA Android.

**ESTADO: WIP — SEM APK, SEM MERGE.** A branch permite avançar com CI estática e gerenciada, mas ainda não é loader jogável.
