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
