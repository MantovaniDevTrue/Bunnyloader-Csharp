# Benchmark do mundo completo — Bunny C# .80

Esta experiência amplia o benchmark .78 já validado no Android com checksum correto e 60 FPS observados na captura de 512x512 tiles. Ela **não** é um novo sistema de carregamento de mapa, não renderiza nem envia pixels/texturas à GPU. A comparação com os 3 segundos de carregamento do JavaScript só poderá ser considerada equivalente quando os dois caminhos realizarem as mesmas operações.

## Implementação

O serviço existente `TileScanBenchmark.cpp` usa `TileAccess.h` e captura **todos** os `width * height` tiles reais do Terraria Mobile. Faz isso apenas na thread do jogo, com orçamento aproximado de 2200 microssegundos por atualização, em blocos de até 1.048.576 células e checagem do tempo a cada 8192 células. O tamanho máximo aceito é 24 Mi tiles (48 MiB de armazenamento C++); a alocação é verificada. Mundo ausente, dimensões alteradas ou ponteiros de TileData substituídos invalidam o snapshot. A troca de mundo limpa o estado. Nenhuma escrita de tiles.

Cada tile é um `ushort` com `65535` reservado para células inativas, como no teste .78. O checksum nativo FNV-1a 64 bits é conferido contra o checksum do MonoVM na mesma cópia do mundo. A leitura C# também conta os tiles ativos e os de tipo 0.

## Procedimento no Android

Usar APK `0.1.10.16.90.29.80-csharp-full-tile-scan`. Entrar em singleplayer com **um mundo de testes ou backup**. Abrir o Editor C#, executar:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.StartFull());
```

Fechar o Editor C# e jogar, anotando o FPS. Voltar ao editor e executar:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Progress());
```

Se a captura estiver pronta, executar:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.FinishFull());
```

Para estresse gerenciado de dez passadas sobre o mesmo snapshot (opcional):

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.FinishFull10());
```

Ao terminar, liberar memória:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Stop());
```

`Start()` e `Finish()` continuam disponíveis para a região 512x512 com dez passadas, como antes.

## Como interpretar

- `captura real`: segundos **decorridos do primeiro ao último frame** da captura. Mede o tempo de varredura real durante o jogo; diferente do tempo de CPU.
- `CPU nativa`: soma do processamento no thread do jogo.
- `copia`: tempo de alocação e cópia do snapshot para o array C# do Mono.
- `C#`: tempo do algoritmo de leitura, sem alocação, cópia nem warmup.
- `soma etapas`: `captura real + copia + C#`; aproxima o caminho de processamento e exclui o atraso manual entre fechar e abrir o editor. **Não é** a latência completa de carregamento de mapa.
- `frames`: quadros consumidos para capturar. Os 60 FPS precisam ser confirmados no Android.
- `checksum`: precisa ser idêntico em C++ e C#.

O objetivo de menos de um segundo é hipótese de desempenho, não resultado garantido. Este teste não é validação de JIT contra Interpreter ou da futura BunnyVM. O processo possui pico extra de memória por snapshot no C++ e array no C#; observar RAM e eventuais encerramentos do Android. Não repetir continuamente com mods pesados antes de validar uma primeira vez.
