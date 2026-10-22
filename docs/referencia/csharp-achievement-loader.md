# AchievementLoader C# — validação de fontes e runtime gerenciado

Referência exclusiva para Bunny: código original de `ModAchievement.js`, `AchievementLoader.js`, `ExampleBossKilled.js` e `ManyWormsKilled.js` do ExampleModBunny.

## Implementado

- Classes C# `BunnyAchievement`, `BunnyAchievementCondition`, `BunnyAchievementRegistry`, `BunnyAchievementRuntime` e `BunnyAchievementStore` no SDK distribuído `BunnySourceSdk` (.NET 8).
- Condições de conclusão por flag, inteiro, float, NPC morto, item coletado, item fabricado e tile destruído.
- Progresso estável por UID/classe/chave, combinação de condições, contagem numérica, Hidden, Category e callback OnCompleted.
- Persistência `BACH1` por personagem/mod em `csharp-achievements/profiles/<sha256 do caminho + local/nuvem>/<moduid>.bach`, limite de 1 MiB, UTF-8 estrito, escrita em `.tmp` e substituição atômica. Identidade calculada com `Main.get_ActivePlayerFileData` + `FileData._path` / `_isCloudSave` na thread do jogo, usando o SHA-256 portátil do Bunny. Saves inválidos não são sobrescritos. O save vanilla não é modificado.
- Autosave de progresso alterado, flush FIFO no fim do mundo e heartbeat leve de cinco segundos. Requisições rejeitadas por saturação são reencaminhadas. Cada evento de conquista leva uma geração da sessão: ao sair ou trocar de personagem, a fila do MonoVM invalida eventos pendentes da sessão anterior; nenhum evento de um perfil antigo deve ser executado depois de ativar o próximo.
- Troca de personagem sem reiniciar implementada em fonte e pendente de QA Android: operação `SwitchProfile = 11` restaura o BACH1 de outro personagem com rollback do gerenciador C# em caso de save corrompido. O worker alterna as pastas por UID mantendo a origem do personagem antigo; as conquistas nativas só são liberadas após substituir os objetos do mod no `AchievementManager` usando um dicionário privado de tipo e identidade validados. Caso a ABI privada não corresponda ao Terraria mobile, a operação falha fechada e os eventos permanecem desabilitados.
- Metadados `BACH_META_V2` enviados somente quando uma condição muda: linhas `A` registram a conquista; linhas `C` preservam chave, tipo, limite e valor individual. O nativo atualiza cada condição, valida todos os campos antes da publicação e recusa o lote inteiro se uma identidade ou condição estiver inconsistente. O parser continua aceitando V1.
- Endpoint `BunnyAchievementRuntime.Dispatch` no worker MonoVM: bind, eventos, save/load, unload, metadata, configuração de pasta e flush. A fila nativa tem até 256 comandos, **2 MiB de payloads acumulados**, resposta limitada e diagnóstico de saturação com logs reduzidos.
- Descoberta de `BunnyAchievement` no parser do Bunny e no verificador das assemblies compiladas, incluindo assemblies que contenham apenas conquistas.
- Metadados gerenciados exportados em `source-achievement-values.txt`, separados de `source-runtime-values.txt` para não interferir em registros de itens.
- Registro `NativeAchievementRegistry` de objetos `Achievement`, condições nativas individualizadas `CustomFlagCondition`, `CustomIntCondition` e `CustomFloatCondition`, `LocalizedText`, índices, categorias e trackers com verificação das assinaturas IL2CPP.
- Nova API nativa `NativeAchievementManagerAdapter`: confirma `AchievementManager._achievements` ser um `Dictionary<string, Achievement>`, consulta `TryGetValue`, compara o ponteiro exato e remove apenas conquistas pertencentes ao mod. `replaceNativeAchievementObjects` prepara objetos novos antes de tocar o manager, tenta rollback caso alguma etapa falhe, confirma que `Main.Achievements` não foi substituído e conserva as raízes GC em rollbacks incertos. As referências antigas são liberadas apenas quando o menu já foi reconstruído. A API é **condicional à ABI real**, não foi validada ainda no Android.
- Descarregamento por UID (API de baixo nível): `unregisterNativeAchievementMod(modUid)` valida todas as conquistas do mod antes de alterar o gerenciador, usa propriedade exata e tenta rollback caso a remoção falhe. `unregisterNativeAchievementBinding` só retira o snapshot após o jogo pausar o despacho de eventos e a gravação final entrar na FIFO; se a fila estiver cheia, mantém a ligação para nova tentativa. No descarregamento C#, `Flush()` evita escrita redundante quando nada mudou. O último mod removido também invalida o cache do menu. **Ainda falta** um coordenador completo para executar detach nativo, unload MonoVM e reativação de outros mods; essas APIs não devem ser invocadas isoladamente durante gameplay.
- Fila MonoVM: a pausa de sessão invalida a geração anterior e também descarta imediatamente eventos de gameplay ainda enfileirados, preservando controles, save e unload na ordem FIFO. O teste C++ comprova que uma saída de mundo não elimina solicitações pendentes de gravação.
- Integração visual `NativeAchievementVisualHooks`: usa `ContentAssets::loadTextureAsset`, a API nativa de imagens do Bunny, e exige atlas 592 × 64. Controla `GUIAchievementsMenu.AchievementDraw`, cache do menu e `AchievementUnlockedPopup`, com textos ocultos `???` protegidos pelo GC.
- Idiomas sem JavaScript: `NativeAchievementLocaleJson` lê `content/Localization/<cultura>.json` do próprio mod, usa `Achievements.<Classe>.FriendlyName` ou `Name` e `Description`, com fallback `en-US` e finalmente os textos do C#. A cultura é lida pela API tipada `Terraria.Localization.Language.ActiveCulture.Name` no lado nativo, com verificação periódica de baixo custo; apenas mudanças de cultura ou conteúdo aplicam `LocalizedText.SetValue`.
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
- `tests/achievements_csharp/native_progress_test.cpp` valida a conversão determinística para barras de progresso.
- `tests/achievements_csharp/profile_test.cpp` verifica SHA-256 estável, isolamento entre personagens e distinção local/nuvem. A suíte .NET valida que o mesmo UID de mod não compartilha progresso entre dois diretórios de perfil.
- `tests/achievements_csharp/metadata_sync_test.cpp` valida atualizações atômicas individuais de condições e bloqueio de alterações estruturais/retrocessos pós-conclusão.
- `tests/achievements_csharp/locale_test.cpp` valida os textos do Bunny em inglês e português, Unicode e entradas inválidas.
- `tests/achievements_csharp/session_test.cpp` valida o epoch de mundos contra eventos atrasados. A suíte .NET testa troca entre dois perfis, flush de progresso sujo, restauração de flag e rollback quando o save novo está corrompido.
- `tests/achievements_csharp/ownership_test.cpp` verifica UID e prefixo exatos (`split` não inclui `splitplus`), sem correspondência com conquistas de outros mods ou vanilla.

