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
| Identidade `[BunnyContent]` no Mono | Aplica nome/textura antes de `SetDefaults()` e valida a identidade declarada, inclusive em classes herdadas; host aprovado |
| Sorteio `Main.AnglerQuestSwap`, diálogo `Lang.AnglerQuestChat` e PT-BR | Hooks nativos compilados; integração IL2CPP simulada aprovada |
| `FishingCheck_RollItemDrop`, `FishingAttempt` e condições nativas | Integração host simulada aprovada, ABI Android pendente |
| `ItemID.Sets.IsQuestFish` e `CanBePlacedOnWeaponRacks` | Registro e restauração por mod testados no host |
| Tabelas `Main.anglerQuestItemNetIDs` e seleção preservadas | Planejamento de IDs e flags antes da aplicação; falhas de alocação simuladas |
| Compatibilidade com JS, lista de vanilla e outros mods | Reconciliação de IDs testada no host |
| Ordenação canônica dos peixes C# entre cliente/host | Ordem estável por mod/classe, inclusive com cargas invertidas e incrementais; host validado |
| Ciclo `mods::loadAll` e substituição da fonte C# | Limpeza dos registros de quests da sessão anterior; testes de C# puro/misto |
| `netMode` 0/1/2/3 | O cliente não refaz sorteio autoritativo; testes host aprovados |
| Manifest de compatibilidade | Hash FNV-1a determinístico de mod, classe, tipo de peixe, fases e políticas; calculado no registro e registrado uma vez por atualização |
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

As funções de sorteio, diálogo e pesca verificam a geração publicada do registro, recusando usar descritores antigos imediatamente após remover um mod ou invalidar um mundo. O hook de pesca também valida a instância atual de `Main.player` como array de referências `Terraria.Player` de rank 1, no máximo 256 posições, e verifica o tipo concreto do jogador antes de ler `gravDir`. Durante a transição de mundo, as flags nativas são reavaliadas separadamente por tabela: se o Terraria substituiu a tabela por outra instância, a restauração passa a respeitar os valores que já existiam na nova tabela, e não os do mundo anterior.

As tabelas nativas agora são conferidas com o validador original `il2cpp::nativeArrayContract` antes de ler os IDs da missão ou o diálogo e antes de atualizar `IsQuestFish` ou `CanBePlacedOnWeaponRacks`. O validador confirma tipo do elemento, rank do array, largura do valor e limite de entradas. Arrays incompatíveis são rejeitados sem mutação dos dados. A suíte host injeta tabelas `Boolean[]` no campo de IDs e `Int32[]` em flags para verificar essa proteção.

A reconstrução de predicados C# no Mono também segue agora a mesma declaração de conteúdo que o compilador: `[BunnyContent(BunnyContentKind.Item,...)]` fornece nome e textura antes de executar `SetDefaults()`, e a identidade é conferida depois. Classes herdadas sem a declaração são rejeitadas. O fixture C# possui peixe herdado real cujo `SetDefaults()` exige os defaults do atributo.

A atualização das tabelas possui **preparação antes do commit**: a lista de IDs e a expansão de ambas as tabelas de flags são verificadas antes de publicar alterações. Cada novo array IL2CPP temporário recebe um `GCHandle` **pinned** até que todas as tabelas sejam publicadas; ele mantém o objeto vivo e fixa o endereço enquanto ponteiros nativos são utilizados entre alocações do array de IDs e das duas flags. Os handles são liberados por RAII após o commit ou em qualquer falha. Se `gchandle_new`/`gchandle_free` não estiver disponível ou um handle não puder ser criado, a atualização é adiada, sem mutar as tabelas. O host verifica raiz durante cada publicação, a liberação dos handles e falhas simuladas na primeira, segunda e terceira alocação de handles. Isso não substitui o teste com GC IL2CPP real no Android.

## Garantias e limites de multiplayer

