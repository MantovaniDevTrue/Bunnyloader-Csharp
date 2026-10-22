# Núcleo de loaders C# baseado no tModLoader

Base oficial **tModLoader 1.4.5**, commit `fcafebc0b792b90e110bac7c1fc67cac81cb60ed`.
O objetivo do projeto é portar todos os loaders; [inventário completo dos arquivos oficiais](TMODLOADER-LOADERS.md).
Este lote trata do **núcleo gerenciado de Loader.cs**, não declara os loaders de gameplay completos.
Busca nas PRs dos dois repositórios em 22/10/2026 por `LoaderManager`, `ModTypeLookup` e
`ContentInstance` não encontrou outro lote C# aberto dessas classes. ModSystem, Hair,
DamageClass, Cloud/Biome, Dust, Gore e os demais lotes existentes ficam nas suas PRs.

## Classes oficiais e adaptação

| Fonte oficial | Implementação Bunny |
| --- | --- |
| `Loader.cs`: `Loader`, `Loader<T>`, `ILoader`, `LoaderManager` | [Loader.cs](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/Loader.cs) |
| `ModTypeLookup.cs`: dicionário por tipo, nome completo e dicionário por mod | [ModTypeLookup.cs](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/ModTypeLookup.cs) |
| `ContentInstance.cs`: singleton exato e lista quando existe mais de uma instância | `ContentInstance<T>` no [ciclo gerenciado](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/BunnyContentLoader.cs) |
| `ModType`, `IModType`, `AutoloadAttribute`, `LegacyNameAttribute` | Identidade, hooks básicos e atributos no mesmo ciclo gerenciado |

Preservados: `VanillaCount`, reserva sequencial, `Get` fora da faixa retorna null,
registro genérico independente por categoria, nome `Mod/Classe`, aliases, nomes
case-sensitive, singleton por tipo CLR exato, autoload por atributo herdado/lado,
exclusão de classes abstratas/genéricas abertas/sem construtor padrão, e ordem
`Load → Register → ResizeArrays → SetupContent`. Como no original, a categoria
sobrescreve `SetupContent` para chamar `SetStaticDefaults`.

São APIs no namespace `BunnyLoader.Source`; a identidade do dono usa
`BunnyContentMod` em vez do `Terraria.ModLoader.Mod` do PC. Não existe promessa de
compatibilidade binária com assemblies tModLoader. O registro genérico não atribui
IDs reais de Item/NPC/Tile nem escreve tabelas IL2CPP.

Diferenças deliberadas, para revisão:

- Entradas de lifecycle antes internas são públicas para o host Bunny. O bootstrap
  chama `BunnyContentLoader.Load(uid, assembly)` explicitamente e deve chamar
  `Unload(uid)` ao desativar o mod. Este lote não instala um hook de startup Android.
- `LoaderManager.Get<T>` mantém a instância criada antes de AutoLoad; no original
  esse caso retorna uma instância temporária. Isso evita perder registros de um
  loader gerenciado criado por uma categoria customizada.
- Aliases são validados em lote antes de escrever. Falhas não deixam metade dos
  aliases registrada, e uma falha de nome não consome um ID no loader genérico.
- Unload individual deixa buracos nos IDs gerenciados; os IDs dos outros mods não
  mudam. `LoaderManager.Unload/Clear`, após descarregar os conteúdos, reinicia a
  contagem. Reinicializar/resetar um loader com conteúdo vivo é recusado.
- `ContentInstance<T>` consulta o catálogo central, sem estático genérico guardar
  objetos de mods descarregados. `Instances` é um snapshot imutável em cache.
  O registro é feito pelo ciclo do mod, não pela API avulsa `ContentInstance.Register(object)` do PC.
- Identidades aceitam 1–160 caracteres ASCII alfanuméricos, `_`, `-`, `.` por segmento.
  O nome de classe/identidade deve permanecer estável durante sua carga.
- O ciclo foi adaptado para rollback por mod e agregação de erros no unload; hooks
  de terceiros continuam responsáveis por desfazer seus próprios recursos externos.
  Nem o rollback nem o unload conseguem desfazer efeitos arbitrários de um construtor.

