# AnglerQuestLoader C# — integração em revisão

**PR #114 (draft). Não gerar APK, .bmod ou release.** Base de implementação: Bunny Loader e ExampleModBunny, sem copiar o tModLoader, sem depender de QuickJS para os componentes C#.

## Referências originais

- `app/src/main/cpp/script/js/mod/Loaders/AnglerQuestLoader.js`
- `app/src/main/cpp/script/js/mod/Loaders/PlayerLoader.js`, comportamento `CatchFish`
- `samples/ExampleMod/content/Content/Items/Quests/ExampleQuestFish.js`
- `samples/ExampleMod/content/Common/Players/ExampleFishingPlayer.js`

O sprite do peixe mantém o mesmo blob original.

## Contratos implementados

| Sistema | Estado |
| --- | --- |
| `BunnyItem.DefaultToQuestFish()`, fases pré/Hardmode e `BunnyQuestCatchRules` | C# SDK, compiler e registro nativo implementados |
| `BunnyPlayer.FishingQuestFishName/FishingCatchRules` | Implementado e testado |
| `BunnyItem.QuestAvailableWhen` e `QuestCanCatchWhen` | Delegates C# tipados com `BunnyQuestContext`, execução no worker Mono, host testado |
| Sorteio `Main.AnglerQuestSwap`, diálogo `Lang.AnglerQuestChat` e PT-BR | Hooks nativos compilados; integração IL2CPP simulada aprovada |
| `FishingCheck_RollItemDrop`, `FishingAttempt` e condições nativas | Integração host simulada aprovada, ABI Android pendente |
| `ItemID.Sets.IsQuestFish` e `CanBePlacedOnWeaponRacks` | Registro e restauração por mod testados no host |
| Tabelas `Main.anglerQuestItemNetIDs` e seleção preservadas | Planejamento de IDs e flags antes da aplicação; falhas de alocação simuladas |
| Compatibilidade com JS, lista de vanilla e outros mods | Reconciliação de IDs testada no host |
| Ciclo `mods::loadAll` e substituição da fonte C# | Limpeza dos registros de quests da sessão anterior; testes de C# puro/misto |
| `netMode` 0/1/2/3 | O cliente não refaz sorteio autoritativo; testes host aprovados |
| Multiplayer e persistência de missão em dispositivos reais | Ainda não validados |
| Hot-unload de todos os tipos de conteúdo C# | Ainda não concluído |
| Terraria Android, crash-free e 60 FPS | Ainda não testados |

## Contrato do compilador

```text
QUEST_FISH<TAB>Classe<TAB>PreHardmode<TAB>Hardmode<TAB>DescricaoHexOu-<TAB>LocalHexOu-<TAB>CatchRules
QUEST_CALLBACK<TAB>Classe<TAB>CallbackMask
QUEST_LOCALE<TAB>Classe<TAB>Cultura<TAB>DescricaoHexOu-<TAB>LocalHexOu-
QUEST_CATCH<TAB>ClasseBunnyPlayer<TAB>NomePeixe<TAB>CatchRules
```

Em `CatchRules`, o bit 1 exige gravidade invertida, o bit 2 exige tentativa de pesca incomum, e 3 exige ambos. O `CallbackMask` usa bit 1 para `QuestAvailableWhen` e bit 2 para `QuestCanCatchWhen`. O protocolo antigo de `QUEST_FISH` com seis colunas permanece aceito.

`BunnyQuestContext` contém `FishType`, `Hardmode`, `InvertedGravity`, `Uncommon` e `PlayerIndex` (`-1` em consultas globais). As condições são delegates C# reconstruídos no worker Mono a partir do assembly compilado e consultados através do RPC limitado já existente de receitas. Disponibilidade é consultada apenas na escolha da missão, com cache por peixe na mesma chamada e **máximo de quatro chamadas gerenciadas por sorteio** (até 1.500 µs por RPC). Captura é consultada apenas durante o sorteio de pesca, com identificação validada do jogador que lançou o anzol. Não existe execução Mono por frame. Se o worker estiver indisponível, a condição personalizada não autoriza o resultado; o jogo segue a rotina nativa/vanilla.

Peixes sem `QUEST_CATCH`, sem `QuestCatchRules` e sem `QuestCanCatchWhen` não alteram o resultado do sorteio de pesca. Uma condição dinâmica de captura, isoladamente, pode autorizá-lo.

A localização aceita até oito culturas por peixe com textos UTF-8 de até 512 bytes. A cultura ativa é resolvida via IL2CPP, com fallback por idioma e para inglês.

As funções de sorteio, diálogo e pesca verificam a geração publicada do registro, recusando usar descritores antigos imediatamente após remover um mod ou invalidar um mundo. Durante a transição de mundo, as flags nativas são reavaliadas separadamente por tabela: se o Terraria substituiu a tabela por outra instância, a restauração passa a respeitar os valores que já existiam na nova tabela, e não os do mundo anterior.\n\nA atualização das tabelas possui **preparação antes do commit**: a lista de IDs e a expansão de ambas as tabelas de flags são verificadas antes de publicar alterações. Falhas de alocação são testadas e não deixam estado parcial nos mocks do host. Isso não constitui garantia de transação atômica do runtime IL2CPP real, que precisa de validação no dispositivo.

## Garantias e limites de multiplayer

`Main.netMode` é campo obrigatório do hook nativo: 0 solo, 1 cliente, 2 servidor, 3 host. O cliente só chama o sorteio vanilla; servidor e host podem rejeitar peixes indisponíveis. A lista e o diálogo podem ser reconstruídos no cliente, mas **sincronização completa e persistência da quest em sessões multiplayer reais ainda precisam de teste e revisão**.

Um novo `mods::loadAll` remove quests de fontes C# antigas e limpa seus registros de carregamento. Essa limpeza não equivale a descarregar todos os itens, NPCs, projéteis e assemblies C# durante o jogo; o hot-unload completo permanece pendente.

## CI e critérios de conclusão

- Testes de `Registry` (validação, reroll limitado, culturas, IDs e ownership).
- Teste do parser `BunnyPlayer`.
- Hooks C++ reais com IL2CPP simulado: sorteio, chat, cliente/host, flags, remoção e falhas de alocação.
- Hook real de pesca com struct simulada de 92 bytes: gravidade, raridade, owner (índice do jogador) e callbacks dinâmicos válidos/inválidos.\n- Regressão de descarregamento com hook já instalado, transição de mundo com tabelas substituídas e teto de quatro chamadas gerenciadas por sorteio.
- Execução .NET das condições `QuestAvailableWhen`/`QuestCanCatchWhen` no endpoint gerenciado, incluindo recusa de snapshots inválidos.
- Compilação C++ e C# com avisos tratados como erros no host; regressão de carregamento C# isolado e JS misto.

**Checkpoint aprovado antes do novo teste de índice do jogador:** [Actions #39350068007](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39350068007). Jobs `native-host` e `managed-host` aprovados com testes de hot-unload lógico, restauração no mundo e limite de RPC. Conferir também a execução mais recente antes de encerrar a PR.

Ainda faltam ABI do Terraria Android, validação de memória e desempenho a 60 FPS, sincronização/persistência multiplayer real, hot-unload integral de conteúdo C# e possíveis callbacks adicionais do Pescador além dos predicados tipados implementados.

**Não executar Gradle Android, não gerar APK ou .bmod enquanto houver qualquer bloqueio.**
