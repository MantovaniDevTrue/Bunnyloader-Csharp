# CloudLoader C# nativo — desenvolvimento isolado, SEM APK

Base do lote: `v1.0`, commit `cfbe784ee3cbc0ba2fabafce56c59e6eac41b67b`.
Branch: `feature/csharp-cloudloader-source-only-20261021`.
**Estado: BunnyBiome C# + dispatcher, reconhecimento no parser/Roslyn, manifesto nativo, worker MonoVM e coordenador de flags por geração codificados. O registro de biomas ainda está bloqueado pelo SourceLoader, e o lifecycle Android ainda NÃO instala ou ativa CloudLoader C#.**

## Fontes de referência obrigatórias

Somente Bunny Loader / ExampleModBunny, sem copiar tModLoader:

- `app/src/main/cpp/script/js/mod/Loaders/CloudLoader.js`
- `app/src/main/cpp/script/js/mod/ModCloud.js`
- `samples/ExampleMod/content/Content/Clouds/ExampleAdvancedCloud.js`
- `samples/ExampleMod/content/Content/Clouds/ExampleCloud.png` e `ExampleAdvancedCloud.png`

A implementação C# deve usar MonoVM e o núcleo C++ nativo, sem qualquer execução
indireta de código JavaScript. O ExampleModBunny é usado apenas para determinar
o comportamento correto e suas regras já validadas.

## O que já foi codificado

- `NativeCloudSelection.h`: seleção ponderada sem heap no sorteio, limites
  de peso e quantidade, peso vanilla de 22 nuvens comuns / 18 raras / 15
  raras em mundo de aniversário, janela de 60 tentativas, prioridade da
  rolagem rara e bloqueio de tipo sem máscara.
- `NativeCloudRegistry.h`: identidade por mod e classe, IDs desde o
  tipo fornecido pelo plano que respeita `CloudID.Count` **e** os IDs
  já ocupados pelo JS, rejeição de caminhos perigosos,
  segurança transacional de disponibilidade de textura/máscara e limpeza.
- `BunnyCloudDispatcher.cs`: registro C# por assembly, vínculo de IDs nativos, peso por bioma, `OnSpawn`, isolamento de exceções, várias instâncias e descarregamento.
- `NativeCloudIdPlan.h` / `NativeCloudIdProbe.cpp`:
  inspecionam `Terraria.ID.CloudID.Count`, o tipo e o tamanho real de
  `TextureAssets.Cloud` e `TextureMaskManager.CloudMasks`.
  A primeira ID C# fica **depois das duas tabelas**. Se o `Ready/setup`
  dos mods JS ou o `LoadMasks` ainda não estiverem comprovadamente
  concluídos, a reserva é negada em vez de disputar IDs. O probe não
  modifica nada no Terraria; a ativação real ainda não foi conectada.
- `NativeCloudManifest.h`: leitura estrita e transacional do protocolo
  `BUNNY_CLOUDS_V1` gerado pelo Roslyn. Duplicações ou entradas inválidas
  não deixam classes parcialmente registradas.
- `NativeCloudAssets.cpp`: instalação nativa **ainda não chamada pelo loader**.
  Prepara PNGs, duas tabelas IL2CPP separadas, máscaras do horizonte e
  seus handles GC. Publica máscaras antes das texturas e reverte ambas
  se qualquer verificação falhar. Rejeita IDs já ocupados por outro loader.
  A desinstalação também recusa apagar texturas enquanto os hooks ainda
  sorteiam nuvens ou existe alguma nuvem do mod ativa na tela.
- `NativeCloudSpawnLedger.h`: fila limitada de eventos `OnSpawn` para
  MonoVM, com gerações por slot; bloqueia resultados atrasados depois
  que o Terraria reutiliza um slot ou troca de mundo.
- `NativeCloudHookBindings.cpp`: valida as assinaturas exatas
  `void addCloud()` e `int RollRareCloud()` com offsets tipados.
  O campo de espelhamento confirmado pelo ExampleModBunny é
  **`Cloud.spriteDir`**, não `Cloud.spriteEffects`.
- `NativeCloudHooks.cpp`: implementa hooks ShadowHook encadeáveis com
  chamada ao original; só permite ativação mediante confirmação de
  máscaras prontas **e MonoVM**. Nenhum caminho do loader chama o
  instalador ainda. Quando desligado, usa integralmente o original.
  O hook usa cache de pesos e produz apenas eventos simples para o
  worker, sem invocar C# a cada frame.
