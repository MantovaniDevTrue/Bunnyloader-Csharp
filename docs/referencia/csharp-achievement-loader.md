# AchievementLoader C# — desenvolvimento isolado

Base: ExampleModBunny de Jonata, ModAchievement.js, AchievementLoader.js, ExampleBossKilled.js e ManyWormsKilled.js.

## Núcleo implementado nesta branch

O SDK contém BunnyAchievement, BunnyAchievementCondition e BunnyAchievementRegistry.
Suporta flag, inteiro, float, mortes de NPC, coleta/fabricação de itens e destruição de tiles.
Cada condição tem chave estável, progresso limitado ao máximo e identidade UID/classe.
Conquistas com múltiplas condições exigem todas concluídas. OnCompleted ocorre só uma vez por conclusão.
O salvamento BACH1 armazena progresso por condição e preserva entradas de mods temporariamente ausentes.

Teste somente do núcleo gerenciado:

    dotnet run --configuration Release --project tests/achievements_csharp/AchievementCoreTests.csproj

Os exemplos C# são compilados dentro do teste, mas ainda não são conteúdos jogáveis:

- samples/ExampleModCSharp/Examples/Achievements/ExampleBossKilled.cs
- samples/ExampleModCSharp/Examples/Achievements/ManyWormsKilled.cs

## Pendências obrigatórias

- Descobrir subclasses no .bmod e registrar no carregamento C# sem QuickJS.
- Receber eventos reais de Terraria via ponte ARM64 com ciclo de vida, filtro local e antiduplificação.
- Registrar Achievement e conditions/trackers em Terraria.Main.Achievements.
- Implementar ícones mobile, categorias, localização, visibilidade oculta e popup.
- Integrar persistência ao save real sem mexer em conquistas vanilla.
- Cobrir mundo, vários mods, multiplayer, unload, stress, performance e QA Android.

ESTADO: WIP. Não gerar APK, não mesclar e não declarar pronto para o jogo.
O workflow desta branch executa apenas testes dotnet e não invoca Gradle.
