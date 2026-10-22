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
  usa `BunnyContentHostDispatcher` na thread persistente do MonoVM: descobre loaders,
  chama `BunnyContentLoader.Load(uid, assembly)` e confirma apenas após `SetupContent`.
  Consumidores que embutem o SDK fora do launcher podem chamar o ciclo explicitamente.
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
mostra a mesma organização de categoria/loader do tModLoader. O
[mod-fonte de serviços](../Split/Bunny-Loader/samples/ContentLoaderCSharp/Playable/content/main.cs)
usa a descoberta automática do launcher e não precisa de bootstrap escrito pelo modder.
O executável de console mantém chamadas explícitas para demonstrar o contrato de embedding.

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

## Bootstrap automático e encerramento

O parser aceita categorias `ModType` sem reservar IDs nativos. O verificador Roslyn
conta separadamente serviços concretos e entidades legadas: ele não executa os hooks
`Load/Register/SetupContent` durante a compilação isolada. Uma assembly de serviços
produz somente o cabeçalho dos defaults; não precisa de uma entidade fictícia.

`AutoSourceRuntime` aquece o MonoVM para todos os mods-fonte e pede o carregamento
pela fila da thread persistente. A entrada no mundo exige confirmação de bootstrap,
`SetupContent` e aplicação dos defaults de cada mod, inclusive na rota de timeout
de assets. Uma falha posterior descarrega o escopo gerenciado recém-carregado.
Os tipos legados `BunnyNPC/BunnyItem/...` continuam no pipeline existente: o novo
bootstrap não os instancia uma segunda vez nem os converte em `ModType`.

A fila admite até 32 pedidos normais. Pedido ainda enfileirado que perdeu o prazo
não roda; uma carga nova que termina atrasada é desfeita na mesma thread antes do
próximo callback. Repetir uma carga confirmada com os mesmos bytes é idempotente.
Trocar os bytes ou caminho de uma assembly ativa exige reiniciar; também se recusa
substituição dos bytes de um caminho anteriormente carregado, pois `Assembly.LoadFrom`
não descarrega código do domínio. Unload remove escopos, aliases, singletons e loaders
sem outros donos; não é descarga da assembly CLR nem recarga de código a quente.

No encerramento normal da Activity, uma thread auxiliar solicita teardown ao dono
Mono, com espera de até dez segundos antes de encerrar o processo. O shutdown fecha
novas cargas, pausa callbacks legados e descarrega escopos em ordem inversa. Um
shutdown que perde o prazo permanece enfileirado; a Activity ainda pode matar o
processo antes de um hook travado terminar. Morte abrupta pelo sistema não garante
callbacks. Sair de um mundo preserva os mods carregados, como no tModLoader.

## Validação e limite de entrega

```sh
dotnet run --project Split/Bunny-Loader/tests/content_loader -c Release
dotnet run --project Split/Bunny-Loader/samples/ContentLoaderCSharp -c Release
python3 Split/Bunny-Loader/tests/content_loader/check_upstream.py
python3 Split/Bunny-Loader/tests/content_loader/run_native.py
```

A suíte compila o SDK inteiro e o exemplo. Além dos testes de integração/falhas,
executa os arquivos oficiais `Loader.cs`, `ModTypeLookup.cs`, `ContentInstance.cs`,
`IModType.cs` e `LegacyNameAttribute.cs` em um host de teste, e compara ID, limites,
lookups, aliases, singletons e reset com Bunny. São os bytes oficiais pinados,
com apenas `#nullable disable` acrescentado para compilar no harness. O verificador
confere o hash Git de cada fonte após remover essa linha. Os shims de dono,
localização e TypeCaching ficam exclusivamente nos testes.

O workflow `check-csharp-content-loader.yml` testa .NET 8 e 10, executa o exemplo,
confere a referência pública, testa a fila/parser/SourceLoader/gate de produção
com doubles nativos e compila entrypoints contra JNI real. O NDK compila apenas
objetos ARM64 das fontes de integração; não há link, Gradle, assinatura ou pacote.
O workflow recusa a presença de APK. Os testes nativos não substituem Mono/IL2CPP
reais no aparelho; o encerramento Kotlin ainda requer validação Android.

**Estado de entrega: núcleo gerenciado testável; PR Draft para integração/revisão.**
A ligação automática com MonoVM/SourceLoader e o pedido de teardown estão
implementados nas fontes, com testes de host. A validação integrada no Android
ainda não foi realizada; as categorias de gameplay existentes não foram migradas. A matriz permanece sem qualquer
novo `androidVerified`. **Não gerar APK, não mesclar como loader Android concluído,
não publicar `.bmod` nem release enquanto a integração e a validação estiverem pendentes.**
