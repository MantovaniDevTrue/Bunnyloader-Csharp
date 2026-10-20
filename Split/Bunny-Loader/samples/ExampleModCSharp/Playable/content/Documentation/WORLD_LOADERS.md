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
