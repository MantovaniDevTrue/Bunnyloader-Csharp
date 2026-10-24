# Integração árvore/Gore no ExampleMod C#

Base de integração: #160 (`fa16a81`). Referência de origem: #98
(`45fb0bcc6a1fa57e317b138f2e9973e90eb125e2`). Este lote não gera APK.

## Implementação

- `BunnyTree`, `BunnyGore`, muda 1×2, chance de crescimento 1/20,
  solo por identidade do mod, plantio por bolota, madeira/bolota e Shake.
- TREE de 12 campos, GORE de 5 e TILE de 29, com leitores dos formatos
  anteriores. Mantém os formatos de projétil/colisão da #160.
- Integração de callbacks/desenho de tiles que a #98 exige; os callbacks
  adicionais usam a fila MonoVM existente com índices até 28.
- Registro tardio TREE/GORE, contagem de classes e identidade separados de
  Dust, Rarity, Command e WorldSystem. Não reserva IDs de item para árvores.
- `ExampleTree`, `ExampleSapling`, `ExampleTree_Leaf` e seus cinco PNGs são
  os blobs originais da #98. Os testes comparam PNGs com ExampleModBunny.
- Arrays e assets pendentes ficam presos por handles de GC até publicação.
  Escrita de referência estática IL2CPP passa o objeto diretamente; leitura
  de volta confirma as tabelas. Erro restaura os campos já escritos. Se uma
  restauração não puder ser confirmada, os handles são conservados.
- O caminho C# exige os hooks de desenho/crescimento e os efeitos de Shake
  antes de publicar árvores. A rota JS mantém seu fallback anterior.

O teste no jogo usa ExampleTileItem para colocar solo e uma bolota vanilla
para plantar a muda. Não precisa de item, sprite ou comando artificial novo.
Roteiro completo: `samples/ExampleModCSharp/Playable/content/Documentation/ARVORE_GORE_ANDROID.md`.

## Verificação e limites

A suíte `tests/basic_items` compila o ExampleMod inteiro pelo Roslyn real e
verifica wire, assets originais, forma/âncora e espelhamento par/ímpar da muda.
`tests/world_loaders/run.sh` cobre políticas, filas, parser, registro, tabelas,
falhas de publicação e GC. O CI também compila/linka nativamente em ARM64,
sem executar Gradle assemble de APK.

Esses resultados não comprovam Mono/IL2CPP no aparelho. O APK de QA posterior
precisa confirmar crescimento/frequência, visual, folhas, opções de sangue,
Shake, drops, dois mundos/save/load, coexistência JS/C# e host/cliente.
O registro de árvores/Gore é do processo; este lote não promete hot-unload
individual completo ou reversão de terreno já alterado. Localização da entrada
Sapling no mapa continua com a diferença documentada no roteiro original.

Não declarar TreeLoader/GoreLoader completos com base apenas em CI.