Comandos de CI que não geram APK:

```sh
g++ -std=c++20 -O2 -Iapp/src/main/cpp tests/achievements_csharp/source_parser_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o /tmp/csharp-achievements-parser
/tmp/csharp-achievements-parser
dotnet run -c Release --project tests/achievements_csharp/AchievementCoreTests.csproj
```

Validação de 22/10/2026: GitHub Actions [39791211205](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39791211205) — parser, FIFO, metadados V1/V2, epoch de sessão, isolamento SHA-256, localização e sintaxe do novo adaptador IL2CPP passaram; **85 verificações .NET** passaram. A validação do funcionamento do `AchievementManager` no dispositivo Android ainda está pendente.

## Pendências que impedem concluir ou gerar APK

1. **Implementado em fonte, pendente QA Android:** despacho por fila FIFO limitada de 256 comandos no worker MonoVM já existente, com limite de resposta de 1 MiB, eventos assíncronos e controles com timeout/resultado desconhecido explícito.
2. **Implementado em fonte, pendente QA Android:** resolução tipada das quatro assinaturas IL2CPP do AchievementsHelper (NotifyNPCKilledDirect, NotifyItemPickup, NotifyItemCraft e NotifyTileDestroyed). Os ganchos só ficam ativos no mundo e entregam dados do jogador local a cada mod registrado. Ainda falta conferir antiduplificação por sessão e rejeição de eventos atrasados em transições de mundo.
3. **Implementado em fonte, pendente QA Android:** criação de `Achievement` + condições nativas individuais, flag pela `CustomFlagCondition.Create` e `AchievementCondition.Complete` (assinaturas verificadas em runtime), limites e valores próprios para inteiro/float, `Register`, `RegisterIconIndex`, `RegisterAchievementCategory` e tracker adequado para conquistas de condição única ou múltiplas no `Main.Achievements`. Condições de eventos usam espelhos numéricos para evitar contagem duplicada. **Falta** verificar o comportamento Android de `CustomIntCondition`/`CustomFloatCondition`, tracking, callbacks e preservação das conquistas vanilla.
4. **Implementado em fonte, pendente QA Android:** atlas 592 × 64, `Asset<Texture2D>`, ícones no menu/popup, categorias, nomes/descrições, `???`, refresh de menu e traduções em trocas de idioma. **Falta** validar `GameCulture`/IL2CPP getters, `LocalizedText.SetValue` e ABI ARM64 no aparelho.
5. **Implementado em fonte, pendente QA Android:** `BACH1` atômico por personagem/mod, distinção de origem local/nuvem, restauração pré-evento, reenvio de flush e `SwitchProfile` transacional. O mecanismo nativo para trocar personagens tenta retirar objetos próprios dos mods via API IL2CPP refletida e rollback, bloqueando a sessão se qualquer contrato divergir. **Pendente:** confirmar o campo privado e assinatura de remoção no aparelho, sincronização real com nuvem e migração do save global legado.
6. **Pendente:** validar no ARM64 os campos privados e os métodos do `AchievementManager`, coordenar o ciclo automático de desativação e recarregamento de mods (persistência, remoção nativa e unload do MonoVM), garantir que popups antigos não mantenham texturas já liberadas, verificar multiplayer/nuvem e realizar QA de carga no dispositivo.

**ESTADO: WIP — SEM APK, SEM MERGE.** A branch permite avançar com CI estática e gerenciada, mas ainda não é loader jogável.
