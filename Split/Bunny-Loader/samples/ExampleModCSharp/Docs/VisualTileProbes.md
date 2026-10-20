# Probes visuais com formas nativas 3×2 e 1×3 — Android

Os dois tiles C# usam PNGs originais do ExampleModBunny, sem redimensioná-los ou alterar seus pixels. A fogueira usa `BunnyTileShape.Style3x2`, com TileObjectData copiado do campfire vanilla. A luminária usa `BunnyTileShape.Style1x3`, baseada em `Style1xX`, com offset de desenho, espelhamento e frames originais. A ponte cria as formas no Terraria usando a declaração de C#, sem JS.

A geometria é equivalente à dos móveis originais. As formas declaram `FrameToggle=CampfireVertical` ou `LampHorizontal`, com alternância nativa por fio ou clique somente no single-player, validando todos os frames antes da alteração. Em multiplayer a alternância fica bloqueada até existir sincronização por `NetMessage`. Ainda é necessário validar no Android a escrita real dos frames, a persistência após salvar/carregar e o buff original da fogueira. O alvo deste lote é testar colocação, remoção integral, drops, transições ligado/desligado e partículas com a alteração de estado nativa.

Os itens `ExampleCampfireVisualProbeItem` e `ExampleLampVisualProbeItem` têm receitas com um `ExampleItem` e uma bancada vanilla (tile 18). Não são versões finais distribuíveis.

## Roteiro no Android

1. Somente com build experimental que contenha esta PR: entrar no mundo e criar os dois itens na bancada, colocar a fogueira 3×2 e a luminária 1×3 em chão sólido. Conferir frames, hitbox, posição e disponibilidade no inventário.
2. Verificar frame de chama da fogueira em cada uma das seis células, luz e velocidade do `Dust`; na luminária observar somente a célula superior emitindo chama especial, com espelhamento em coordenadas X pares.
3. Remover primeiro o bloco de suporte sob cada móvel, depois repetir quebrando a própria peça, observando remoção de todas as células, drop único e ausência de objetos fantasmas. Testar com um segundo móvel encostado para detectar remoção incorreta de vizinhos.
4. Testar quatro móveis simultâneos e depois doze; medir FPS, velocidade do desenho e quantidade de partículas e repetir com um mod JavaScript com tiles ativo.
5. Salvar, sair, recarregar, remover os móveis, entrar no menu e retornar; registrar capturas do jogo, FPS e logs, principalmente erros de `TileObjectData`, `TileFrameImportant`, `GetTileDrawData` e MonoVM.

O CI cobre política de layout 3×2/1×3, parsing, compilação C#, imagens originais, contratos nativos e regressões. Não executa Terraria Android nem valida layout real de IL2CPP, FPS ou fidelidade total. PR #98 permanece draft, sem APK/.bmod de release.
