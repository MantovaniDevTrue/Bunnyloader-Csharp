# Example — grupo de loaders de mundo

Este fonte em desenvolvimento inicia o grupo Tile/Wall/Tree/TileEntity/GlobalTile. O APK de testes só será entregue quando os cinco contratos e seus exemplos estiverem completos. A presença de ExampleTile não significa que TreeLoader ou TileLoader inteiro estejam prontos.

ExampleTile.cs e ExampleTileItem.cs preservam o bloco original: textura, tamanho12×12 do item, receita de dez por ExampleItem, colisão sólida, bloqueio de luz, junção com terra, dust Stone e mapa RGB(0,200,255). DefaultToPlaceableTile usa o nome do conteúdo deste mod; o serviço nativo resolve o ID e a persistência por chave. Flags são aplicadas na thread de conteúdo depois da validação dos arrays IL2CPP, sem chamar JavaScript por quadro.

BunnyTileFlags.Lighted habilita processamento de luz e não emite uma cor por si só. Não confunda com BlockLight, que bloqueia luz. Os callbacks `ModifyLight` e `NearbyEffects` já possuem ponte C#/nativa limitada, mas a assinatura IL2CPP e os resultados de iluminação ainda exigem validação no Android.

Roteiro Android, a executar somente quando o grupo for disponibilizado:

1. Obter ExampleItem e fabricar dez Example Blocks pela receita original. A estação WorkBenches é requisito original da receita; o objeto testado é sempre o bloco do ExampleMod.
2. Colocar Example Blocks junto à terra, caminhar sobre eles e observar frames de junção, mapa e luz bloqueada. Comparar com o ExampleTile original.
3. Quebrar, conferir dust e recolher o mesmo item; repetir com uma pilha, sem duplicar drops.
4. Colocar novamente, salvar/sair e reabrir o mundo; conferir bloco, mapa e item recuperado.
5. Repetir com mods C#/JS habilitados juntos e ordem diferente de carregamento. Registrar versão, mod, ação, resultado e relatório.

Esses comportamentos ainda aguardam teste no Terraria Android nesta revisão. Testes de host validam contratos e registro/serialização; não medem iluminação, frames ou FPS no telefone. Os roteiros dos outros quatro exemplos serão incluídos junto das implementações completas, sem testes substitutos com itens vanilla.


## GrowSapling — crescimento nativo em validação

`BunnyTile.GrowSapling(x, y, underground, treeHeightAddon, ignoreWalls)` recebe a tentativa de crescimento para um tile C# com override. Retornar `false` bloqueia; retornar `true` solicita o `WorldGen.GrowTree` nativo e só retorna sucesso do lado do jogo se ele realmente crescer. O SDK integra esse callback ao registro nativo da muda e ao `BunnyTree`; a presença da ponte e dos testes de host não substitui testes de crescimento, efeitos e persistência no Terraria Android.


## Perfil `Sapling1x2`

`BunnyTileShape.Sapling1x2` registra um tile de 1x2 com frames 16/18, origem (0,1), âncora sólida inferior, sprite espelhado e três estilos por padrão. Defina `SaplingGroundTile = "ExampleTile"`, `SaplingVanillaGroundTile = 8`, `SaplingStyleRange = 3` e as seis flags do perfil (FrameImportant, NoAttach, LavaDeath, TreeSapling, CommonSapling, SwaysInWindBasic) no `SetStaticDefaults`. A origem do nome do bloco deve ser o **mesmo mod**. A descrição segue o ExampleSapling original do Bunny e não autoriza chamar código tModLoader nem JS no novo mod C#.

Esse formato participa do registro C# de `ExampleSapling` e `ExampleTree`, com substituição temporária controlada na tentativa de crescimento. Ainda é necessário testar a geometria, o crescimento real e a restauração após falhas no Android.


## Crescimento do `Sapling1x2` (etapa de troca nativa)

A tentativa de crescimento valida duas células e solo antes de chamar `WorldGen.GrowTree`. A muda é convertida **temporariamente** no tipo vanilla 20 por `TileData.SetType` (não por escrita direta no array internado); em falha, restaura as células. Um override `BunnyTile.GrowSapling` pode vetar; sem override, a autorização padrão é verdadeira. A ponte de terreno e o registro de troncos/copas/galhos já existem; a comprovação de crescimento real, save/reload e compatibilidade com árvores vanilla continua pendente.


## Terreno de árvore — hooks nativos condicionais

O terreno definido por `SaplingGroundTile` agora pode ser reconhecido como válido por `WorldGen.IsTileTypeFitForTree` e `TreeTypes.Custom=13` pelo `WorldGen.GetTreeType`. O comportamento original permanece para terrenos não registrados. `BunnyTree` agora registra texturas de tronco, copa e galho, terreno, madeira, bolotas e efeitos de folhas. `BunnyGore` registra PNGs explícitos por classe C# e permite resolver `LeafGoreName`; o exemplo `ExampleTree_Leaf` usa `SpecialAI=3` e `SafeGore=true`. O protocolo `TREE` de 9, 11 e 12 campos preserva `LeafGoreType` e `DustType`, inclusive com item personalizado de Shake. O registro e os testes de host não certificam os hooks IL2CPP, os efeitos ou a jogabilidade no Android.

Consulte `ARVORE_GORE_ANDROID.md` para o roteiro real de crescimento, folhas, poeira, Shake, drops, multiplayer, coexistência C#/JS, persistência e FPS. A PR #98 permanece em draft; os cinco loaders do grupo ainda não estão certificados e não se deve publicar APK/.bmod.