- `NativeCloudRollCoordinator.h`: combina pesos recebidos do worker,
  preserva os pesos vanilla (22/18/15), requisição de refresh a cada 60
  tentativas e rejeição transacional de snapshots inválidos.
- `NativeCloudWeightWire.h`: leitura com validação por tipo e mod das
  respostas `BUNNY_CLOUD_WEIGHTS_V1`; rejeita IDs desconhecidos, pesos
  inválidos e atualizações parcialmente preenchidas.
- `BunnyCloudDispatcher.cs`: entrypoints `RegisterFromPath`,
  `WeightsWire` e `OnSpawnWire`, executados fora do frame.
- `ManagedRuntime.cpp`: integrado à thread MonoVM fixada existente,
  com jobs limitados de registro, vinculação de IDs, pesos e `OnSpawn`.
  Os retornos incluem a geração da sessão e dos lotes de pesos;
  callbacks atrasados são descartados. A troca de mundo invalida os
  callbacks em voo sem perder os assemblies e IDs registrados.
- `NativeCloudHooks.cpp`: `pumpNativeCloudManagedCallbacksOnGameThread`
  drena resultados de `OnSpawn` e publica pesos somente depois das
  respostas de todos os mods. Nuvens autoload recebem peso 1 sem
  criar jobs gerenciados; a thread de jogo nunca aguarda o MonoVM.
  **Ainda não chamado pelo ciclo de atualização real**, aguardando
  descoberta fiel de biomas e ativação das texturas/máscaras.
- `NativeCloudAutoload.cpp`: descoberta única de PNGs em qualquer
  `Clouds/` dentro de `content/`, com limite de profundidade e arquivos,
  ignorando links simbólicos e duplicatas de classes, igual ao
  `ModFiles.ListTextures('Clouds')` do Bunny original.


- `SourceParser.cpp` / `SourceContentIdentity.h`: reconhecimento explícito de `BunnyCloud` e domínio `CLOUD`; `SourceLoader.cpp` recusa nuvens antes de registrar itens enquanto não houver máscaras e hooks nativos. O bloqueio vale tanto para classes `BunnyCloud` quanto para **PNGs autoload sem classe**, para que o mod não perca conteúdo silenciosamente.
- `CompiledAssemblyProbe.cs`: reconhecimento por reflexão e geração do catálogo isolado `source-clouds-v1.txt`, sem alterar o formato V2 dos demais itens.
- `tests/cloud_loader/`: exemplos executáveis das duas classes de nuvens,
  casos de PNG ausente, peso inválido, duplicação, mundo de aniversário,
  isolamento de `OnSpawn`, vazamento de estado entre tentativas e tamanho
  do array de máscaras.
- `.github/workflows/check-csharp-cloudloader-source.yml`: **somente**
  testes de código-fonte C++ estritos e sanitizers; sem Gradle, APK ou `.bmod`.

Os testes do núcleo e dos exemplos C# NÃO provam renderização no Terraria Android.

## API C# compilável e exemplos no código

Foi adicionado `experiments/csharp/BunnyCsSdk/BunnyCloud.cs` ao
`BunnySourceSdk.csproj`. Ele define o contrato **experimental**:
`BunnyCloud`, `BunnyCloudEnvironment`, `BunnyCloudSpawn` e
`BunnyCloudSpriteEffects`. A chance é normalizada por
`BunnyCloud.SafeSpawnChance`; nomes de biomas são identificadores internos,
não nomes traduzidos.

Duas implementações comentadas e testes compiláveis estão em:

- `tests/cloud_loader/managed_examples/ExampleCommonCloud.cs`:
  nuvem comum de peso 1, como o exemplo de textura do Bunny original.
- `tests/cloud_loader/managed_examples/ExampleAdvancedCloud.cs`:
  nuvem rara, peso 10 dentro do bioma do exemplo, 1 fora, `OnSpawn`
  removendo o espelhamento de seu PNG com letras.
- `tests/cloud_loader/managed_examples/CloudContractTests.cs`:
  testes das chances, contextos, parâmetros, proteção de desenho e tipo.
