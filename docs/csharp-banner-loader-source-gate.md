# BannerLoader C# — contrato e bloqueio de APK

Base: `integration/ready-prs-source-fixes-20261020`. Branch isolada: `feat/csharp-bannerloader-source-only-20261022`.

## Objetivo

Portar o **BannerLoader inteiro**, sem JavaScript na rota C#, preservando a logica do loader original em `app/src/main/cpp/script/js/mod/Loaders/BannerLoader.js` e a organizacao de registros do tModLoader. O codigo do tModLoader pode orientar as assinaturas publicas, mas toda chamada de jogo precisa corresponder a um simbolo real do Terraria Android. Nao confundir referencias de API com implementacao comprovada.

## Entregue neste primeiro lote (apenas fonte)

- `BunnyBannerDefinition`: UID persistente, NPC contador, NPC representante e nome da classe do item.
- `BunnyBannerRegistry.Register/TryBind`: nomes primeiro; IDs reais depois da carga, com detecao de colisao e recusa sem publicacao parcial.
- `NpcToBanner`, `BannerToItem`, `ItemToBanner`: tabelas tipadas de consulta, sem tocar IDs vanilla.
- `BunnyBannerState` e `Capture/Load/Restore`: progresso, premios pendentes e sinalizador `newBanner`; snapshots independentes da ordem dos IDs, inclusive de mods temporariamente ausentes.
- `BunnyBannerArrays.Grow`: prepara os cinco arrays (incluindo as duas SceneMetrics) para futura publicacao atomica pelo host IL2CPP; preserva indices existentes.
- `Unregister/ResetWorld`: vida util separada entre registro do mod e estado do mundo.
- Exemplo editavel em `samples/ExampleModCSharp/Examples/Banners/ExampleBannerRegistration.cs`, compilado como fonte vinculada ao projeto de testes. Ele ainda nao e carregado pelo jogo.

A API ja permite validar contratos de identidade, conflito e save em .NET, mas **nao registra um banner jogavel nem instala hooks reais**.

## Contratos restantes antes de declarar o loader completo

| Area original | Status |
| --- | --- |
| `Track`, `Finish`, vinculos classe/ID e verificacoes de conflito | Nucleo C# feito; falta ligar ao parser/Roslyn e ao registro de NPC/item |
| `Resize` e `Metrics` | Preparacao de arrays feita; falta publicacao segura no IL2CPP e restauracao quando o jogo recriar os arrays |
| `NPCtoBanner`, `BannerToItem`, `BannerToNPC` | Tabelas prontas; faltam hooks nativos ARM64 com original encadeado e bypass vanilla |
| `SceneMetrics.Reset` e `BannerSystem.Clear` | Pendente no host |
| `NetBannersModule.WriteFullState` e `BannerSystem.Save` | Pendente; rede vanilla deve ficar limitada a IDs vanilla |
| `BannerWorldSystem.SaveWorldData/LoadWorldData` | Snapshot C# pronto; falta sidecar real com integridade e migração entre versoes |
| `BannerWorldSystem.NetSend/NetReceive` | Pendente; autoridade de servidor, identidade por UID e rejeicao de pacotes invalidos |
| Refresh do inventario de banners + `bannerState` | Pendente; precisa executar no jogo depois do restore |
| ExampleMod C# (item, tile e textura de banner) | Pendente; deve usar o asset original e mostrar obtencao ao atingir kills necessarios |
| Multi-mod, recriacao de mundo, reentrada, desconexao, perf e Android | Pendente; testes de host nao substituem QA em aparelho |

## Testes previstos

`dotnet run --project tests/banners/Banners.HostTests.csproj -c Release`

O workflow `.github/workflows/check-csharp-bannerloader-source-only.yml` executa apenas compilacao .NET e testes do registry. Nao chama Gradle, nao empacota `.bmod` e falha se detectar qualquer `.apk`.

