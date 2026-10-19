# AnglerQuestLoader C# — fonte em revisão

**PR #114 (draft). Sem APK, .bmod ou release.** Desenvolvimento isolado da branch `v1.0`, sem copiar o tModLoader e sem usar o QuickJS no caminho C#.

## Fonte do Bunny original

- `app/src/main/cpp/script/js/mod/Loaders/AnglerQuestLoader.js`
- `app/src/main/cpp/script/js/mod/Loaders/PlayerLoader.js`, `CatchFish`
- `samples/ExampleMod/content/Content/Items/Quests/ExampleQuestFish.js`
- `samples/ExampleMod/content/Common/Players/ExampleFishingPlayer.js`

O sprite foi copiado com o mesmo Git blob do ExampleModBunny.

## Componentes já implementados em código

| Categoria | Estado |
| --- | --- |
| `BunnyItem.DefaultToQuestFish()` e atributos de disponibilidade | SDK C# implementado |
| `BunnyQuestCatchRules` (gravidade invertida e sorteio incomum) | SDK C# implementado |
| `BunnyPlayer.FishingQuestFishName/FishingCatchRules` e `QUEST_CATCH` | SDK, compilador e SourceLoader implementados; host validado |
| Compilador: `QUEST_FISH` contendo flags, UTF-8 hex e regras de captura | Host validado |
| `SourceLoader`: proprietário, identidade, defaults e publicação ao final | Código integrado; host CI |
| `NativeAnglerQuestRegistry`: por mod, deduplicação, atualização, unload lógico, 50 tentativas, UTF-8 | Host validado |
| `NativeAnglerQuestGameplay`: `AnglerQuestSwap`, `AnglerQuestChat`, `IsQuestFish` e array de quests | Código presente; **não validado em Terraria Android** |
| `NativeAnglerQuestFishing`: `FishingCheck_RollItemDrop` e `FishingAttempt` (92 bytes) | Código presente; **ABI Android não confirmada** |
| `ExampleQuestFish.cs` e `ExampleFishingPlayer.cs` separados como no ExampleModBunny | Wire e parser host validados; ainda não testados no jogo |
| Preservação dos IDs JS/vanilla, escolha atual e limpeza de IDs C# antigos | Reconciliador C++ testado no host; jogo real pendente |
| PT-BR do ExampleModBunny, fallback por idioma e detecção nativa da cultura | Implementado em fonte e validado no host; Android pendente |\n| Hooks dinâmicos C# arbitrários e sincronização de captura por jogador | Pendente |
| Reinvalidação na entrada/saída de mundo | Implementada na thread do jogo; Android pendente |\n| Multiplayer, save e remoção de mod a quente | Pendente |
| Testes Android e desempenho/FPS | Pendente |

## Protocolo de compilação

O compilador emite dois registros independentes para item e jogador:

```text
QUEST_FISH<TAB>Classe<TAB>PreHardmode<TAB>Hardmode<TAB>DescriptionUtf8HexOu-<TAB>LocationUtf8HexOu-<TAB>CatchRules
QUEST_CATCH<TAB>ClasseBunnyPlayer<TAB>NomePeixe<TAB>CatchRules\nQUEST_LOCALE<TAB>NomePeixe<TAB>Cultura<TAB>DescricaoHexOu-<TAB>LocalHexOu-
```

`CatchRules=1`: gravidade invertida; `CatchRules=2`: captura incomum; `3` exige ambos. A leitura nativa também aceita o formato anterior de seis colunas, com regras `0`.

O item expõe `QuestDescriptionTranslations` e `QuestCatchLocationTranslations`, com no máximo oito culturas válidas por classe e textos de até 512 bytes UTF-8. O diálogo é escolhido pela cultura ativa do Terraria (exata, depois idioma), com fallback inglês. Não há chamada JavaScript no diálogo.\n\nO registro só é publicado depois das demais operações de defaults. O registro QUEST_CATCH exige BunnyPlayer e um BunnyItem de missão do próprio mod; duplicações e políticas conflitantes são rejeitadas. A thread do jogo atualiza `IsQuestFish` e a lista de `anglerQuestItemNetIDs` sem deslocar entradas JS, usando identidade estável do mod. Os hooks conferem métodos e campos IL2CPP antes de atuar; contratos incompatíveis preservam o funcionamento vanilla. O hook de pesca usa `resolveValueLayout` existente no núcleo, aplica o predicado testado por C++, e não executa MonoVM nem JS.

## Testes e bloqueios

O workflow `check-csharp-angler-quest-host.yml` executa compilação C++ host com `-Werror`, suite do registro, teste do parser da classe de jogador, verificação de sintaxe da integração `SourceLoader`/`AutoSourceRuntime`, compilação .NET do SDK/compilador e execução do `CompiledAssemblyProbe` no peixe real.

**Checkpoint de CI:** SDK/compilador, geração do wire incluindo PT-BR, parser da classe global, reconciliação de IDs com outros mods, políticas nativas e compilação das unidades C++ alteradas são verificadas pelo workflow `check-csharp-angler-quest-host.yml`. Consultar o último resultado antes de fechar a PR.

**Ainda não existe garantia de paridade total:** salvar/restaurar dinâmica de missões em multiplayer, traduções por cultura, hooks gerenciados personalizáveis de pesca/jogador, validação de acesso e ABI IL2CPP no Terraria Android, efeitos de outra versão do jogo, união com mods JS e 60 FPS. Não marcar esta PR como pronta até testes funcionais e regressões completas.

**Política absoluta deste lote: não executar Gradle Android, não compilar/gerar/distribuir APK ou .bmod enquanto houver qualquer item pendente.**