- `tests/cloud_loader/CloudContractTests.csproj`: testes .NET 8 do dispatcher e da inspeção real do compilador Roslyn, **sem build de Android**.
- `tests/cloud_loader/source_parser_cloud_test.cpp`: teste do parser com classe C# real e sem hooks de Terraria.

A estrutura original do Bunny Loader é usada como comportamento de referência.
**As classes já são reconhecidas pelo parser e compilador, mas ainda NÃO são registradas como nuvens visíveis no Terraria.** Seus testes não comprovam carregamento no jogo.
O bioma `ExampleModCSharp/ExampleSurfaceBiome` é uma chave de referência
para o teste, não um bioma já implementado no C#.

```bash
dotnet run --project tests/cloud_loader/CloudContractTests.csproj -c Release
dotnet build experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release
```

## Fidelidade de biomas: dependência ainda não implementada

O exemplo original `ExampleAdvancedCloud.js` consulta
`Main.LocalPlayer.InModBiome(ExampleSurfaceBiome)`. Esse bioma é
definido em `samples/ExampleMod/content/Content/Biomes/ExampleSurfaceBiome.js`
e depende de três condições simultâneas:

- Contagem `ExampleBiomeTileCount.exampleBlockCount >= 40`;
- Jogador na região central do mundo, segundo `maxTilesX`;
- `ZoneSkyHeight` **ou** `ZoneOverworldHeight`.

Esses três dados NÃO equivalem simplesmente a `ZoneOverworldHeight`.
A política `NativeCloudBiomePolicy.h` agora reproduz os três critérios
e ainda exige registro de um bioma C# verdadeiro. O sampler
`NativeCloudBiomeSampler.cpp` valida os campos reais de
`Terraria.Main` e do jogador local (menu, zonas, posição e modo de
aniversário), depois tenta ler em O(1) a contagem da
`Main.PlayerSceneMetrics._tileCounts[ExampleTile]`, usando
`bl::runtime::modTileTypeByName`, já existente no Bunny Loader C++.

Enquanto não existir `BunnyBiome` registrado no host C#, o sampler
define `exampleBiomeRegistered=false`. Portanto **mesmo 40 blocos
não ativam a chave do bioma do exemplo**; a nuvem rara conserva
`SpawnChance=1` fora de um bioma C# registrado. Isso impede uma
simulação enganosa por mera altura, sem usar o JavaScript.

O método `noteAutoGameUpdate` já chama o pump protegido
`pumpNativeCloudManagedCallbacksOnGameThread` quando (e só quando)
o hook C# foi realmente ativado por uma etapa externa e o mundo está
pronto. O snapshot é renovado a cada 20 atualizações; os resultados
de `OnSpawn` são drenados a cada quadro. Se qualquer contrato
IL2CPP não corresponder à versão real do Terraria, a seleção
C# é pausada e as nuvens vanilla continuam disponíveis.

## BunnyBiome C# — novos contratos e limites

O SDK `BunnySourceSdk.dll` possui `BunnyBiome`,
`BunnyBiomeSnapshot` e `BunnyBiomeDispatcher`, baseados exclusivamente
no `ModBiome` e `BiomeLoader` do ExampleModBunny (sem JavaScript em
runtime). Cada classe declara suas dependências em `CountedTiles`
e calcula `IsBiomeActive(snapshot)`, recebendo apenas tipos primitivos
na thread MonoVM. Os callbacks `OnEnter`, `OnInBiome` e
`OnLeave` agora executam na avaliação gerenciada na mesma ordem do
`BiomeLoader.js`: primeiro a comparação do estado, em seguida a
entrada ou saída (se houve mudança), depois `OnInBiome` se ativo.
Exceções são isoladas por callback, sem desativar permanentemente
o bioma que lançou. Esse pipeline **ainda não coincide com a frequência
de `Player.UpdateBiomes` por quadro**, pois a coleta nativa permanece
amostrada a cada 20 updates.

O Roslyn reconhece `BunnyBiome` e gera o manifesto separado
`source-biomes-v1.txt`, com versão `BUNNY_BIOMES_V1`, classe e
dependências de tiles. O parser nativo atribui o domínio `BIOME` separado
de `CLOUD`. O `SourceLoader.cpp` rejeita qualquer `BunnyBiome`
declarado enquanto `NativeBiomeRegistryNotInstalled` for verdadeiro:
assim, o mod não carrega perdendo conteúdo silenciosamente.

