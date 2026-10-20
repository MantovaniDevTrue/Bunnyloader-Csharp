# Probes visuais com formas nativas 3×2 e 1×3 — Android

Os dois tiles C# usam PNGs originais do ExampleModBunny, sem redimensioná-los ou alterar seus pixels. A fogueira usa `BunnyTileShape.Style3x2`, com TileObjectData copiado do campfire vanilla. A luminária usa `BunnyTileShape.Style1x3`, baseada em `Style1xX`, com offset de desenho, espelhamento e frames originais. A ponte cria as formas no Terraria usando a declaração de C#, sem JS.

A fogueira usa os 8 quadros a cada 4 ticks do exemplo original. A animação avança em `Main.AnimateTiles`, e seu índice é publicado em snapshot atômico para o desenho, sem consultar `Main` por célula. O transporte de desenho usa 13 números e mantém os formatos anteriores com 11 ou 12 números. A chama lê o quadro nativo no C# e não usa relógio independente. O quadro apagado (frameY 36/54) agora usa offset 252, o quadro 7 estático do original; o quadro aceso recebe o índice 0..7 multiplicado por 36. Essa política é aplicada apenas às fogueiras 3×2 de oito quadros e altura 36, após a resposta C# e antes do desenho nativo, mantendo fallback se o snapshot não tiver quadro válido. A sincronização visual ainda precisa de validação real no aparelho.

A geometria é equivalente à dos móveis originais. As formas declaram `FrameToggle=CampfireVertical` ou `LampHorizontal`. Preservando o ExampleModBunny original, a fogueira alterna por clique e fio; a luminária alterna apenas por fio. A execução permanece limitada ao single-player e valida todos os frames antes da alteração. Em multiplayer a alternância fica bloqueada até existir sincronização por `NetMessage`. Ainda é necessário validar no Android a escrita real dos frames, a persistência após salvar/carregar e o buff original da fogueira. O alvo deste lote é testar colocação, remoção integral, drops, transições ligado/desligado e partículas com a alteração de estado nativa.

Os itens `ExampleCampfireVisualProbeItem` e `ExampleLampVisualProbeItem` têm receitas com um `ExampleItem` e uma bancada vanilla (tile 18). Não são versões finais distribuíveis.

## Roteiro no Android

1. Somente com build experimental que contenha esta PR: entrar no mundo e criar os dois itens na bancada, colocar a fogueira 3×2 e a luminária 1×3 em chão sólido. Conferir frames, hitbox, posição e disponibilidade no inventário.
2. Verificar frame de chama da fogueira em cada uma das seis células, luz e velocidade do `Dust`; na luminária observar somente a célula superior emitindo chama especial, com espelhamento em coordenadas X pares.
3. Remover primeiro o bloco de suporte sob cada móvel, depois repetir quebrando a própria peça, observando remoção de todas as células, drop único e ausência de objetos fantasmas. Testar com um segundo móvel encostado para detectar remoção incorreta de vizinhos.
4. Testar quatro móveis simultâneos e depois doze; medir FPS, velocidade do desenho e quantidade de partículas e repetir com um mod JavaScript com tiles ativo.
5. Salvar, sair, recarregar, remover os móveis, entrar no menu e retornar; registrar capturas do jogo, FPS e logs, principalmente erros de `TileObjectData`, `TileFrameImportant`, `GetTileDrawData` e MonoVM.

O CI cobre política de layout 3×2/1×3, parsing, compilação C#, imagens originais, contratos nativos e regressões. Não executa Terraria Android nem valida layout real de IL2CPP, FPS ou fidelidade total. PR #98 permanece draft, sem APK/.bmod de release.


## Buff nativo da fogueira

A declaração `BunnyTileFlags.Campfire` publica `TileID.Sets.Campfires`, `InteractableByNPCs`, `Wiring.IsAMechanism` e `Wiring.IgnoreWhenValidatingTraps` por escrita conjunta validada em arrays `Boolean[]`. O indicador ocupa o bit 65536 sem acrescentar campos ao wire TILE de 22 colunas; o compilador rejeita bits acima de 131071.