A lista de missões C# é ordenada por identidade do mod e nome da classe, independentemente da ordem em que cada compilação terminar. Durante troca de mundo, se `Main.anglerQuestItemNetIDs` for substituído pelo Terraria, o reconciliador não remove IDs da nova tabela usando propriedade da sessão anterior. Mantém a escolha pelo **tipo de peixe**, mesmo que o índice tenha mudado; quando um salvamento antigo traz índice negativo ou maior que a lista, normaliza para zero mesmo que os IDs não tenham mudado, sem realocar o array. Os dados persistentes de missão continuam sendo os campos nativos do Terraria, não um salvamento paralelo C#. O reconciliador preserva as entradas vanilla e de mods JS na ordem em que o Terraria as fornece e remapeia a seleção pelo **tipo real do peixe**, evitando deslocamentos indevidos dos índices. O registro publica o conjunto de peixes junto à sua revisão sob o mesmo lock; após callbacks longos, os hooks confirmam novamente que a revisão ainda está válida. Esses mecanismos foram verificados no host, inclusive com mil publicações concorrentes e descarregamento durante callbacks.

**Compatibilidade multiplayer:** a mensagem `Stage=Ready` inclui `Manifest=<hex>` com hash ordenado das identidades, IDs, regras, textos e traduções de todos os peixes C# locais, além de `EffectiveList=<hex>` com hash da **lista real e ordenada** de IDs vanilla, JavaScript e C#. O log também é atualizado quando um mod externo altera a lista, sem repetir a cada frame. Dois aparelhos com manifest ou EffectiveList diferentes possuem listas incompatíveis. Valores iguais são apenas indício de compatibilidade: os hashes **não são autenticação, handshake, transporte de estado nem prova de equivalência do código C# dos outros mods**. A infraestrutura nativa encontrada não fornece handshake C# pronto, portanto não foram criados pacotes de rede ou APIs fictícias.

`Main.netMode` é campo obrigatório do hook nativo: 0 solo, 1 cliente, 2 servidor, 3 host. O cliente só chama o sorteio vanilla; servidor e host podem rejeitar peixes indisponíveis. Valores fora de 0–3 são rejeitados pelos hooks C# de sorteio e captura, preservando o caminho vanilla sem aplicar regras do mod. Após uma condição gerenciada de captura, o hook também reconfirma o modo de rede, o dono do anzol, o jogador, a gravidade e a identidade/tipo da tentativa antes de alterar o item sorteado; transições durante o callback mantêm o resultado vanilla. A lista e o diálogo podem ser reconstruídos no cliente, mas **sincronização completa e persistência da quest em sessões multiplayer reais ainda precisam de teste e revisão**. O `AutoSourceRuntime` existente ainda impõe `singleplayer-only` para curvas de projétil e efeitos automáticos C# e só declara a sessão de live callbacks pronta quando `netMode==0`; remover essa limitação sem um sistema de sincronização confirmado seria inseguro.

Um novo `mods::loadAll` remove quests de fontes C# antigas e limpa seus registros de carregamento. O vínculo de callbacks nativos com o assembly também é removido; após o novo bind, o caminho do assembly recebe uma **revisão por mod** (`|bunny-revision=N`) para que o worker Mono reconheça uma compilação nova mesmo quando o arquivo continua no mesmo caminho. Os caches de receitas e predicados do Pescador são reconstruídos para a revisão seguinte. Isso foi verificado no host com revisões sucessivas e identificadores inválidos.

O vínculo do assembly é confirmado **antes de o worker Mono iniciar a execução** e novamente **depois** de cada resposta gerenciada de receitas e condições do Pescador. O descarregamento do mod tenta, em até 1500 µs, enviar um comando interno `kind=6` ao worker para limpar os caches de receitas e delegates dos peixes; o comando valida a revisão ativa, não limpa outros mods e rejeita requisições antigas ou malformadas. Se a limpeza não executar a tempo, o próximo bind força a expulsão dos handlers antigos. A remoção final do vínculo compara atomicamente a revisão capturada: se outra thread vinculou uma versão nova enquanto aguardava o Mono, o descarregamento anterior não remove essa versão. Essa corrida é reproduzida por um teste host com duas threads. Isso não descarrega assemblies do domínio Mono. Uma solicitação enfileirada para uma revisão antiga não deve executar o callback gerenciado. Se a revisão mudar durante a execução, a resposta antiga é descartada sem efeitos nativos. O registro de revisões está isolado em `ManagedAssemblyBindings.h`, com teste host que abrange recarga no mesmo caminho, remoção, dois mods independentes e mil ciclos concorrentes. Isso não interrompe uma chamada Mono que já esteja executando; a corrida de threads real no Android ainda requer QA.

