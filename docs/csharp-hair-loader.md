# HairLoader C# — descoberta de fonte, manifesto e hooks nativos (sem APK)

**Estado: implementação em andamento, não disponível no Terraria Android.**

Fonte ativa: `Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/ModHair.cs`.
Base técnica: `ModHair` do tModLoader (API pública) e `HairLoader.js` /
`ModHair.js` do Bunny Loader original, sem copiar o motor JavaScript para C#.

## O que está implementado

- `ModHair` e alias `BunnyHair`, com nomes próximos do tModLoader:
  `Type`, `Texture`, `AltTexture`, `AvailableDuringCharacterCreation`,
  `RandomizedCharacterCreationGender`, `AutoStaticDefaults`, `SetStaticDefaults`,
  `GetUnlockConditions`.
- `Condition` tipada em C#: predicado testável e reavaliado quando se consulta
  a lista do Stylist, sem guardar um resultado que possa ficar obsoleto.
- Registro C# por UID/classe: duplicatas, caminhos inseguros, ausência da
  textura principal e valores de gênero inválidos são recusados.
- `ExampleHair.cs` comentado na pasta `Examples/Hairs/`; ainda **não**
  copiado para `Playable/`, para evitar um exemplo não funcional no Android.
- `Reset()`, snapshots ordenados e resolução de cabelo salvo por nome estável.
- Inclusão do contrato no `BunnySourceSdk.csproj` realmente compilado pelo Roslyn.
- O `SourceParser.cpp` reconhece `ModHair` e `BunnyHair` no domínio
  `HAIR` junto com outros tipos do pacote, mas o `SourceLoader.cpp` recusa
  a ativação **antes** de registrar qualquer conteúdo enquanto a ponte
  completa (UI, save, rede, unload) não estiver conectada.
- Testes C# sem NuGet, smoke do parser C++ e GitHub Actions apenas de fonte,
  sem Gradle/Android/APK.
- `NativeHairRegistry.h`: pré-registro atômico de lotes, IDs por sessão,
  recuperação por nome estável, limites, checagem de arquivos, fallback _Alt,
  chave de personagem versão H1 e pacote de aparência usando UID/classe.
- O servidor só aceita aparência do próprio remetente; clientes só aceitam
  atualização vinda do servidor (índice 256). O cliente converte UID/classe
  para seu próprio tipo local, sem confiar no número recebido.
- `NativeHairTextures.{h,cpp}`: instalador IL2CPP isolado que amplia
  `TextureAssets.PlayerHair` e `PlayerHairAlt` com cópias, fixa arrays e
  assets enquanto carrega PNGs e volta às tabelas anteriores se falhar.
  **Ainda não é chamado por nenhum hook de produção**.
- `NativeHairUnlockState.h` e `NativeHairUi.{h,cpp}`: hooks de fonte
  para `HairstyleUnlocksHelper.ListWarrantsRemake/RebuildList`, com
  validação de metadados, fallback conservador e reavaliação do Stylist
  limitada a intervalos de 250 ms para evitar consultas Mono por frame.
  **Os hooks ainda não são acionados pelo carregador de produção.**
- `HairManifestSession.Prepare()`: reconhece subclasses concretas a partir
  dos tipos do assembly C# compilado, ordena pelo nome, recebe um primeiro
  ID reservado pelo host e desfaz todas as instâncias em falhas.
- `CompiledHairProbe.Export()`: a ponte preparada para o worker Roslyn/MonoVM,
  escrita no `BunnySourceCompiler`, exige UID e diretório reais do mod,
  confere arquivos e bloqueia symlinks; remove sidecars antigos de execuções
  que falharam. **Ainda não é invocada pelo ciclo de produção**.
- `ExportNativeManifest()` + `NativeHairManifest.h`: protocolo V1
  determinístico de oito campos por cabelo, com classe/UID, caminho PNG,
  gênero e disponibilidade na criação; o parser C++ recusa registros
  parciais, IDs divergentes ou caminhos inseguros. A ponte Mono que envia
  os dados permanece pendente.
