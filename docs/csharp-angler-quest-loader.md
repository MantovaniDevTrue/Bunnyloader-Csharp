# AnglerQuestLoader C# — implementação isolada

**Estado: WIP, não pronto para instalar.** Esta branch foi criada de `v1.0` e não altera os lotes paralelos de Tile, Prefix, Localization ou Rarity.

## Fonte de comportamento

- Bunny Loader original: `app/src/main/cpp/script/js/mod/Loaders/AnglerQuestLoader.js`
- ExampleModBunny: `samples/ExampleMod/content/Content/Items/Quests/ExampleQuestFish.js`
- ExampleModBunny: `samples/ExampleMod/content/Common/Players/ExampleFishingPlayer.js`
- Imagem e textos do peixe de ponta-cabeça reutilizados do ExampleModBunny.

O caminho de execução C# é independente do JavaScript. A fonte JS é apenas referência comportamental, não ponte.

## Implementado até aqui

| Requisito | Estado |
| --- | --- |
| API `BunnyItem.DefaultToQuestFish()` | Código presente |
| Flags Pre-Hardmode/Hardmode e diálogo do Pescador | Código presente, ainda sem hook de jogo |
| Compilação dos metadados C# para `QUEST_FISH` no wire V2 | Código presente; CI host |
| Validação de owner/ID/duplicatas e texto UTF-8 | Código presente; CI host |
| Registro nativo por mod e unload lógico | Código presente; CI host |
| Mesclagem de IDs e máximo de 50 sorteios | Código presente; CI host |
| Exemplo C# com sprite original | Código presente; ainda não jogável |
| `ItemID.Sets.IsQuestFish` no Terraria | Pendente |
| Array `Main.anglerQuestItemNetIDs` nativo, sem conflito com JS | Pendente |
| Hooks `Main.AnglerQuestSwap` e `Lang.AnglerQuestChat` | Pendente |
| Callback C# dinâmico, chat localizado e gravação multiplayer | Pendente |
| Captura via `CatchFish` como ExampleFishingPlayer | Pendente |
| Teste em Terraria Android e 60 FPS | Pendente |

## Contrato atual

`QuestFish` só pode ser usado em `BunnyItem`; o registrador verifica a mesma classe em `ITEM`. Metadados do peixe são publicados **depois dos defaults**. A classe usa `MaxStack=1`, aceita nomes por mod e não modifica o sistema de spawn no estado atual.

O compilador escreve:

```text
QUEST_FISH<TAB>NomeDaClasse<TAB>PreHardmode(0/1)<TAB>Hardmode(0/1)<TAB>DescriptionHexOu-<TAB>LocationHexOu-
```

O registrador rejeita nomes inválidos, IDs fora do conjunto registrado, dois peixes com o mesmo ID, texto com controles perigosos e descrição/localização acima de 512 bytes UTF-8. O reroll nativo não ultrapassa 50 tentativas.

## Critérios de conclusão

Não marcar como completo até verificar, no Terraria real, distribuição e atualização dos arrays de missões, chat do NPC, sorteio por condições, captura do peixe, entrada/saída de mundos, mundo singleplayer e multiplayer, coexistência C#/JS, restauração após unload, textos em idiomas suportados e estabilidade/FPS sem alteração no balanceamento.

**Regra obrigatória: NÃO criar, compilar ou publicar APK, .bmod, release ou Gradle Android antes da conclusão total.** Os jobs desta branch só compilam C++ no host e o SDK/compilador C# em .NET host.
