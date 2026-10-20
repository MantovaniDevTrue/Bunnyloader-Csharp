# BunnyTree e BunnyGore — roteiro Android

Exemplo C# baseado exclusivamente nos arquivos originais do ExampleModBunny: `ExampleTree.js`, `ExampleSapling.js`, `GoreLoader.js` e `ModGore.js`. Todas as cinco texturas PNG foram reutilizadas sem alterações. O solo continua sendo `Content/Tiles/ExampleTile.cs` e os itens `ExampleItem` e `ExampleTileItem` já existentes.

## Experimentos

1. Em um mundo descartável, coloque `ExampleTile` e plante uma bolota. A muda deve usar os sprites `ExampleSapling`, não os sprites vanilla.
2. Aguarde crescimento nativo; confira o tronco, a copa e os galhos, as transições de mundo e a persistência ao salvar/reentrar.
3. Corte a árvore e confira `ExampleItem` substituindo madeira e as bolotas extras da copa.
4. Sacuda uma árvore intacta uma vez: `ExampleTileItem`, efeitos da folha, ausência de duplicação ao sacudir novamente; teste árvores vanilla em paralelo.
5. Verifique `ExampleTree_Leaf`: PNG original, `SpecialAI=3`, `SafeGore=true`; teste com os efeitos visuais e a opção de sangue desativada.
6. Repita com ExampleMod JS ativo, outro mod C#, multiplayer host/cliente e pós-reload; compare FPS e memória antes/depois.

## Limites

`SaplingVanillaGroundTile=8` representa `TileID.Gold` do ExampleSapling original. `AdjacentTile=20` corresponde ao `TileID.Saplings` do mesmo exemplo. O `BunnyTree` C# usa nomes locais resolvidos durante o carregamento; seus IDs não são fixos.

A entrada do mapa da muda usa `Sapling` em vez da chave localizada `MapObject.Sapling` do JS, pois a API `BunnyTile.AddMapEntry` aceita apenas nomes simples. A cor original (200,200,200) foi preservada. Essa diferença de localização continua pendente.

O `ExampleSapling.js` original possui uma verificação `Rand.NextBool(20)` no `RandomUpdate`; a frequência efetiva do crescimento e a passagem automática ao hook nativo precisam de confirmação no Terraria Android. A presença das classes e testes da CI não valida esse comportamento. Não publicar o mod/Loader em APK, não mesclar a PR #98 e não declarar `TreeLoader` completo antes do teste de jogo.
