# Bunny C# .81 — benchmark de WorldMapChunk real

## Referência de fonte

Em vez de inferir o mapa a partir de `TileLookup`, esta etapa usa o mesmo `Terraria.Map.WorldMap` e `WorldMapChunk` utilizados em `content/tiles/ModTileMap.cpp`. O bloco `MapTile` tem 4 bytes (`Type` uint16, `Light` byte, `Extra` byte). Um chunk tem 64×64 células, lidas da memória do jogo sem alterá-las; `Extra & 31` representa a cor da tinta.

O código localiza `WorldMap.GetChunkTile` e copia os dados do chunk sob seu próprio `Monitor`, seguindo o padrão de `ModTileMap.cpp`, sem manter um lock global enquanto o jogo salva. A API pública é **somente leitura** e independente de JavaScript/QuickJS. Ao solicitar um chunk por `GetChunkTile`, o Terraria pode carregar/descomprimir dados que ainda não estão em memória; isso faz parte do custo capturado.

## Teste Android

Instale o APK .81 em mundo singleplayer de testes. Primeiro libere o snapshot anterior com `Print(BunnyLoader.Source.BunnyTileScanBenchmark.Stop());` caso esteja retido.

No Editor C#:

```csharp
Print(BunnyLoader.Source.BunnyWorldMapBenchmark.Start());
```

Feche o Editor C#, deixe o jogo rodar, observe as quedas de FPS. Após alguns segundos:

```csharp
Print(BunnyLoader.Source.BunnyWorldMapBenchmark.Progress());
```

Quando informar captura concluída:

```csharp
Print(BunnyLoader.Source.BunnyWorldMapBenchmark.Finish());
```

Liberar memória:

```csharp
Print(BunnyLoader.Source.BunnyWorldMapBenchmark.Stop());
```

## Métricas e limites

- `captura` é o tempo real entre o primeiro e último frame, incluindo espera entre frames e possível carregamento/descompressão de chunks, com orçamento aproximado de 1,8 ms/frame.
- `CPU nativa` é a soma do tempo dedicado a obter e copiar dados dos chunks.
- `copia` é a transferência do snapshot para `uint[]` C#.
- `C#` é uma passada de interpretação dos quatro bytes por célula mais o hash FNV-1a; o aquecimento não entra na medida.
- `verificacao nativa` é o cálculo independente do checksum C++.
- `etapas` soma captura + cópia + varredura C#, excluindo os intervalos manuais no editor.
- `iluminadas`, `comTipo` e `comTinta` medem os campos do mapa de exploração, não os tiles físicos do mundo. Áreas ainda desconhecidas continuam escuras.

Esse é o teste real dos dados **visuais do WorldMap**; não é renderização do mapa na tela, upload GPU, nem uma repetição comprovadamente idêntica do caminho JavaScript citado como 3 segundos. Não revela tiles, não grava mapa e não altera a luz. Como `GetChunkTile` pode carregar dados, a captura pode gerar pressão de memória e quedas temporárias: acompanhe FPS, informe eventuais crashes e guarde o relatório. Não execute durante salvamento ou em multiplayer. O teste é experimental e não está validado em aparelho.
