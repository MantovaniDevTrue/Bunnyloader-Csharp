# Random Compat Smoke CSharp — Android QA

Mod de teste C# independente; não modifica o ExampleMod CSharp nem usa JavaScript.

Este mod registra `RandomCompatProbePlayer : BunnyPlayer`, que executa as APIs `Terraria.Utilities.UnifiedRandom`, `WeightedRandom<int>` e `Terraria.Utils` uma única vez no callback `OnEnterWorld` (não a cada frame).

## Teste no Terraria Android

1. Use um Bunny Loader C# **compilado do mesmo commit de QA**, com o SDK atualizado. O aplicativo antigo não contém os novos tipos.
2. Importe `RandomCompatSmokeCSharp-v0.1.0.bmod` pela tela de mods e habilite apenas esse mod para o primeiro teste.
3. Entre em um mundo singleplayer e confira a mensagem/registro retornado por `OnEnterWorld`:
   `Bunny C# Random Compat: PASS (seed/weights/bool/float/vector/list)`.
4. Se o mod não carregar ou não aparecer nenhuma mensagem, consulte o relatório do Bunny e procure os diagnósticos `MonoVM`, `Roslyn` ou `RandomCompatProbePlayer`. Ausência de mensagem não significa aprovação.
5. Repita com outros mods e registre tempo de carregamento, FPS e memória; o teste compilado no PC não comprova esses valores.

`OnEnterWorld` verifica igualdade de 32 resultados por seed, reinicialização com `SetSeed`, 32 escolhas com peso zero/positivo, `NextBool`, intervalo de `NextFloat`, vetor normalizado e seleção em lista. O teste não cria itens, drops, nem escreve estado do jogador.

## CI gerenciada

`dotnet run --project Split/Bunny-Loader/tests/random_compat_android_probe/RandomCompatAndroidProbe.csproj -c Release`

O projeto compila o **mesmo fonte C#** enviado ao mod e chama o callback 200 vezes. O CI empacota um .bmod que contém apenas fonte C# e manifesto.