A implementacao so podera ser marcada **completa** depois de 100% dos contratos acima, compilacao do SDK e ExampleMod completos, teste de host, bridge, memoria IL2CPP, ARM64 e CI de fontes todos aprovados. A regra solicitada proibe **qualquer geracao de APK antes disso**. QA Android fica para a etapa posterior ao fechamento integral do codigo.

## Uso da API em C#

```csharp
var banners = new BunnyBannerRegistry(firstModNpc);
banners.Register(new BunnyBannerDefinition(
    "examplemodcsharp", "ExampleSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner"));
banners.TryBind("examplemodcsharp", "ExampleSlimeNPC",
    npcType, npcType, bannerItemType);
```

Os IDs no exemplo representam os IDs **reais** que o loader deve obter do registro de conteudo, nunca constantes do mod. Para uma variante de NPC, registre outro `NpcClass` com o mesmo `BannerNpcClass`. A classe `ExampleSlimeBanner` devera existir como item/tile C# real antes de ativar a ponte de jogo.

## Lote 2: Roslyn -> registro C++ nativo, sem APK

O `BunnyNPC` ganhou `Banner` e `BannerItem` como **nomes de classes locais**, pois o `int Type` do Terraria Android so existe depois do registro. Isso preserva o proposito de `Banner = Type` e `BannerItem = ModContent.ItemType<T>()` no tModLoader, sem inventar IDs estaveis.

A execucao verdadeira de `SetDefaults()` pelo compilador emite `NPC_BANNER\tNpcClass\tRepresentativeNpcClass\tBannerItemClass`. O `SourceLoader.cpp` resolve os tres nomes por tipo e dono nativos ja registrados, confere que o item e `BunnyItem` e faz preflight em `ManagedBannerRegistry.h`. Representantes precisam declarar a si mesmos; variantes compartilham o item e o contador. O snapshot C++ troca mapas completos sem locks nas consultas e descarta o mapeamento ao descarregar o mod.

A API **ainda nao ativa o BannerSystem do jogo**. Os mapas existem, mas nao interceptam NPCtoBanner, BannerToItem, BannerToNPC, SceneMetrics, morte de NPC, save ou multiplayer. Nenhum contador de kills e incrementado por esta etapa. Nao declarar bandeira funcional/colocavel no Android ate concluir os hooks.

O `ExampleSlimeNPC` e `ExampleInvasionEliteSlimeNPC` foram configurados no ExampleMod C# para compartilhar a bandeira. `ExampleSlimeBanner.cs` usa o PNG binario original do ExampleMod Bunny. O item existe como fonte editavel e e registrado, mas **nao pode ser colocado ainda**: o `BunnyTileShape` nativo atual suporta apenas `Single` e `Style2x1`, nao o banner pendurado `1x3` com ancoragem, sprites/estilos e `NearbyEffects` originais. Essa dependencia bloqueia o item jogavel.

### Evidencias de fonte

`tests/banners/native_registry.cpp` valida propriedade de mod, representante, colisao, rollback, unload e leitura durante troca de snapshots. `tests/banners/native_parser.cpp` executa o SourceParser original com as propriedades C# e recusa expressao dinamica insegura. `tests/banners/Program.cs` testa a API gerenciada, progresso por UID, migracao de tipo, normalizacao e transacoes.

O workflow `check-csharp-bannerloader-source-only.yml` compila o SDK, Roslyn, o ExampleMod C# completo, roda os testes gerenciados/C++ e verifica **a sintaxe do proprio SourceLoader.cpp**. Executa sem Gradle, sem Android e sem artefatos APK.

**CI de lote 2:** [39790448717](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39790448717), sucesso. **QA Android: nao executado; APK: 0**.

### Proximos gates tecnicos obrigatorios