No núcleo nativo, `NativeBiomeManifest.h` confere IDs e nomes por
proprietário de mod e rejeita snapshots adulterados. O mesmo manifesto
é confirmado pelo `BunnyBiomeDispatcher.TilesWire` executado pelo
**worker MonoVM fixado existente**, antes da aprovação de cada
registro. `readNativeBiomeTileCountsWire` faz um snapshot com no máximo
32 dependências do mod usando `ModTiles.modTileTypeByName` e
`PlayerSceneMetrics._tileCounts`, sem varredura de blocos.

`NativeBiomeGameBridge` prepara o registro, recebe os resultados do
worker por geração, verifica novamente cada chave e passa somente flags
C# autenticadas para o cache de chances do CloudLoader. Os resultados
são invalidados em troca de mundo ou novo snapshot; não há chamadas
MonoVM dentro de `Cloud.addCloud`. Como a inscrição de biomas pelo
carregador principal ainda não está habilitada, esse pipeline fica
**inativo no jogo**.

Os testes executam uma versão C# do `ExampleSurfaceBiome` (40 blocos
`ExampleTile`, posição central, altura sky/overworld), isolamento de
exceções, negativos, JSON inexistente, chaves por mod, manifesto Roslyn,
parser C++ e sanitizers. Tudo é teste de fonte; ainda faltam os
hooks de ciclo `UpdateBiomes` e QA Android.

## Bloqueio de ativação confirmado

A checagem `SourceLoader.cpp` retorna
`Reason=NativeCloudHooksAndMasksNotInstalled` para qualquer mod C# que
declare uma classe `BunnyCloud`. O compilador já pode descobrir as
classes e gerar o manifesto `source-clouds-v1.txt` em testes isolados,
mas o runtime nunca deve anunciá-las como conteúdo registrado.

Isto é intencional: o céu mobile exige máscaras válidas em
`TextureMaskManager.CloudMasks` para cada tipo, e a ausência desse
passo pode causar interrupção do renderizador. O manifesto é apenas
metadado, não uma ponte para QuickJS.

## Segurança de coexistência e troca de mundo

O registro original JS usa `CloudID.Count + ByType.size`, e
cresce suas tabelas somente na etapa `Ready.Add(..., 'setup')`.
Reservar `CloudID.Count` diretamente em C# poderia sobrepor uma
nuvem JS. O plano novo considera o maior tamanho das duas tabelas
já estabilizadas. **Ele não considera seguro um estado intermediário.**

Na transição de mundo, `AutoSourceRuntime.cpp` invalida
as solicitações pendentes do MonoVM; `resetManagedCloudWorldSession`
mantém os assemblies e IDs do mod carregados para reutilização
posterior. O unload completo usa uma função diferente.

No teardown gráfico, `uninstallNativeCloudAssets` verifica
os hooks e as nuvens ainda ativas em `Main.cloud` antes de liberar os
`Texture2D`. Qualquer falha de inspeção mantém os recursos vivos
e recusa a restauração, para não deixar o renderizador com ponteiros
para texturas destruídas.

O CI executa `native_cloud_id_plan_test.cpp`, casos com
`CloudID.Count`, IDs JS existentes, máscaras maiores que texturas,
setup incompleto e UUIDs com hífens. `SourceLoader.cpp` também é
analisado pelo Clang/GCC em desktop com um cabeçalho `android/log.h`
restrito aos testes, **sem alterar o código Android real**.

## O que falta para finalizar o CloudLoader inteiro

- [x] Criar `BunnyCloud` experimental, contrato gerenciado, exemplos
      anotados e testes de compilação C# no BunnySourceSdk.
- [x] Compilar SDK, exemplos C#, código C++ e o manifesto do Roslyn no CI sem APK.
- [ ] Estabilizar a API com o host nativo e validar em aparelho antes de classificá-la como utilizável em jogo.
- [x] Reconhecer `BunnyCloud` no parser, reservar domínio `CLOUD` e exportar catálogo Roslyn separado.
- [x] Implementar planejamento seguro de IDs após JS e validação nativa das duas tabelas, somente leitura.
- [x] Detectar nuvens PNG-only sem classes e bloquear carregamento incompleto antes de registrar outros conteúdos.
- [ ] Completar registro nativo real e instalação automática de texturas e máscaras antes da entrada no mundo.
- [x] Codificar instalador transacional nativo e verificação de máscaras,
      com rollback e preservação das tabelas vanilla.