Para os **handlers de receitas e predicados do Pescador**, o runtime agora carrega a DLL em `AssemblyLoadContext` coletável próprio de cada mod e compartilha o tipo `BunnySourceSdk` já carregado para preservar a identidade dos objetos. O descarregamento limpa os delegates e receitas, remove o registro do mod e chama `AssemblyLoadContext.Unload()`. A suíte .NET verifica não apenas `IsCollectible` e isolamento entre mods, mas também um `WeakReference` ao contexto antigo que precisa desaparecer após GC, inclusive após recompilar o mesmo mod no mesmo caminho.

**Escopo da correção:** os outros subsistemas C# que ainda carregam assemblies diretamente no domínio Mono (IA, eventos, interfaces e outros tipos nativos) não são automaticamente migrados por essa alteração. Também não é possível garantir no host que o runtime Mono embutido no Android tenha exatamente o mesmo comportamento de coleta; essa validação continua pendente no aparelho. O hot-unload integral de todos os conteúdos C# e o descarregamento de itens/NPCs/projéteis nativos ainda exigem trabalho próprio.

## Revogação dos callbacks Mono fora dos predicados

Ao descarregar ou substituir fontes C#, o cache de métodos dos callbacks
`BunnyLiveEntry`, `BunnyLiveEvents` e `BunnyLiveNpc` daquele mod é removido.
Os próximos pedidos de callbacks para o mesmo mod retornam `restart-required`
sem reabrir a DLL; classes de outros mods continuam preservadas. Uma
regressão host verifica exclusão exata de todas as entradas do mod e evita
remover outro identificador com prefixo semelhante, incluindo 1000 revogações.

**Limite:** a revogação não descarrega os assemblies do domínio Mono. Ela
impede reutilizar métodos antigos, mas ainda não implementa hot-reload.
Reiniciar o aplicativo permanece obrigatório para reativar callbacks Mono
que foram carregados diretamente no domínio. Outros subsistemas gerenciados
e suas filas precisam de auditoria de propriedade por mod.

## Filas por mod (IA e efeitos de tela)

A revogação do mod também desativa as bindings de NPCs, projéteis, iluminação
gerenciada e dano gerenciado do jogador pertencentes ao mesmo mod. Trabalhos pendentes são removidos por
identidade da binding, preservando a ordem de outros mods. A tabela nativa
de propriedade por tipo é liberada somente quando ainda aponta para a binding
revogada, e os resultados anteriores do mod são descartados. Respostas de
IA, efeitos e dano em andamento confirmam novamente que a binding continua
ativa antes de publicar resultados. Pedidos enfileirados de receitas/quests
também são cancelados ao revogar o mod. A chamada Mono que já começou não é
interrompida; o descarregamento de assemblies continua fora do escopo.

## Revogação da fila de projéteis e combate C#

O descarregamento por mod agora invalida a admissão de novos eventos
`OnHitNPC`/`OnKill` do mod revogado usando a fila nativa existente. As
bindings secundárias do mod são desarmadas individualmente, retirando apenas
os eventos desse mod e preservando os demais. Se o mod revogado for o principal,
a fila principal é desativada e os mods secundários permanecem ativos.
O resultado de um callback que já estava em execução só é aplicado quando
a binding e o mod ainda estiverem ativos. Não existe tentativa de interromper
o callback Mono em andamento nem de descarregar assemblies do domínio.

## Diagnóstico seguro da ABI Android