1. Hooks reais do BannerSystem IL2CPP no ARM64, com original encadeado, preflight dos metodos e rollback seguro em caso de falha.
2. Redimensionamento transacional de `killCount`, `claimableBanners`, `newBanner`, `SceneMetrics.NPCBannerBuff` e `PlayerSceneMetrics.NPCBannerBuff` no mundo correto.
3. Contagem de kills, entregas a cada limite vanilla, refresh e efeitos de banners proximos; tile pendurado 1x3 com suas texturas, estilos, drops e ancoras.
4. Persistencia real no sidecar do mundo, isolamento multi-mod, envio de rede por UID, autoridade servidor e recuperacao de mods ausentes.
5. Testes automatizados de todos os pontos acima, verificacao de link nativo ARM64, Android QA separado e performance. **Apenas depois do codigo integral e verificacoes exigidas** considerar gerar um APK.

## Lote 3 — hooks, sidecar e cinco arrays (estado real)

- `ManagedBannerLookupHooks.h` implementa os detours das três assinaturas de `Terraria.GameContent.BannerSystem`, com a ABI IL2CPP (último argumento `MethodInfo*`), encadeamento para o original, verificação de assinatura antes da instalação e fallback completo para IDs vanilla. O gate `lookupsActive` começa **false**.
- `ManagedBannerWorldState.h` implementa um formato binário `BNR1`, endian little, CRC32, UID/NomeDaClasse, entradas limitadas a 4096 e dados limitados a 1 MiB. Decodificação é transacional. Converte contadores de variantes para o mesmo NPC representante, conserva entradas de mods ausentes e migra IDs entre sessões.
- `ManagedBannerArrayBridge.h` prepara as três tabelas estáticas do BannerSystem e os dois arrays de `SceneMetrics` em uma transação na thread do jogo: confere tipos, ranks, campos, offsets e tamanhos; aloca tudo antes de publicar; protege clones via GC handles e grava campos de instância pela barreira do coletor. Descarta a operação sem substituir qualquer slot caso o preflight/alocação falhe.
- `prepareBannerWorldArraysOnGameThread` é o ponto explícito para o futuro scheduler do mundo. A rotina adia a instalação de detours até a thread do jogo e mantém `lookupsActive = false` mesmo após sucesso, porque ainda falta a integração do save real, contador, refresh, rede e tile pendurado.
- Testes de host novos: `native_lookups.cpp`, `native_world_state.cpp`, `native_array_plan.cpp`. O workflow segue **somente código-fonte**; jamais Gradle/assembleDebug.

**Importante:** O codec de sidecar ainda não está ligado à gravação/leitura do arquivo de mundo; o preparador de arrays ainda não está ligado ao ciclo de mundo. Os três hooks nativos possuem código real de instalação, mas **continuam desativados no gameplay**. Esses são bloqueios obrigatórios — testes de host não significam QA Android.

O bloqueio de APK permanece absoluto até o contrato todo ficar completo, inclusive persistência real, rede, tile 1×3, contadores, efeitos e testes end-to-end.
## Lote 4 — persistencia real, restauracao nativa e fronteira de save/rede

O arquivo `ManagedBannerWorldFile.h` reutiliza a semantica do sidecar C# ja existente em `InvasionSaveFile.h`: grava um arquivo **.wld.banners.bl** ao lado do mundo (somente save local, nunca cloud), com limite de 1 MiB, bytes BNR1/CRC32, nome temporario exclusivo, `fsync`, `rename` atomico e permissao 0600. Nao segue symlinks de leitura, nao aceita diretorios, `..\` ou caminhos que nao terminem em `.wld`. Erros de integridade nao substituem os dados previamente validados em memoria. A gravacao preserva o sidecar anterior quando a codificacao falha.

`ManagedBannerWorldSession.h` mantem sessao por epoch de mundo: carrega uma vez na entrada, conserva os contadores de mods ausentes, recusa commit de cliente nao autoritativo e bloqueia a sobrescrita automatica quando encontra sidecar corrompido. Mudanca de mundo limpa path, epoch e cache.

