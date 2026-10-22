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
- `NativeHairRegistry.h`: registro atômico de lotes e faixas não
  consecutivas por `RegisterAt`, IDs por sessão, nomes estáveis, validação
  de recursos, fallback _Alt, chave de personagem H1 e protocolo de
  aparência usando UID/classe. `NativeHairManifest.h` aceita o primeiro
  ID reservado para não recompactar cabelos de mods distintos.
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
- `CompiledAssemblyProbe.Verify` agora chama
  `CompiledHairProbe.RejectUninstalledHairTypes` sobre os tipos **da
  assembly real** antes de executar `SetDefaults`: um `ModHair` herdado
  indiretamente, que escape ao parser textual C++, também fica bloqueado
  sem carregar metade de um mod misto.
- `NativeHairNetworkState.h`: contrato de rede C++ que transmite
  `H1|jogador|UID/classe` e limpa cabelo de mod com `H1|jogador|#tipoVanilla`.
  Rejeita falsificação de remetente, IDs acima de 227 no formato vanilla,
  payloads incorretos e rebroadcast de mensagens duplicadas; recodifica
  o ID local entre clientes que carregam os mods em ordens diferentes.
  **O transporte NetPacket/NetManager ainda não está conectado**.
- A referência pública gerada do SDK foi sincronizada pela workflow
  `sync-csharp-hair-sdk-reference.yml` sem APK; as assinaturas do
  HairLoader são rotuladas como `hair_manifest_only_native_lifecycle_blocked`.
- A auditoria do `ExampleModCSharp` permite explicitamente somente
  `Examples/Hairs/ExampleHair.cs` além dos seis espelhos da fixture e
  confirma que o cabelo **não** integra `Playable/`.

- `NativeHairReservation.h`: reserva blocos contíguos em startup para
  diversos mods C#, ignorando slots ocupados em `PlayerHair` e
  `PlayerHairAlt` (inclusive cabelos registrados pelo Bunny JS).
  Um unload libera só a faixa daquele mod sem renumerar os restantes.
  O host ainda precisa **usar esse alocador** antes de pedir a compilação.
- `NativeHairSaveState.h`: guarda RAII para gravar 0 no arquivo vanilla e
  restaurar o cabelo local em memória. `PendingRestore` adia o retorno
  ao ID de mod até as texturas estarem prontas.
- `NativeHairSidecar.{h,cpp}`: arquivo de versão `H1` independente do
  `.plr.bl.json` do PlayerLoader JavaScript. Gravação atômica,
  verificação de tamanho, leitura sem seguir symlinks e chave por UID/classe.
- `NativeHairSaveSession.h`: preserva dados de um mod temporariamente
  ausente, mas remove o sidecar se o jogador escolher outro penteado
  e o salvamento concluir com sucesso.
- `NativeHairPlayerIo.{h,cpp}`: implementação isolada dos hooks IL2CPP
  de `InternalSavePlayerFile` e `LoadPlayer`, com checagem de assinatura,
  campos e cloud save, usando os contratos de save anteriores.
  **Ainda não está vinculada nem ativada pelo ciclo de produção.**
- A verificação de organização do repositório voltou a passar depois
  da recuperação de `docs/STATUS.md`.

- Em `ModHair.cs`, `Type` já é atribuído antes de `AutoStaticDefaults`
  e `SetStaticDefaults`, como no tModLoader; se ocorrer erro, a inscrição
  gerenciada é revertida, sem consumir ID.

## Contrato que ainda precisa estar completo antes de APK

1. Ligar `CompiledHairProbe.Export` ao worker MonoVM e ao
   `AutoSourceRuntime`; usar `TypeReservations` para reservar cada
   faixa global **depois** do loader JS e importar manifestos via
   `ParseAndRegisterManifest(..., firstReservedType)` em `Registry`.
   A preservação de lacunas e a rejeição de colisões já possuem testes
   host, mas ainda faltam o coordenador de startup e o hook do jogo.
   Remover o bloqueio do `SourceLoader` **somente depois** de validar
   o ciclo completo.
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
5. Instalar os hooks `NativeHairPlayerIo` somente depois de confirmar as
   assinaturas e a disponibilidade de todos os módulos em Android; fechar
   a política de replace/remove de sidecar, salvar, recarregar e cloud.
   O contrato usa arquivo `.plr.bl.csharp-hair` independente do JSON JS,
   preservando o arquivo `.plr.bl.json` sem parsing do motor JavaScript.
   Os testes host incluem RAII, falha, symlink, mod ausente e troca vanilla.
6. Ligar `NetworkState::EncodeOutgoing/Receive` ao transporte nativo
   `NetPacket`/`NetManager` após a mensagem vanilla 4. O contrato host
   já cobre autenticação do sender, fallback, supressão de duplicata,
   mudança para cabelo vanilla e mapeamento entre ordens de mods, mas
   ainda faltam hooks reais, entrada tardia, desconexão e QA multiplayer.
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
dotnet run --project Split/Bunny-Loader/tests/hair_loader/HairRuntimeGate.Tests.csproj -c Release
g++ -std=c++20 -Wall -Wextra -Werror \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/native_reservation.cpp \
  -o /tmp/native-hair-reservations && /tmp/native-hair-reservations
g++ -std=c++20 -Wall -Wextra -Werror \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/native_save.cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairSidecar.cpp \
  -o /tmp/native-hair-save && /tmp/native-hair-save
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
g++ -std=c++20 -Wall -Wextra -Werror \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/tests/hair_loader/native_network_state.cpp \
  -o /tmp/native-hair-net && /tmp/native-hair-net
g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairTextures.cpp
g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairUi.cpp
g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
  -I Split/Bunny-Loader/app/src/main/cpp \
  Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/NativeHairPlayerIo.cpp
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
esta branch. A PR #134 permanece Draft. Testes de host e compilação de
sintaxe **não** demonstram que hooks nativos tenham sido instalados ou
executados no Android.

Documentação da referência:
https://docs.tmodloader.net/docs/stable/class_mod_hair.html