O hook nativo `SceneMetrics.Scan` executa a função original antes de consultar a área centrada em `BiomeScanCenterPositionInWorld`, usando `Main.buffScanAreaWidth` e `Main.buffScanAreaHeight` com limite de 256 células por dimensão. Para os tiles C# marcados como fogueira, o frame Y de 0 ou 18 ativa `HasCampfire` na instância de `SceneMetrics`; 36 ou 54 não ativam. A varredura é nativa e encerra no primeiro tile válido. A assinatura, o layout e os tipos dos campos IL2CPP são exigidos antes da instalação; um contrato incompatível recusa o registro em vez de executar com endereço presumido. Não chama JavaScript.

Esse código ainda exige teste no Android real para confirmar compatibilidade ABI, aplicação e remoção do buff, funcionamento com fogueiras JavaScript simultâneas e comportamento em multiplayer. O som `SoundID.Mech` ainda não foi integrado ao clique C#. Nenhum desses resultados pode ser inferido somente pelo CI.

## Áudio de clique

A fogueira 3×2 C# usa o `SoundID.Mech` original através de `SoundEngine.PlaySound(LegacySoundStyle,Vector2,float,float)`. O host confere a assinatura nativa, `SoundID.Mech` e o layout de `Vector2` no registro, e invoca o som somente após o clique completar a alternância. Fios não tocam o som. O runtime não consulta QuickJS. A emissão de áudio real e os diagnósticos de ABI ainda requerem teste no Android; incompatibilidade rejeita o registro em vez de inventar um ID de som.

## Layout e tamanho da área de varredura

Os limites são os mesmos do `ExampleModBunny` original: `centro - floor(tamanho/2)` até `centro + floor(tamanho/2)` (limite superior exclusivo). Para tamanho par, isso examina exatamente as células pedidas; para ímpar, preserva a fórmula do fonte original. Em `SceneMetricsScanSettings` struct, o host exige tamanho acima de 16 bytes e usa `il2cpp::resolveValueLayout` para converter offsets boxed/unboxed; em classe usa `nativeFieldContract`. Um layout ambíguo interrompe a instalação do hook. Não são inferidos offsets pelo tamanho do cabeçalho.

## Smart-interact nativo

O fallback de `ModTile.HasSmartInteract` da referência Bunny adiciona os tiles com `RightClick` sobrescrito à seleção inteligente. O núcleo C# agora tenta vincular `TileSmartInteractCandidateProvider.FillPotentialTargetTiles` com assinatura e campos validados, aceita candidatos com `RightClick` C# ou fogueira com clique nativo e registra alvos apenas uma vez (`JoinValue`/`Contains`/`Add`). A área LX/LY/HX/HY é inclusiva, como `bl.tiles.find`; o scanner é limitado a 64×64 células. A rotina original continua sendo executada primeiro. Falhas de ABI desativam somente o aprimoramento do smart-interact e preservam o cursor móvel anterior. Overrides arbitrários de `HasSmartInteract` ainda não estão implementados; falta verificar a ABI com Terraria Android e a combinação com outros mods.

## HasSmartInteract C# dinâmico

O contrato `ModTile.HasSmartInteract(i,j,settings)` agora aceita um override C# tipado `HasSmartInteract(int,int,BunnyTileSmartInteractSettings)` com os quatro limites LX,LY,HX,HY. O índice 20 do dispatcher só aceita um snapshot de seis inteiros e valida a geometria antes do callback. O núcleo executa a rotina original, mantém a inclusão rápida de `RightClick` e permite até 8 avaliações gerenciadas por varredura com prazo individual de 450 µs. Em callback indisponível, erro ou estouro do orçamento, não cria alvo novo. O bit 20 amplia a máscara para 21 callbacks; os bits antigos e os wires de desenho continuam válidos. A sobrecarga `AnimateIndividualTile(type,...)` também recebe corretamente o ID real com o snapshot de 13 campos. Permanecem pendentes demais membros de `SmartInteractScanSettings` não transportados, a validação ABI real no Android e callbacks não implementados de `ModTile.js`.

## Hook TileFrame

`BunnyTile.TileFrame(int x,int y,ref bool resetFrame,ref bool noBreak)` oferece cancelamento do enquadramento (`false`) e alteração das duas flags, como no `TileLoader.js` original. `WorldGen.TileFrame` é interceptado só para os tiles C# com override e se o worker estiver pronto. A resposta `FRAME|executar|reset|noBreak` exige booleanos válidos. Em falha, fila ocupada ou timeout de 650 µs, o Terraria executa o framing original. `TileFrameImportant` e a limpeza de móveis permanecem nativos. Testes de ABI e gameplay no Android continuam pendentes.
