# BiomeConversionLoader C#

Categoria concreta portada de tModLoader **1.4.5**, commit
`fcafebc0b792b90e110bac7c1fc67cac81cb60ed`, arquivos `BiomeConversionLoader.cs`
e `ModBiomeConversion.cs`. [Fontes oficiais pinadas nos testes](../Split/Bunny-Loader/tests/biome_conversion/upstream).
Busca de PRs abertas nos dois repositórios e de branches `conversion` em 22/10/2026
não encontrou outro lote da categoria. A implementação depende do núcleo da PR #138.

## Contrato original preservado

`ModBiomeConversion.Type`, registro/lookup da categoria, reserva após as 22 conversões
vanilla, `GetBiomeConversion`, `SetStaticDefaults`, `PostSetupContent` e teardown.
`Register` usa o registro genérico existente, com aliases e rollback. Os IDs são
identidades de conversão; não são IDs de tile, parede ou bioma. Unload individual
mantém buracos para preservar os IDs dos demais mods; shutdown global reinicia em 22.

No original, **ModBiomeConversion não possui hooks de execução por tile**:
`TileLoader` e `WallLoader` possuem esses hooks. Este lote não declara esses dois
loaders concluídos. Não adiciona propagação de infecção, worldgen, um bioma ou
substituição dos tipos vanilla de solução. Não há métodos vazios simulando isso.

## Conversão nativa utilizável

A extensão Bunny permite `RegisterTileConversion(source, target)` e
`RegisterWallConversion(source, target)` durante `PostSetupContent`, seguida por
`conversion.Convert(i, j, size, tiles, walls)`. O código de substituição é executado
pelo jogo na sua thread de atualização; não depende de JS nem chama Mono por tile.

`PostSetupContent` roda após todos os `SetupContent` **do mod atual**. Dependências
entre mods devem respeitar a ordem de carga do host; não há promessa de acesso a
um mod que ainda não carregou. As regras são congeladas antes da publicação nativa.
Publicação inválida ou exceção no post-setup desfazem o escopo sem afetar outros donos.

- `i/j`: coordenadas em tiles (pixel/16). Raio entre 0 e 8; raio zero afeta uma posição.
- A área é um quadrado recortado por distância Manhattan, como o pincel de conversão.
- Somente tiles ativos e paredes existentes são fontes. Parede zero pode ser destino
  para remover uma parede; não pode ser fonte para criar paredes no ar.
- Até 4096 regras por mod, 64 pedidos pendentes e quatro pedidos por update.
- `true` significa admissão na fila. Não é confirmação de aplicação; unload/troca
  de mundo cancelam pedidos pendentes. Fila cheia retorna `false`, permitindo retry.
- Pedidos pertencem ao UID e ao tipo de conversão; outro mod não pode usar esse tipo.
- Apenas single-player. Entrar em menu ou multiplayer descarta o trabalho pendente.
- Destinos precisam caber nas tabelas reais de Tile/Wall no instante da aplicação.
  Não converta blocos em objetos de múltiplos tiles: uma regra de troca de tipo não
  instala os frames/entidades auxiliares desses objetos.

O adaptador verifica `Terraria.Tile` de quatro bytes, assinaturas completas dos
setters `UInt16`, métodos estáticos de enquadramento e tipos dos campos. A entrada
no mundo aguarda essa ligação para mods da categoria. Métodos indisponíveis geram
um erro de ABI e bloqueiam o gate; não se fazem escritas por offsets supostos.

**Mobile interna tiles compartilhados.** O adaptador chama `Tile.set_type`/
`Tile.set_wall`, preservando copy-on-write, em vez de escrever `TileType[TileLookup]`.
Depois chama `SquareTileFrame`/`SquareWallFrame`. O save normal do jogo preserva
as alterações de terreno vanilla; unload da conversão não desfaz o terreno já aplicado.

A API de execução aqui é uma extensão declarativa Bunny. Não instala um hook em
`WorldGen.Convert`, nem implementa callbacks arbitrários de `TileLoader.Convert`/
`WallLoader.Convert`; consumidores C# solicitam a execução por `ModBiomeConversion.Convert`.
Compatibilidade binária com mods PC e multiplayer não são prometidas.

## Exemplo e validação

[Exemplo comentado com conversão, arma e projétil](../Split/Bunny-Loader/samples/BiomeConversionCSharp/Playable/content/main.cs).
O projétil solicita Dirt → Stone e DirtUnsafe Wall → Stone Wall depois de avançar.
A amostra usa as bases de arma/projétil existentes; obter a arma requer a ferramenta
usual de criação de itens do host/ambiente de testes, pois `BunnyWeapon` ainda não
possui API de receita neste checkpoint. Não introduz receita fictícia.

```sh
python3 Split/Bunny-Loader/tests/biome_conversion/run.py
python3 Split/Bunny-Loader/tests/content_loader/run_native.py
```

O runner confere os hashes das duas fontes oficiais e executa seus algoritmos num
host de teste. Compila o exemplo e um mod conversion-only com Roslyn, exercita
bootstrap, P/Invoke real, engine C++, cancelamento, bounds, falhas e rollback.
O adaptador IL2CPP de produção é testado com metadata e tiles internados simulados,
incluindo setters, enquadramento, ABI inválido, menu e multiplayer.

O CI compila SDK/compilador em .NET 8/10 e gera apenas objetos NDK ARM64.
**Não gera APK.** Este é um loader concreto implementado em fonte, com contrato
adaptado explicitamente acima; testes de host não equivalem a QA no aparelho.
A PR fica Draft para revisão e validação, sem merge ou publicação de builds.