- O instalador de texturas recusa qualquer ID ocupado por outro mod.
- Em `ModHair.cs`, `Type` já é atribuído antes de `AutoStaticDefaults`
  e `SetStaticDefaults`, como no tModLoader; se ocorrer erro, a inscrição
  gerenciada é revertida, sem consumir ID.

## Contrato que ainda precisa estar completo antes de APK

1. Ligar `CompiledHairProbe.Export` ao worker MonoVM e ao
   `AutoSourceRuntime`; reservar a faixa global de IDs entre vários mods,
   passar o manifesto tipado ao `NativeHairManifest` e remover o bloqueio
   do `SourceLoader` **somente depois** de validar todo o ciclo.
2. Acionar o instalador de texturas na thread IL2CPP, com resolução real do
   diretório do mod e instalação antes da UI. Validar retorno de erro, unload,
   reinstalação e falhas de texture load no Android. O instalador C++ está
   implementado **mas não está conectado ao jogo**.
3. Conectar os hooks IL2CPP já escritos ao ciclo nativo de instalação e
   completar o callback C# de `GetUnlockConditions` sem bloquear os frames.
   Confirmar lista e UI de criação/Stylist no Android; os testes host
   verificam somente compilação, comportamento lógico e fail-closed.
4. Aplicar o gênero sugerido no sorteio de criação de personagem, sem restringir
   o jogador a um gênero na escolha manual.
5. Conectar o contrato H1 ao save extra do personagem do Bunny. O arquivo
   vanilla deve conter 0 durante save e ser restaurado mesmo em exceção; o
   arquivo extra mantém UID/classe. Os testes H1 **não** substituem esse hook.
6. Conectar a validação H1 aos hooks de envio e recebimento de pacote
   multiplayer e fazer QA com dois clientes de ordens diferentes. Os testes
   locais cobrem anti-spoof, identidade e fallback, não a rede real.
7. Evitar chamadas MonoVM por pixel ou por desenho; avaliação de condições e
   criação das texturas devem ocorrer apenas nos pontos de atualização corretos.
8. Testar descarregamento, múltiplos mods, colisões de nomes, trocar de mundo,
   salvar/reabrir, hot-reload e erros de arquivos de textura.
9. Validar num aparelho Android os cenários vanilla e de mod, incluindo
   estabilidade, logs limpos e FPS. Só **depois** autorizar APK.

## Como rodar os testes de fonte

Na raiz deste repositório:

```sh
dotnet build Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release
dotnet run --project Split/Bunny-Loader/tests/hair_loader/HairLoader.Tests.csproj -c Release
dotnet run --project Split/Bunny-Loader/tests/hair_loader/HairCompiler.Tests.csproj -c Release
g++ -std=c++20 -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/parser_guard.cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/SourceParser.cpp \
  -o /tmp/bunny-hair-parser-check
/tmp/bunny-hair-parser-check
g++ -std=c++20 -Wall -Wextra -Werror \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/native_registry.cpp \
  -o /tmp/bunny-hair-native-contract
/tmp/bunny-hair-native-contract
g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairTextures.cpp
g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairUi.cpp
for name in native_unlock_state native_manifest; do
  g++ -std=c++20 -Wall -Wextra -Werror \
    -I Split/Bunny-Loader/app/src/main/cpp \
    Split/Bunny-Loader/tests/hair_loader/${name}.cpp \
    -o /tmp/${name} && /tmp/${name}
done
```

Esses testes **não** comprovam renderização, salvamento nem sincronização no
Terraria. Eles verificam o contrato C#, a descoberta por Roslyn, a
serialização de manifesto, o núcleo nativo isolado, a proteção de
carregamento e a sintaxe C++ dos hooks. Nenhum APK,
bmod ou pacote jogável deve ser publicado a partir desta etapa.
O job Android de `build-bunny-csharp-88-7.yml` recusa explicitamente
esta branch. A PR #134 permanece Draft.

Documentação da referência:
https://docs.tmodloader.net/docs/stable/class_mod_hair.html
