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