- [ ] Acionar o instalador no momento correto do ciclo de vida do jogo
      e validar as classes e máscaras reais em Android; na falha,
      bloquear aquele tipo sem afetar nuvens vanilla.
- [x] Codificar a resolução estrita das assinaturas IL2CPP e offsets tipados.
- [x] Codificar hooks nativos encadeáveis que preservam a chamada vanilla
      e as dimensões do sprite; bloqueados enquanto não existir ponte MonoVM.
- [ ] Validar as assinaturas e offsets em Terraria Mobile rodando e
      instalar/ativar os hooks no ciclo de vida real.
- [x] Implementar dispatch C# de `SpawnChance` e `OnSpawn` com identificação por mod, limites e isolamento de exceções.
- [x] Implementar cache C++ e entrypoints C# de snapshot por mod e
      callback de espelhamento. Testados sem Android.
- [x] Executar os entrypoints no worker MonoVM fixado do runtime
      e disponibilizar os resultados por caixas de mensagens delimitadas.
- [x] Codificar o pump de respostas na thread do jogo, com atualização
      transacional dos pesos e proteção contra resposta antiga por geração.
- [x] Conectar o pump protegido à atualização real do Terraria:
      somente quando hooks/máscaras já estiverem ativos; snapshot
      periódico de flags vanilla e contagem nativa de tiles em O(1).
- [x] Criar contratos `BunnyBiome`, dispatcher C#, reconhecimento
      no parser/Roslyn, manifesto nativo e registro/avaliação
      assíncronos no worker existente.
- [x] Codificar a leitura genérica de `SceneMetrics._tileCounts`
      de até 32 tiles declarados e a publicação segura de
      biomas após verificação por proprietário e geração.
- [x] Implementar no dispatcher a ordem original
      `OnEnter` → `OnInBiome` e saída `OnLeave`, com isolamento
      de exceções, retorno após falha e limpeza de estado por mundo.
- [x] Agendar `BunnyBiomeDispatcher.ResetWorld` no worker
      MonoVM fixado quando a sessão do Terraria muda, sem bloquear
      a thread do jogo nem carregar flags antigas.
- [ ] Associar a avaliação e a execução dos três callbacks ao hook real
      `Player.UpdateBiomes` por quadro, sem perder 60 FPS.
- [ ] Habilitar registro de `BunnyBiome` pelo carregador de fontes
      somente após fechar a inicialização nativa e QA Android.
- [x] Preparar fila `OnSpawn` por slot/geração com limites e testes
      para ignorar resultados de callbacks atrasados.
- [x] Codificar atualização de tamanho de nuvem pelo PNG, seleção rara
      sobrepondo a comum, checagem de slot ativo e resposta `spriteDir`.
- [ ] Confirmar esse comportamento rodando no Terraria Android e corrigir
      eventuais diferenças de layout IL2CPP.
- [x] Cobrir em testes sem Android: múltiplos mods, mundo de aniversário,
      PNG simples, colisões de ID, pesos inválidos e gerações de slots.
- [x] Codificar limpeza de jobs por época do mundo, preservando classes/IDs,
      e recusa de desinstalação com nuvens ativas.
- [ ] Cobrir no Terraria real: recarga de mundo, desconexão,
      recompilação, assets ausentes, desinstalação e ausência de leaks.
- [x] Criar classes C# de nuvem comum e avançada comentadas e testes de
      contrato fora da pasta Playable, evitando distribuição prematura.
- [ ] Integrar essas classes ao ExampleModCSharp jogável com os PNGs originais
      e testar a geração real das nuvens na versão Android.
- [x] Validar suíte de código-fonte em CI, com C#, GCC, Clang,
      sanitizers e simulações dos contratos.
- [ ] Abrir QA visual Android somente quando os pontos de integração
      estiverem fechados (céu, máscara, horizonte, chance, OnSpawn e FPS).

**Regra de entrega:** nenhuma build Android, nenhum APK, nenhum `.bmod`,
nenhum merge e nenhuma classificação como `device_verified` antes da
implementação e validação completa. O branch é apenas fonte em andamento.

