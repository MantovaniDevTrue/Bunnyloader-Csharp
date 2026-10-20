# Example — grupo de loaders de mundo

Este fonte em desenvolvimento inicia o grupo Tile/Wall/Tree/TileEntity/GlobalTile. O APK de testes só será entregue quando os cinco contratos e seus exemplos estiverem completos. A presença de ExampleTile não significa que TreeLoader ou TileLoader inteiro estejam prontos.

ExampleTile.cs e ExampleTileItem.cs preservam o bloco original: textura, tamanho12×12 do item, receita de dez por ExampleItem, colisão sólida, bloqueio de luz, junção com terra, dust Stone e mapa RGB(0,200,255). DefaultToPlaceableTile usa o nome do conteúdo deste mod; o serviço nativo resolve o ID e a persistência por chave. Flags são aplicadas na thread de conteúdo depois da validação dos arrays IL2CPP, sem chamar JavaScript por quadro.

BunnyTileFlags.Lighted habilita processamento de luz e não emite uma cor por si só. Não confunda com BlockLight, que bloqueia luz. Callbacks de iluminação personalizada continuam pendentes no SDK; não existe uma API simulada neste exemplo.

Roteiro Android, a executar somente quando o grupo for disponibilizado:

1. Obter ExampleItem e fabricar dez Example Blocks pela receita original. A estação WorkBenches é requisito original da receita; o objeto testado é sempre o bloco do ExampleMod.
2. Colocar Example Blocks junto à terra, caminhar sobre eles e observar frames de junção, mapa e luz bloqueada. Comparar com o ExampleTile original.
3. Quebrar, conferir dust e recolher o mesmo item; repetir com uma pilha, sem duplicar drops.
4. Colocar novamente, salvar/sair e reabrir o mundo; conferir bloco, mapa e item recuperado.
5. Repetir com mods C#/JS habilitados juntos e ordem diferente de carregamento. Registrar versão, mod, ação, resultado e relatório.

Esses comportamentos ainda aguardam teste no Terraria Android nesta revisão. Testes de host validam contratos e registro/serialização; não medem iluminação, frames ou FPS no telefone. Os roteiros dos outros quatro exemplos serão incluídos junto das implementações completas, sem testes substitutos com itens vanilla.


## GrowSapling — callback parcial (não habilita árvores completas)

`BunnyTile.GrowSapling(x, y, underground, treeHeightAddon, ignoreWalls)` recebe a tentativa de crescimento para um tile C# com override. Retornar `false` bloqueia; retornar `true` solicita o `WorldGen.GrowTree` nativo e só retorna sucesso do lado do jogo se ele realmente crescer. O SDK oferece o transporte e o hook somente com ABI validada, mas não configura automaticamente `TileObjectData` 1×2, terreno, `TreeSapling` ou o trunk do `TreeLoader`. Ainda não criar mods C# de árvores como se a fidelidade do ExampleSapling estivesse concluída.


## Perfil `Sapling1x2`

`BunnyTileShape.Sapling1x2` registra um tile de 1x2 com frames 16/18, origem (0,1), âncora sólida inferior, sprite espelhado e três estilos por padrão. Defina `SaplingGroundTile = "ExampleTile"`, `SaplingVanillaGroundTile = 8`, `SaplingStyleRange = 3` e as seis flags do perfil (FrameImportant, NoAttach, LavaDeath, TreeSapling, CommonSapling, SwaysInWindBasic) no `SetStaticDefaults`. A origem do nome do bloco deve ser o **mesmo mod**. A descrição segue o ExampleSapling original do Bunny e não autoriza chamar código tModLoader nem JS no novo mod C#.

Esse formato de objeto **ainda não** torna o `ExampleTree` jogável em C#: falta registrar árvore, tronco e integrar `GrowTree` com a substituição temporária dos IDs. O teste da geometria no Android permanece necessário.


## Crescimento do `Sapling1x2` (etapa de troca nativa)

A tentativa de crescimento valida duas células e solo antes de chamar `WorldGen.GrowTree`. A muda é convertida **temporariamente** no tipo vanilla 20 por `TileData.SetType` (não por escrita direta no array internado); em falha, restaura as células. Um override `BunnyTile.GrowSapling` pode vetar; sem override, a autorização padrão é verdadeira. Ainda depende dos futuros hooks de terreno e do registro completo do `TreeLoader` para realmente produzir a árvore do ExampleModBunny no Android.
