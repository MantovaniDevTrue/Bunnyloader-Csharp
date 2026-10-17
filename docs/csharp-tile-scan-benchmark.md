# Benchmark C# de tiles reais (experimental .78)

O teste usa o acesso nativo existente em `content/tiles/TileAccess`, sem JavaScript, sem escrita no mundo e sem novos hooks de jogo. A captura central de 512 x 512 tiles é realizada em 32 atualizações de no máximo 8192 células cada, no próprio game thread. Uma mudança de mundo ou realocação de TileLookup invalida o snapshot.

O snapshot contém 262144 valores `ushort`. Tiles inativos são representados por 65535. O código C# do SDK varre o mesmo buffer dez vezes, com uma passada de aquecimento excluída da medição. A implementação nativa calcula um FNV-1a de 64 bits enquanto captura; a rotina C# precisa produzir exatamente o mesmo hash.

## Android

Instale a build com este commit, entre em um mundo singleplayer e abra o Editor C#:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Start());
```

Feche o editor e deixe o mundo continuar por pelo menos 32 frames. Abra novamente:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Progress());
```

Quando a captura estiver pronta:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Finish());
```

Ao terminar:

```csharp
Print(BunnyLoader.Source.BunnyTileScanBenchmark.Stop());
```

Se um comando imprimir uma falha ou não compilar, guarde o erro exato e não atribua sucesso ao teste. Não executar em multiplayer ou durante transições de mundo.

O tempo `C#` inclui somente o loop de leitura das dez passadas; exclui a alocação/cópia, aquecimento e captura. O tempo `captura nativa` inclui leitura dos dados reais em vários frames, mas não é um benchmark C++ sobre um buffer contíguo equivalente. Não comparar diretamente essas duas taxas como se fossem idênticas. O editor pausa o Terraria enquanto aberto, portanto esse resultado não mede FPS com gameplay ativo. Medir FPS em sessão separada para validar impacto da captura, e só comparar Mono JIT/Interpreter ou BunnyVM com o mesmo algoritmo e mesmo snapshot.