## Executar apenas testes nativos

```bash
bash tests/cloud_loader/run_native.sh
```

O runner compila com C++20, `-Wall -Wextra -Werror -pedantic`. A Action
específica também executa AddressSanitizer e UndefinedBehaviorSanitizer e
verifica a sintaxe real do instalador de assets e do resolvedor IL2CPP
com GCC e Clang, sem executar código Android.


## Atualização — integração MonoVM e hooks de nuvens

O worker fixado em `ManagedRuntime.cpp` ganhou seis funções nativas
de envio/leitura/pausa e uma fila de até 64 solicitações. As classes
C# são registradas e associadas aos IDs nativos no worker, e os
métodos `SpawnChance`/`OnSpawn` nunca são chamados por `addCloud`.
O mecanismo usa a mesma API Mono dinâmica e o mesmo arquivo
`BunnySourceSdk.dll` empregados pelo registro de entidades C#.

`NativeCloudHooks.cpp` oferece o pump completo do lado da thread do
Terraria, incluindo respostas atrasadas por sessão, slot e geração de
refresh, mas **não é chamado automaticamente**. Para ligar com segurança
falta obter o estado de bioma *real* do jogador e resolver a ordem de
instalação do `TextureMaskManager` / `TextureAssets` e a concorrência
de IDs com o CloudLoader JS.

O CI do lote também verifica a compilação da unidade **real**
`ManagedRuntime.cpp` (GCC e Clang), além do SDK C# e de todos os
testes isolados. Compilação não equivale a testes IL2CPP no aparelho.

## Atualização — snapshot nativo e retomada do worker

- `NativeCloudBiomePolicy.h` e testes exigem simultaneamente os
  40 `ExampleTile`, a região central do mapa, altura correta e
  um bioma C# registrado antes de retornar `ExampleSurfaceBiome`.
- `NativeCloudBiomeSampler.cpp` inspeciona o jogador real e lê os
  contadores nativos já produzidos por `SceneMetrics`; não faz
  varreduras massivas de blocos nem consulta o JavaScript.
- `AutoSourceRuntime.cpp` conecta o pump ao update, usando
  snapshots a cada 20 frames e pausando com segurança quando os
  contratos não existem. A troca de mundo pausa os hooks e
  descarta callbacks pendentes.
- `ManagedRuntime.cpp` agora repete uma limpeza C# malsucedida
  com intervalo de 250 ms, sem deixar `g_cloudDispatcherHealthy`
  permanentemente falso por uma tentativa feita antes do MonoVM estar
  disponível.
- O CI verifica também os arquivos reais `AutoSourceRuntime.cpp`
  e `NativeCloudBiomeSampler.cpp` com Clang e GCC. Continua
  **sem build Android**: assinatura e renderização em aparelho
  permanecem sem validação.

## Ciclo de vida gerenciado — lote atualizado

O `BunnyBiomeDispatcher` agora guarda o estado anterior por
instância e executa os três eventos na ordem do carregador Bunny.
Quando `IsBiomeActive` lança, vale falso naquela avaliação (em vez de
desabilitar permanentemente o bioma). `OnEnter`, `OnInBiome` e
`OnLeave` têm isolamento independente de exceções; um callback ruim
não suprime os demais biomas nem invalida as flags do snapshot.

Na saída ou recarga do mundo, `resetManagedCloudWorldSession` coloca
um job `ResetWorld` na fila MonoVM, limpa avaliações antigas e aciona
um `OnLeave` final para instâncias ativas. O reset é idempotente;
uma falha deixa os IDs C# bloqueados, dispara a rotina de recuperação
e evita reutilizar registros possivelmente inconsistentes.

Os testes do `ExampleSurfaceBiome` verificam entrada uma vez,
`OnInBiome` em avaliações sucessivas, saída exatamente uma vez,
reentrada após `ResetWorld` e falhas independentes em todos os hooks.
A compilação real de `ManagedRuntime.cpp`, testes C# e sanitizers
passou em https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39785023797.

**Limite atual:** a execução é fiel *por avaliação*, mas ainda
não roda sincronamente com cada `Player.UpdateBiomes` original.
O `SourceLoader` continua recusando `BunnyBiome` e `BunnyCloud`;
não houve build Android, pacote de mod ou teste no aparelho.