`NativeInvasionWorldPath.cpp` foi reaproveitado **sem nova deteccao paralela**: `AutoSourceRuntime.cpp` ja chama `updateNativeInvasionWorldPath` uma vez por worldEpoch quando a sessao C# fica pronta. Nessa mesma funcao, `banners::worldSession().begin(...)` carrega o novo sidecar. Em saida, o cache e desligado.

`ManagedBannerGameCounters.h` le os tres arrays verdadeiros de `BannerSystem` com verificacao de classe, rank, tipo e limite; a captura usa `WorldLedger::capture`. A restauracao prepara antes os arrays, resolve `RefreshHasBannerAvailable()`, `RefreshAnyNewClaimableBanners()` e `bannerState`, aplica exclusivamente os IDs das bandeiras de mod, refaz os indicadores e reverte o trio de valores se um refresh nativo produzir excecao. Vanilla nao recebe gravacoes por esse processo.

`SourceLoader.cpp` oferece `restoreBannerWorldCountersOnGameThread(epoch)` e `saveBannerWorldCountersOnGameThread(epoch, authoritative)`. `AutoSourceRuntime.cpp` as agenda apenas para a sessao correta, sem polling caro de arquivos, com retry limitado pela cadencia da thread principal.

A separacao entre save e rede utiliza `ManagedBannerSaveDispatch.h` e `ManagedBannerSaveHooks.h`. O hook de `NetBannersModule.WriteFullState` guarda um marcador **thread_local**; o de `BannerSystem.Save` primeiro chama o metodo original e so depois agenda o sidecar quando nao esta dentro de um pacote de rede. A gravacao verdadeira e processada na thread do jogo, vinculada ao worldEpoch, sem IO pelo callback de rede. Os dois hooks possuem preflight de assinaturas e confirmacao de metodo estatico; nenhum dos dois e instalado antes da preparacao do BannerSystem.

**Gate de gameplay ainda fechado:** `lookupsActive` continua `false`; a restauracao no mundo e a gravacao local nao entram em producao ate a ativacao final. Isto evita chamar o refresh vanilla com contadores de mod enquanto faltam tile 1x3, efeitos de proximidade, entrega por abates, representacao e sincronizacao da rede com IDs por UID. O teste host nao substitui verificacao real da ABI arm64 da versao 1.4.5.8.6.

### Testes automatizados novos

- `native_world_disk.cpp`: dois mundos, save atomico, permissao, symlink, corrupcao e cloud.
- `native_world_session.cpp`: UID por mundo, troca de worldEpoch, cliente sem autoridade e bloqueio de sidecar danificado.
- `native_save_dispatch.cpp`: aninhamento de pacotes de rede, isolamento thread-local, descarte de pedidos de worldEpoch antigo e consumo unico.
- `check-csharp-bannerloader-source-only.yml`: compila `AutoSourceRuntime.cpp`, `SourceLoader.cpp` e `NativeInvasionWorldPath.cpp` na mesma checagem de sintaxe C++; executa todos os testes acima, o ExampleMod Roslyn e garante **nenhum APK**.

Nao realizar `assemble`, criar release nem abrir PR que acione geracao de APK antes de validar TODOS os gates restantes.

## Lote 5 — banner original 1x3, estilo e sprite preservados

