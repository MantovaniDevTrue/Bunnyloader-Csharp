# HairLoader C# — contrato gerenciado, sem APK

**Estado: implementação em andamento, não disponível no Terraria Android.**

Fonte ativa: \`Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/ModHair.cs\`.
Base técnica: \`ModHair\` do tModLoader (API pública) e \`HairLoader.js\` /
\`ModHair.js\` do Bunny Loader original, sem copiar o motor JavaScript para C#.

## O que está implementado

- \`ModHair\` e alias \`BunnyHair\`, com nomes próximos do tModLoader:
  \`Type\`, \`Texture\`, \`AltTexture\`, \`AvailableDuringCharacterCreation\`,
  \`RandomizedCharacterCreationGender\`, \`SetStaticDefaults\`,
  \`GetUnlockConditions\`.
- \`Condition\` tipada em C#: predicado testável e reavaliado quando se consulta
  a lista do Stylist, sem guardar um resultado que possa ficar obsoleto.
- Registro C# por UID/classe: duplicatas, caminhos inseguros, ausência da
  textura principal e valores de gênero inválidos são recusados.
- \`ExampleHair.cs\` comentado na pasta \`Examples/Hairs/\`; ainda **não**
  copiado para \`Playable/\`, para evitar um exemplo não funcional no Android.
- \`Reset()\`, snapshots ordenados e resolução de cabelo salvo por nome estável.
- Inclusão do contrato no \`BunnySourceSdk.csproj\` realmente compilado pelo Roslyn.
- Bloqueio explícito do \`SourceParser.cpp\` para impedir que um \`ModHair\`
  C# pareça instalado antes da ponte nativa real.
- Testes C# sem NuGet, smoke do parser C++ e GitHub Actions apenas de fonte,
  sem Gradle/Android/APK.

## Contrato que ainda precisa estar completo antes de APK

1. Registrar \`ModHair\` no **SourceParser** e \`SourceLoader\` com IDs,
   texturas e identidade, sem deixar a classe só no assembly do MonoVM.
2. Instalar \`TextureAssets.PlayerHair\` e \`PlayerHairAlt\` na thread IL2CPP,
   estender as duas tabelas sem substituir entradas de outros mods, e fazer
   rollback seguro em erro ou recarga.
3. Inserir os IDs na lista de criação de personagem e no Stylist, preservando
   itens vanilla, ordem, estado desbloqueado e reavaliação quando condições
   mudarem. Usar a API native de \`HairstyleUnlocksHelper\`.
4. Aplicar o gênero sugerido no sorteio de criação de personagem, sem restringir
   o jogador a um gênero na escolha manual.
5. Gravar cabelo vanilla compatível no arquivo do jogo e UID/classe no arquivo
   extra do Bunny; restaurar ao entrar, oferecer fallback para cabelo 0 quando
   o mod está ausente e não corromper personagens existentes.
6. Sincronizar multiplayer validando remetente, índice, mod e nome estável;
   garantir compatibilidade quando a ordem de mods/IDs difere entre clientes.
7. Evitar chamadas MonoVM por pixel ou por desenho; avaliação de condições e
   criação das texturas devem ocorrer apenas nos pontos de atualização corretos.
8. Testar descarregamento, múltiplos mods, colisões de nomes, trocar de mundo,
   salvar/reabrir, hot-reload e erros de arquivos de textura.
9. Validar num aparelho Android os cenários vanilla e de mod, incluindo
   estabilidade, logs limpos e FPS. Só **depois** autorizar APK.

## Como rodar os testes de fonte

Na raiz deste repositório:

\`\`\`sh
dotnet build Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release
dotnet run --project Split/Bunny-Loader/tests/hair_loader/HairLoader.Tests.csproj -c Release
g++ -std=c++20 -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/parser_guard.cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/SourceParser.cpp \
  -o /tmp/bunny-hair-parser-check
/tmp/bunny-hair-parser-check
\`\`\`

Esses testes **não** comprovam renderização, salvamento nem sincronização no
Terraria, apenas o contrato C# e as proteções de carregamento. Nenhum APK,
bmod ou pacote jogável deve ser publicado a partir desta etapa.

Documentação da referência:
https://docs.tmodloader.net/docs/stable/class_mod_hair.html
