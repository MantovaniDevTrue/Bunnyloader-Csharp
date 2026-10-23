# AchievementLoader C# — PR complementar à #141

Base do comportamento: ExampleModBunny 1.9.0, com os fontes \`ModAchievement.js\`, \`AchievementLoader.js\`, \`ExampleBossKilled.js\` e \`ManyWormsKilled.js\`.

## Implementação atual

- API C# pura \`BunnyAchievement\` e \`BunnyAchievementCondition\`, condições de flag, inteiros, float, morte de NPC, coleta de item, fabricação e destruição de tile.
- Categoria, estado Hidden, callback OnCompleted, progresso, snapshots e restauração com verificação por UID, nome e tipo.
- Registry transacional em cada registro de assembly, revogação seletiva por UID e revisão protegida por SHA-256 do identificador físico com sufixo de geração.
- Roteamento \`BunnyRecipeRuntime.Dispatch\` para \`__achievements__\`: tipos 0 (registrar), 1 (evento), 2 (snapshot), 3 (restaurar) e 4 (retirar). O limite de 12.000 caracteres só é aplicado a esse endpoint; receitas permanecem em 512.
- Integração nativa no \`AutoSourceRuntime\` para vincular o UID/assembly à fila nativa e no \`noteAutoGameUpdate\` para consumir no máximo 2 entregas por quadro. Nenhuma varredura de entidades por quadro.
- Hooks nativos **opcionais**, após registro de pelo menos uma conquista, nos contratos originais de \`AchievementsHelper\`: NotifyNPCKilledDirect, NotifyItemPickup, NotifyItemCraft e NotifyTileDestroyed. O hook executa o original antes de registrar o evento, confere assinaturas e falha de forma conservadora.
- Fila limitada a 512 entregas, isolamento por UID/revisão, retry limitado e descarte de eventos obsoletos após substituição ou saída do mundo.
- Fixture autônomo \`AchievementsDemo\` com \`ManyWormsKilled\` fiel ao JS e PNG original de 4303 bytes, sem mexer na contagem do ExampleMod principal.

## Verificações executadas

- Teste C# de registry e do dispatcher real do SDK: dois mods, recarga de DLL no mesmo caminho, eventos locais/remotos, snapshot, recuperação e descarregamento sem interferir no outro UID.
- Compilação do demo original em .NET 8.
- Teste C++ de mailbox por UID e revisão.
- Teste de sintaxe do arquivo de hooks C++ com flags Android \`-fno-exceptions -fno-rtti\` e headers de produção.
- Referência pública do SDK regenerada automaticamente e validada por \`--check\`.
- Workflows adjacentes Localization, Rarity, AnglerQuest, Command, Invasion e outros mantidos. Testes de host não provam a execução no Android.

## Bloqueios antes de declarar o loader concluído

1. Registrar as condições C# efetivamente em \`Terraria.Main.Achievements\`, como faz o ExampleModBunny original. A progressão atual está na camada C#; o manager vanilla ainda não recebe essas conquistas.
2. Integrar UI do menu, textos localizados, popup e sprites 592×64, incluindo \`Hidden\` ("???"). O PNG original já está preservado no fixture.
3. Persistir o progresso no arquivo real de conquistas e reidratá-lo na inicialização/recarga. Snapshot no worker não equivale a save permanente.
4. Medir e validar hooks, assinatura nativa, comportamento local/multiplayer e gameplay no Terraria Android 301720. A compilação de sintaxe em host não substitui teste arm64 no aparelho.
5. Concluir o NPC ExampleBoss e seu registro de ID antes de portar \`ExampleBossKilled\` para o pacote principal, sem inventar ID.

**Estado: WIP, sem APK nem .bmod.** A PR complementar não modifica os três loaders Localization, Rarity ou AnglerQuest. As alterações compartilhadas se limitam ao SDK, roteamento de callbacks e pontos de binding/pump existentes.
