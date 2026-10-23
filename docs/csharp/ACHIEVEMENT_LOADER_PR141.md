# AchievementLoader C# — PR #142 complementar à #141

Fonte de referência: **ExampleModBunny 1.9.0**, especialmente ModAchievement.js, AchievementLoader.js, ExampleBossKilled.js e ManyWormsKilled.js. Implementação do mod em C# e host em C++ nativo, sem executar JavaScript no gameplay de mods C#.

## Implementado e testado no host

- \`BunnyAchievement\` e condições de flag, inteiros, float, morte de NPC, coleta, fabricação e destruição de tile.
- Categoria, ocultação, progresso, conclusão, callback \`OnCompleted\`, snapshot e restauração em memória.
- Registro independente por UID e revisão de assembly, com descarregamento seletivo e SHA-256 da identidade de revisão.
- \`BunnyAchievementWorker\` no MonoVM através do canal de callbacks já existente, sem novo worker. Comandos: 0 registro, 1 evento, 2 snapshot, 3 restauração, 4 unload, 5 descritores e 6 estado dinâmico para o host nativo.
- Exportação \`ACHMETA1\` com nome, friendly name, descrição, textura, índice, categoria, estado oculto/concluído e condições. Parser C++ de limite 12.000 bytes, identidade por UID, limites de condições e validação antes de ativar um mod.
- Estado dinâmico \`ACHSTATE1\` no MonoVM: condições, progresso, conclusão e \`Hidden\`, enviado apenas em caso de alteração. O parser nativo valida UID, identidade das condições, limites e consistência antes de atualizar o snapshot.
- \`getNativeAchievementState\` oferece consulta nativa por UID/nome sem varreduras do mundo, preparando a UI e a persistência.
- Contratos \`Terraria.Main.Achievements\` verificados em IL2CPP, sem mutações: construtores, textos, métodos de registro/categoria, condições, tracker, \`Load\` e possibilidade de \`Unregister\`. Sem reversibilidade confirmada, o registro real permanece bloqueado para proteger conquistas vanilla e outros mods.
- Detecção **de mods contendo somente conquistas C#**, agora suportados pelo parser, SourceLoader, bootstrap MonoVM e verificador Roslyn. Limite de 128 conquistas por assembly.
- Hooks \`AchievementsHelper\` de mortes de NPCs, coleta, crafting e tiles, preservando a chamada original, com inspeção de assinatura e instalação seletiva.
- Mailbox limitado a 512 eventos, máximo 2 eventos por quadro, até 2 retries, isolamento por UID/revisão e descarte de eventos obsoletos.
- Guarda atômica de mundo ativo: não contabilizar mortes, crafting ou coletas enquanto o mundo não está pronto. Sem mutex por quadro quando o menu está ativo.
- \`AchievementsDemo\` isolado do pacote de 100 fontes da PR #141: \`ManyWormsKilled.cs\`, ID 10 (GiantWormHead), contador de 5 e textura original PNG, sem redimensionamento.

## Testes automatizados incluídos

- NUnit não é exigido: fixtures standalone de console em .NET 8 com asserções.
- Registry C#, descarregamento por revisão, isolamento entre dois UIDs, eventos e snapshots.
- Descritores \`ACHMETA1\` emitidos pelo Worker real, limites e payload inválido.
- Compilação do exemplo original e verificação de aceitação do DLL **apenas com conquistas** pelo \`CompiledAssemblyProbe\`.
- Descoberta C++ a partir do código-fonte original em C#, com identidade \`ACHIEVEMENT\` sem registro de conteúdo nativo.
- Mailbox C++ e parser de descritores C++, incluindo casos negativos.
- Syntax check dos hooks com \`-fno-exceptions -fno-rtti\` e headers de produção.
- SDK public reference gerada e checada.
- Workflow específico e regressões adjacentes (Localization, Rarity, AnglerQuest, Command, Dust, Invasion etc.) no GitHub Actions.

## Pendente antes da conclusão

1. Instalar os objetos \`Achievement\` e as condições em \`Terraria.Main.Achievements\` de forma reversível por UID. A etapa atual prepara descritores e processa progresso no C#, mas **ainda não os registra no manager vanilla**.
2. Integrar nomes e descrições localizados, folha 592×64, ícones no menu, popup de conquista e ocultação \`???\`.
3. Persistência vinculada ao perfil de conquistas real, sem duplicar os contadores vanilla no reload; implementar a mesma proteção do ExampleModBunny ao reler o arquivo.
4. Confirmar as assinaturas e o comportamento reais dos hooks no Terraria Android 301720, gameplay local e multiplayer, custo em FPS e teste de unload/reload.
5. Portar \`ExampleBossKilled\` somente quando o ID real do ExampleBoss em C# estiver disponível.

**Status: WIP; nenhum APK ou .bmod foi gerado.** Compilar no CI não equivale a validar em Android. Não fazer merge automático. A PR #141 e seus loaders de Localization, Rarity, AnglerQuest e as PRs paralelas continuam separadas.