A preparação de `FishingCheck_RollItemDrop` diferencia os estados
`MissingIL2CPPExports`, `MissingTerrariaTypes`,
`UnexpectedFishingAttemptLayout`, `UnexpectedFishingMethodSignature`,
`UnexpectedFishingFieldLayout`, `UnexpectedNetModeContract`, `UnexpectedPlayerArrayContract` e
`HookInstallFailed`. O hook continua desativado quando o contrato falha, sem
gravar em `FishingAttempt` nem instalar código com assinatura incerta.
O relatório usa o motivo do último teste e é emitido uma única vez por
sequência de falhas; não há nova sondagem por frame. O fixture host inclui
regressões de struct com tamanho incorreto, argumento de método incompatível
e campo `questFish` com tipo divergente, além do campo `Main.netMode` ausente, não-estático ou com tipo incompatível. O fixture também rejeita exports IL2CPP ausentes, campos sobrepostos em `FishingAttempt` e `Main.player` com rank diferente de um. São verificações simuladas; a ABI
do Terraria Android e o Mono embarcado continuam exigindo validação no
dispositivo antes de qualquer pacote de instalação.

## CI e critérios de conclusão

- Testes de `Registry` (validação, reroll limitado, culturas, IDs e ownership).
- Teste do parser `BunnyPlayer`.
- Hooks C++ reais com IL2CPP simulado: sorteio, chat, cliente/host, flags, remoção e falhas de alocação.
- Hook real de pesca com struct simulada de 92 bytes: gravidade, raridade, owner (índice do jogador), modos 0/1/2/3 e rejeição de modos de rede inválidos, com callbacks dinâmicos válidos/inválidos.
- Testes de arrays IL2CPP falsos rejeitados antes de atualizar IDs/flags, e teste gerenciado de peixe herdado com `[BunnyContent]`.
- Testes de GCHandle nativo pinned: nenhuma publicação de array sem raiz/endereço fixo; falha da API, falhas intermediárias, cleanup de todos os handles e retry bem-sucedido.
- Testes de `Main.player` inválido, preservação de índice selecionado em uma nova tabela de mundo, fingerprints com ordem de mods invertida e limpeza de handlers C# sem interferir em outro mod.
- Testes de perda do jogador ou troca do dono do anzol durante callback C#, modificação externa de flags originalmente verdadeiras e detecção de diferenças de lista vanilla/JS e de traduções.
- Índice restaurado inválido ou negativo com lista inalterada, e corrida de descarregamento de assembly antigo contra vínculo novo: testes C++ host aprovados.
- Regressão de descarregamento com hook já instalado, transição de mundo com tabelas substituídas e teto de quatro chamadas gerenciadas por sorteio.
- Execução .NET das condições `QuestAvailableWhen`/`QuestCanCatchWhen` no endpoint gerenciado, incluindo recusa de snapshots inválidos.
- Compilação C++ e C# com avisos tratados como erros no host; regressão de carregamento C# isolado e JS misto.
- Compilação de sintaxe do próprio `ManagedRuntime.cpp` e teste host isolado de descarregamento/recarga de assemblies nativos com mil ciclos concorrentes.
- Regressão .NET real para `AssemblyLoadContext` coletável por mod, compartilhamento correto de `BunnySourceSdk`, `WeakReference` liberado após descarregamento e recarga, e manutenção dos handlers de outros mods.
- Compilação cruzada com o Android NDK arm64 para `NativeAnglerQuestGameplay.cpp`, `NativeAnglerQuestFishing.cpp`, `NativeRecipeCallbacks.cpp`, `ManagedRuntime.cpp`, `SourceLoader.cpp` e `AutoSourceRuntime.cpp`. Não produz APK nem valida a ABI específica do Terraria em execução.

**CI completo aprovado:** [Actions #39360671798](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39360671798). Jobs `native-host`, `managed-host` e `arm64-android-native` aprovados, incluindo coleta real dos contextos .NET após unload/rebind e compilação NDK arm64 dos arquivos de integração.

Ainda faltam execução da ABI real do Terraria Android, validação do coletor Mono Android e desempenho a 60 FPS, sincronização/persistência multiplayer real, migração dos demais subsistemas gerenciados para contextos descarregáveis e possíveis callbacks adicionais do Pescador além dos predicados tipados implementados. O código existente em `AutoSourceRuntime.cpp` continua explicitamente singleplayer para curvas de projétil e efeitos automáticos C#, portanto essa PR não comprova suporte completo de multiplayer para todos os tipos de mod C#.

**Não executar Gradle Android, não gerar APK ou .bmod enquanto houver qualquer bloqueio.**