- `BunnyTileShape.HangingBanner1x3 = 2` foi adicionado ao SDK. `TileWriter`, `SourceLoader` e `NativeTileGameplay` aceitam o novo tipo, com checagem de limites antes de publicar.
- `NativeContent.applyTileShape` configura `TileObjectData.newTile` a partir de `Style1x2Top`, altura 3, `CoordinateHeights = [16, 16, 16]`, estilos horizontais e `DrawYOffset = -2`. Os setters sao usados quando presentes; caso contrario, somente campos de instancia com contrato IL2CPP verificado por nome, tipo, largura e bounds sao escritos. Na ausencia dos contratos, o registro falha em vez de produzir um objeto quebrado.
- `ExampleEnemyBannerTile.cs` espelha as flags, cor de mapa, textura e formato 1x3 do ExampleModBunny. O PNG de `Content/Tiles/Banners/ExampleEnemyBannerTile.png` e **exatamente o blob original** (`9c9bcdf36687d25599d71cf05e741c3511fc8e2c`). O item `ExampleSlimeBanner` usa `DefaultToPlaceableTile("ExampleEnemyBannerTile", 1)`, como o estilo 1 do original.
- `TileWriter` preserva a chave de localizacao `MapObject.Banner`, permitindo o ponto do resource name mas recusando `..`.
- `NativeBasicItems.basicItemForTileStyle` consulta o item por chave e estilo explicitamente; `NativeTileGameplay` converte `frameX` em estilo horizontal com passo de 18 px. O tile 1x3 so faz drop do item do estilo exato quando o Terraria solicita drop de objeto grande. Evita a antiga consulta fixa no estilo 0, que perderia o ExampleSlimeBanner.
- O teste do ExampleMod compilado pelo Roslyn verifica a linha `TILE\tExampleEnemyBannerTile\t...` com `shape = 2` e os dois descritores `NPC_BANNER` vinculados ao NPC representante. `native_tile_style.cpp` cobre coordenadas, limites e flags de drop. O CI tambem compila sintaticamente `NativeContent.cpp`, `NativeTileGameplay.cpp` e `NativeBasicItems.cpp`.

**Ainda nao esta completo nem ativado no Android:** a alternativa de `AnchorTop` para plataformas, o `TileDrawing.DrawMultiTileVinesInWind`, os `NearbyEffects` nativos, a entrega de banners apos abates e a sincronizacao de rede precisam reproduzir as APIs do JS do Bunny e as regras do Terraria sem inferir assinaturas IL2CPP. Os testes C++ de host nao provam os offsets, setters ou assets no aparelho. `lookupsActive` continua false. Nao produzir APK.

## Lote 6 — proximidade nativa, SceneMetrics, Clear e BannerStrength (NO APK)

- `ManagedBannerNearbyPolicy.h`: cache thread-safe limitado a 512 candidatos, indexado por coordenada e ligado ao worldEpoch real. Expira amostras depois de 120 ticks, permite invalidar tiles removidos e zera ao sair/trocar de mundo. Não faz chamadas ao MonoVM durante a busca.
- `NativeTileGameplay.cpp`: somente os tiles C# declarados com `BunnyTileShape.HangingBanner1x3` entram na busca já existente de proximidade. O scanner amortizado roda a cada 6 ticks e lê o `frameX` para localizar o item do estilo exato (18 pixels por estilo). Só coleta candidatos quando `lookupsActive` está liberado. O scanner não é uma nova varredura global do mapa.
- `ManagedBannerMetrics.h`: hook real de `SceneMetrics.Reset()`, inicialmente passivo, com pré-validação e cache dos offsets/campos IL2CPP. Após o método original, candidatos de bandeira são revalidados pela localização atual, pelo tipo de tile, frameX, item registrado e mapeamento item→banner. Só marca `NPCBannerBuff[banner]` e `hasBanner` quando `ItemID.Sets.BannerStrength[item].Enabled` foi resolvido nativamente com contrato do campo e tipo exatos. Consultas repetidas de força são agrupadas por item em cada Reset.
- `ManagedBannerLookupHooks.h`: agora também contém hook para `BannerSystem.Clear()`: fecha imediatamente o gate e descarta a lista de tiles próximos, mas ainda chama o método vanilla original. Evita que dados de mundo anterior continuem em uso antes de reabrir a sessão; o registro estável por mod não é removido por Clear.
- `ManagedBannerStrength.h`: preparação transacional de `ItemID.Sets.BannerStrength` para itens de mod, com validação de todos os offsets antes de modificar qualquer byte. A política recusa IDs vanilla (`itemType < 6196`) e posições fora do array. Existe um ponto explícito `prepareBannerStrengthOnGameThread`; **nenhuma chamada de ativação é feita pelo runtime neste lote**.
- `tests/banners/native_nearby.cpp`, `native_strength_policy.cpp` e cobertura ampliada de `native_lookups.cpp` testam capacidade limitada, isolamento por mundo, expiração, política de item vanilla e Clear.
- A CI exclusivamente de fontes compila o `SourceLoader.cpp`, `NativeTileGameplay.cpp` e `AutoSourceRuntime.cpp` com as novas pontes; confirma que não foram gerados APKs.

