# Partículas da poção — Bunny C#

Referência: `samples/ExampleMod/content/Content/Items/Consumables/ExampleBuffPotion.js`, Bunny JS 1.9.0.

A implementação em `Content/Items/Consumables/ExampleBuffPotion.cs` preserva a duração de 5400 ticks, raridade Orange, preço, textura, som Item3 e três cores dos resíduos de bebida: (240,240,240), (200,200,200), (140,140,140). Não cria receita que não exista na fonte.

`BunnyNativeDefaults.SetItemDrinkParticleColors` aceita de uma a oito instâncias de `BunnyColor`, serializadas com os valores RGBA. O host nativo valida `ItemID.Sets.DrinkParticleColors` como `Color[][]`, consulta os offsets de canais do runtime e cria a lista de cores somente na thread do jogo. O valor anterior é mantido por handle gerenciado; a retirada seletiva restaura apenas a célula que ainda pertence ao mod. Outro mod pode alterar a célula sem ser sobrescrito na retirada. Recompilações substituem o valor sem carregar JavaScript.

Host QA: `tests/drink_colors/native_test.cpp`, `tests/basic_items/Program.cs` e `tests/rarity_loader/PlayableRarityProgram.cs`; executados pelo workflow `check-csharp-golf-ball.yml`. Os testes incluem falha atômica por ABI inválido, canais não canônicos, limpeza no unload e preservação de outra célula. Testes no Terraria Android 301720, multiplayer, renderização e consumo de memória real continuam pendentes. Sem APK.