`ModType` aqui implementa a base de dados/serviços gerenciados. Os genéricos de
clonagem de entidades do tModLoader e a migração das bases `BunnyItem/BunnyNPC/...`
existentes não fazem parte deste núcleo. Permanecem requisitos de seus respectivos
loaders de gameplay; não são tratados como concluídos por esta PR.

## Uso e lifecycle

O [exemplo executável comentado](../Split/Bunny-Loader/samples/ContentLoaderCSharp/ExampleDefinitions.cs)
mostra a mesma organização de categoria/loader do tModLoader. O bootstrap precisa
existir no host/consumidor que integra esta API; o launcher atual não descobre
automaticamente uma assembly contendo apenas os novos `ModType`.

1. Definir `ExecutionSide` (`Client` ou `Server`) antes de carregar loaders/conteúdo.
2. `LoaderManager.AutoLoad(assembly)` encontra loaders concretos habilitados, ou
   `LoaderManager.Get<T>()` cria um explicitamente. Descoberta falhada não publica
   parte dos loaders construídos.
3. `BunnyContentLoader.Load(uid, assembly, configure)` descobre `ModType`, executa
   `Load/Register`, aceita adições manuais em `configure`, fecha o registro, redimensiona
   arrays e executa `SetupContent`. Só então retorna o escopo com `IsLoaded=true`.
4. `ModContent.Find<Categoria>(uid, nome)`, `TryFind`, `GetInstance<T>` e `GetContent<T>`
   consultam as instâncias reais. Não criar um objeto novo para simular um lookup.
5. `Unload(uid)` executa callbacks em ordem inversa e remove aliases, singletons e
   posições do loader daquele mod. `UnloadAll()` preserva ordem inversa de carga.
6. Após unload de tudo, `LoaderManager.Clear()` libera referências dos loaders.

Os callbacks são síncronos e serializados; consultas reentrantes na mesma thread
são permitidas. `Load` pode adicionar conteúdo ao próprio mod. Carregar/descarregar
outro escopo de dentro de um callback é recusado. Não bloquear aguardando outra
thread consultar o catálogo enquanto um callback está executando. Os snapshots
retornados e referências que o próprio mod guardar continuam sob responsabilidade
do chamador; não reutilizar instâncias/IDs depois de unload.

Dependências entre mods são carregadas na ordem definida pelo host. `SetupContent`
garante que todos os conteúdos **do mod atual** foram registrados, não que um mod
futuro já tenha sido carregado. Use `TryFind` para dependências opcionais.

## Validação e limite de entrega

```sh
dotnet run --project Split/Bunny-Loader/tests/content_loader -c Release
dotnet run --project Split/Bunny-Loader/samples/ContentLoaderCSharp -c Release
python3 Split/Bunny-Loader/tests/content_loader/check_upstream.py
```

A suíte compila o SDK inteiro e o exemplo. Além dos testes de integração/falhas,
executa os arquivos oficiais `Loader.cs`, `ModTypeLookup.cs`, `ContentInstance.cs`,
`IModType.cs` e `LegacyNameAttribute.cs` em um host de teste, e compara ID, limites,
lookups, aliases, singletons e reset com Bunny. São os bytes oficiais pinados,
com apenas `#nullable disable` acrescentado para compilar no harness. O verificador
confere o hash Git de cada fonte após remover essa linha. Os shims de dono,
localização e TypeCaching ficam exclusivamente nos testes.

O workflow `check-csharp-content-loader.yml` testa .NET 8 e 10, executa o exemplo,
confere a referência pública e recusa a presença de APK. Não chama Gradle/NDK,
não assina pacotes e não publica builds.

**Estado de entrega: núcleo gerenciado testável; PR Draft para integração/revisão.**
A ligação automática deste núcleo com o lifecycle do MonoVM/SourceLoader e sua
validação no Android ainda não estão realizadas. A matriz permanece sem qualquer
novo `androidVerified`. **Não gerar APK, não mesclar como loader Android concluído,
não publicar `.bmod` nem release enquanto a integração e a validação estiverem pendentes.**
