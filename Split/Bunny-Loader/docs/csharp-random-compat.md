# Lote C# — Terraria.Utilities (compatibilidade tModLoader)

Este lote implementa de ponta a ponta uma API **gerenciada**, sem atravessar o bridge C++/IL2CPP: `Terraria.Utilities.UnifiedRandom`, `Terraria.Utilities.WeightedRandom<T>` e as extensões aleatórias de `Terraria.Utils`.

## Contrato e fidelidade

- Mantém os construtores e membros públicos de `WeightedRandom<T>` vistos na documentação do tModLoader, inclusive o operador implícito, `elements`, `random`, `needsRefresh`, `Add`, `Get`, `Clear` e `CalculateTotalWeight`.
- `UnifiedRandom` usa o gerador gerenciado `System.Random(seed)`, com `SetSeed` para reinicialização. Isso dá sequências repetíveis no **mesmo runtime**, mas não é uma promessa de igualdade bit a bit com o PRNG do Terraria Android/IL2CPP.
- `NextBool`, `NextFloat`, escolha em listas e funções vetoriais seguem nomes de `Terraria.Utils`. Os vetores são `System.Numerics.Vector2` (tipo disponível no SDK C#), não `Microsoft.Xna.Framework.Vector2` nativo.
- A implementação reforça pesos: negativos, NaN, infinitos, soma estourada e tabelas sem peso positivo geram exceção em vez de escolher opções inválidas.
- `Get()` não aloca por sorteio. `CalculateTotalWeight` é executado novamente somente quando `needsRefresh` for verdadeiro.
- A instância não é sincronizada automaticamente com `Main.rand`, multiplayer ou outros mods. Para eventos de jogo que exijam sincronização, o host deverá fornecer um seed de origem autorizada e sincronizar o resultado.

## Exemplo C#

```csharp
using Terraria;
using Terraria.Utilities;

var rng = new UnifiedRandom(12345);
var drops = new WeightedRandom<int>(rng);
drops.Add(2, 7.0);
drops.Add(3, 2.0);
drops.Add(4, 1.0);

int itemType = drops.Get();
bool chance = rng.NextBool(4);
float speed = rng.NextFloat(3f, 6f);
```

O exemplo que compila junto com o ExampleMod está em `samples/ExampleModCSharp/Playable/content/Common/Random/ExampleWeightedChoice.cs`. Ele **não cria drops sem uma fonte nativa válida**: apenas seleciona IDs. O chamador é responsável pelo evento de drop via API disponível em seu mod.

## Gate do lote (sem APK)

```sh
dotnet run --project Split/Bunny-Loader/tests/random_compat/RandomCompat.csproj --configuration Release
```

A Action `check-csharp-random-compat.yml` executa esse teste de contrato sem Gradle, NDK, APK ou Android. O teste verifica seeds, pesos, validação de erros, vetores e extensões.

A entrega deste lote corresponde **somente** à API gerenciada aleatória, não ao espelhamento do `Main.rand` nativo. Não habilitar distribuição Android sem os gates gerais do projeto e testes no aparelho.