### Gate final ainda bloqueado

O hook real de proximidade e o de `BannerSystem.Clear` estão disponíveis na camada nativa, mas os buffs do jogo só podem ser ativados quando `lookupsActive` for liberado junto de **todos** os contratos restantes. Faltam validar a ABI arm64 do Android para as structs `BannerStrength`/`SceneMetrics`, a ancoragem extra sobre plataformas, o desenho animado pelo vento, a integração completa da entrega de bandeiras no fluxo nativo e sincronização multiplayer por UID, com reentrada/save/restore testadas. Não promover a branch, não gerar APK nem afirmar paridade com o PC.

## Lote 7 — âncoras fiéis e proteção do Save vanilla (source-only)

O `ModBannerTile.js` original declara duas âncoras nativas: `SolidTile | SolidSide | SolidBottom | PlanterBox`, `DrawYOffset = -2`, e uma alternada de `PlatformNonHammered | AllFlatHeight` com `DrawYOffset = -10`, altura 3 e `CoordinateHeights = [16,16,16]`.

`ManagedBannerAnchors.h` configura `AnchorData` e `TileObjectData.newAlternate` diretamente pela API IL2CPP, com preflight de classe e assinatura; `NativeContent.applyTileShape` publica `addTile(type)` apenas depois da configuração. `ManagedBannerAnchorPolicy.h` concentra esses valores testados por `native_anchor_policy.cpp`. **A verificação de ABI arm64 em jogo continua pendente.** Um `AnchorType` ou método não resolvido rejeita o registro em vez de trocar o comportamento.

`ManagedBannerSaveSlice.h` prepara uma cópia temporária de `BannerSystem.killCount` e `claimableBanners` limitada a `BannerSystem.MaxBannerTypes`, fazendo a validação de todos os limites e protegendo os arrays originais e substitutos por GC handles. `ManagedBannerSaveHooks.h` executa a serialização .wld dentro desse escopo, restaurando referências na saída. `NetBannersModule.WriteFullState` continua em escopo thread-local separado e conserva os arrays estendidos. Se o recorte vanilla não puder ser preparado, o hook falha de forma explícita e desativa a entrada de mod, em vez de produzir silenciosamente um save incompatível. `native_save_slice.cpp` testa os limites do recorte.

A API `BunnyBannerTile : BunnyTile` foi incluída no `BunnySourceSdk.csproj`. `ExampleEnemyBannerTile` usa herança da classe-base e preserva o sprite do ExampleModBunny. `SourceParser.cpp` normaliza `BunnyBannerTile` para o tipo de registro `BunnyTile`; `CompiledAssemblyProbe` permite esse tipo de herança interna, e `native_parser.cpp` cobre o caso. Essa é uma exceção controlada para uma classe do próprio SDK; a exigência de declaração explícita para outras heranças de conteúdo permanece.

**Continua faltando:** renderização `TileDrawLoader.AddSpecialPoint(...,5)` / `DrawMultiTileVinesInWind` no passo nativo correto de desenho, verificação do ciclo de entrega por mortes, protocolo multiplayer por UID, testes de stress/concorrência reais e QA arm64. O gate `lookupsActive` permanece fechado e o workflow não chama Gradle ou gera APK.
