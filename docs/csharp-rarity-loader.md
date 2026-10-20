# RarityLoader C# — implementação independente em andamento

**Branch:** `feat/csharp-rarity-loader-core-20261019`. **Status:** parcial, não publicável. Nenhum APK ou `.bmod` deve ser construído, exportado ou distribuído por este trabalho enquanto houver contratos incompletos.

## Escopo e origem

O sistema escolhido é **RarityLoader**, no lote `loaders-05`. A PR #98 trabalha Tile/Wall/Tree/TileEntity/GlobalTile, a PR #100 trabalha Item/Equip/ArmorSet/Wing/Prefix, e a PR #101 trabalha Localization. Nenhum desses escopos precisa ser editado para iniciar a raridade.

Fontes de comportamento do Bunny Loader original (somente referência, **sem código JS no runtime C#**):

- `Split/Bunny-Loader/app/src/main/cpp/script/js/mod/ModRarity.js`
- `Split/Bunny-Loader/app/src/main/cpp/script/js/mod/Loaders/RarityLoader.js`
- `Split/Bunny-Loader/samples/ExampleMod/content/Content/Rarities/ExampleModRarity.js`
- `Split/Bunny-Loader/samples/ExampleMod/content/Content/Rarities/ExampleHigherTierModRarity.js`

## Implementado neste checkpoint

- Classe `BunnyRarity` e registro `BunnyRarityRegistry` totalmente gerenciados, sem bibliotecas JS ou tModLoader.
- IDs determinísticos a partir de `ItemRarityID.Count = 12`, segregação por mod, detecção de duplicata, limites e publicação imutável.
- `SetStaticDefaults` único antes da publicação; falha de inicialização não gera snapshot utilizável.
- `RarityColor` com getter dinâmico e consulta de cor sem lock no caminho de leitura.
- `GetPrefixedRarity` com degraus de `valueMult` exatamente `[-2, -1, 0, 1, 2]`; retorno de raridades modded sem ser truncado ao limite vanilla 11; clamp e sentinelas negativos conforme o Bunny.
- Testes de host para IDs, isolamento, erros, publicação, cor dinâmica, limites de prefixo, transição entre raridades e casos inválidos.
- CI de host exclusivamente `dotnet run`, sem Android/Gradle e sem empacotamento.

O exemplo em teste cria duas subclasses de raridade, incluindo subida e descida por prefixos. **Isso não é ainda um exemplo jogável nem reprodução visual aprovada.**

## Continuação — scanner, IDs nativos e contrato V2 (19/10)

- O scanner nativo `SourceParser` reconhece classes concretas que estendem `BunnyRarity`, mesmo sem `SetDefaults`; métodos gerenciados permanecem no MonoVM.
- O `SourceLoader` reserva IDs em um registro C++ global C# com nomes `<mod>/<classe>`, snapshots imutáveis e proteção de duplicatas.
- O `CompiledAssemblyProbe` inspeciona classes de raridade e emite `RARITY\\tNome` no contrato gerenciado V2; o `SourceLoader` confere que cada identidade compilada corresponde ao ID reservado.
- O SDK oferece `BunnyRarityRegistry(firstCustomType)` para catalogar o intervalo de IDs fornecido pelo host quando houver outros mods antes dele.
- A suíte de host ganhou conferência do `CompiledAssemblyProbe` real e teste C++ de scanner, registro, limites e concorrência.

**Estado:** registro de IDs e handoff de declarações estão implementados, mas ainda não há a ligação desses IDs de volta aos objetos gerenciados no momento de `SetStaticDefaults`. O hook de popup está implementado na continuação abaixo; a aplicação real no Android permanece pendente. Nenhuma alteração publica APK.

## Checkpoint — dispatcher MonoVM e testes reais de host (19/10)

- Novo `BunnyRarityRuntime.Dispatch` em C# no SDK efetivo: vincula todas as raridades de um assembly **em lote**, atribuindo os IDs do host antes de `SetStaticDefaults`, permite lookup por tipo, remove registros do mod e isola exceções.
- A rota `RarityColor` recebe os canais Disco do host e devolve `COLOR|R|G|B|A`; a rota de prefixo recebe o multiplicador e devolve `RARITY|N`. São contratos de worker, **não hooks de desenho já instalados**.
- `ManagedRuntime.cpp` reutiliza a fila MonoVM limitada existente com despacho próprio `BunnyRarityRuntime`, pré-aquecimento por mod e API nativa para consultas limitadas. `AutoSourceRuntime.cpp` envia todas as identidades `RARITY:Classe -> ID` após a aplicação dos defaults C# e inclui mods que têm apenas raridades no prewarm.
- Foram portadas, como **fixtures C# isoladas**, as duas raridades do ExampleModBunny: `ExampleModRarity` e `ExampleHigherTierModRarity`. Os testes verificam cor fixa, Disco, transições nos dois sentidos, IDs 14/15 e 18/19, colisão de IDs, perda de bind e descarregamento.
- A CI de host executa C#, teste nativo de IDs/parser, compilação `-Wall -Wextra -Werror` de `SourceLoader.cpp` e `AutoSourceRuntime.cpp` e regressões reais do worker C++. Também regenera a referência pública do SDK a partir do assembly compilado.

**Evidência:** [RarityLoader Core #39312836894](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39312836894) concluiu com sucesso no SHA `e9c87664ce321f807d3898559ecbb19d28567c8c`. Nenhuma build Android, APK, `.bmod` ou confirmação de FPS ocorreu.

**Atenção:** o prewarm é enfileirado, e agora é verificado pelo sampler nativo antes da primeira consulta de cor. A fila nativa não equivale ao hook de Terraria. O item jogável de exemplo e os hooks visuais ainda não estão completos.

## Checkpoint — hooks nativos de popup e prefixo (19/10)

- Adicionado `NativeRarityPalette`, com **4096 slots atômicos** e leitura sem lock no hook de renderização. A atualização recebe apenas uma cor serializada validada `COLOR|R|G|B|A`, e o Color de quatro bytes é reempacotado com o layout IL2CPP real validado em runtime.
- `NativeRarityGameplay` instala `Item.GetPopupRarityColor(int itemRarity)` somente após uma primeira amostra gerenciada válida; sem cor no cache, chama a implementação original de Terraria. Não chama MonoVM durante o desenho.
- A rotina de atualização verifica o resultado do **prewarm em lote**, amostra no máximo **uma cor a cada dois game updates** (30 Hz totais a 60 FPS), consulta `Main.DiscoR/G/B` nativamente com validação de tipo e preserva a última cor quando o worker excede o orçamento.
- Hook nativo `Item.Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)`: guarda `item.rare` antes da chamada original, preserva o contrato `Prefix(-3)`, lê o multiplicador de `TryGetPrefixStatMultipliersForItem` quando o prefixo foi aplicado e invoca a regra `GetPrefixedRarity` no worker com orçamento limitado. Se o worker falha, preserva a raridade personalizada anterior sem deixar a raridade ser truncada para 11.
- Validação de ABI/assinaturas feita em runtime e hook instalado com fallback original se a API não corresponder à build. Não foi executado ainda em Android.
- Testes C++ novos verificam decoding estrito de `COLOR`/`RARITY`, limites, estado de cache e publicação/leitura concorrente; a CI compila `NativeRarityGameplay.cpp` com `-Werror`.

**Verificação:** [RarityLoader Core #39315222797](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39315222797), host CI com sucesso no SHA `546c8baf3f366f0272dacd6c6a03b9e1306b6e31`.

**Atenção:** a animação depende do orçamento total compartilhado entre raridades; múltiplos mods dividem a taxa de amostragem. O hooking em jogo continua **não validado**; não foram integrados `ItemRarity.Initialize`, `Main.MouseTextInner`, a cor do nome no tooltip, raridades JS coexistentes, associação `item.rare` à raridade de itens C#, nem saves/multiplayer.

## Lote — tabela ItemRarity, vinculação de itens e isolamento de cores (19/10)

- `BunnyItem.SetRarity<T>()` registra a identidade de uma classe `BunnyRarity` sem resolver IDs durante `SetDefaults`. O compilador emite `ITEM_RARITY\\tItem\\tRaridade` somente quando a raridade concreta existe no mesmo assembly.
- `SourceLoader` valida a identidade do item e da raridade registrada no mesmo mod; após analisar o `ITEM` vanilla da fonte, vincula o ID nativo a `BasicItemEntry.values[BasicRare]` antes de aplicar o lote de propriedades ao Terraria.
- O wire original `ITEM` ainda não aceita valores de raridade acima de 11 declarados pelo mod. Somente a referência tipada, resolvida e verificada no host, pode usar IDs C# 12..4107. Há teste de host contra IDs falsificados.
- O fixture dos dois exemplos de raridade inclui agora `ExampleRarityItem` com `SetRarity<ExampleModRarity>()`. A CI verifica as linhas geradas, inclusive os IDs resolvidos indiretamente.
- O hook `Terraria.GameContent.UI.ItemRarity.Initialize()` foi acrescentado com validação nativa da tabela `_rarities`, `ContainsKey`, `set_Item`, chave `Int32` e ABI `Color`. Após a chamada original, reinstala somente as cores C# que não encontrem IDs já ocupados por outro registro. A amostragem posterior atualiza as entradas que o próprio host marcou como suas.
- A política de proteção é **não substituir uma entrada preexistente** da tabela. Isto **não soluciona** a colisão global de IDs com o RarityLoader JavaScript, que continua atribuindo seus próprios números.
- Mantido fallback original para pop-ups e ausência de chamada MonoVM no desenho. Ainda faltam os hooks de nome no chão, tooltip e demais superfícies visuais.

**Validação de host:** [CI #39316929714](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39316929714) passou após o hook de tabela. Testes adicionais de item falsificado e ABI estão na revisão atual, sujeitos à nova CI. Não houve APK, validação Android ou medição de FPS.

## Lote — IDs compartilhados entre JS e C# (19/10)

- O alocador `NativeRarityRegistry` distingue entradas gerenciadas (`managed=true`) das reservas JS (`managed=false`) e atribui IDs pelo **mesmo snapshot atômico**, começando em 12, sem reserva paralela e colisão entre esses dois loaders.
- O QuickJS **existente** recebeu apenas `bl.csharp.reserveRarityId()` e `bl.csharp.rarityCount()`, funções C++ que operam diretamente no registro nativo. **Mods C# não executam nem consultam JavaScript**.
- O `RarityLoader.js` original consulta a reserva compartilhada em `Add(inst)`; o `RarityCount` usado nos prefixos reflete também IDs registrados pelo C#. Há fallback compatível com o loader JS antigo quando a ponte nativa não existe.
- A rotina visual C# ignora explicitamente reservas JS no hook de inicialização de cores, no lookup de prefixos e na enumeração de raridades de um mod.
- Testes reais de host cobrem a sequência JS/C#/JS/C#, capacidade de 4096, esgotamento, IDs duplicados, retorno do contador nativo e concorrência entre duas threads. Um teste Node executa **o próprio arquivo** `RarityLoader.js` com a ponte nativa simulada e verifica que ele aceita o ID intercalado.

**Verificação:** [CI #39318842507](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39318842507) passou com o contrato de IDs compartilhados. Teste concorrente de dois escritores foi acrescentado após esse checkpoint.

**Ainda não garantido:** IDs persistentes ao mudar a ordem de carregamento, reidratação de saves antigos com IDs anteriores, compatibilidade visual real dos hooks JS e C# no Android e restrições de multiplayer. A compilação host **não substitui** o teste no aparelho.

## Lote — nomes no chão e raridades nas quatro famílias de itens (19/10)

- `NativeRarityGameplay.cpp` ganhou integração **condicional** com `Main.MouseTextInner(Main.MouseTextCache, bool)` e o overload nativo de `SpriteBatch.DrawString`. Ao detectar apenas um item de raridade C# sem tooltip de buff, sem HoverItem e sem dificuldade especial, aplica a cor cacheada ao mesmo texto que o Terraria desenha. O original preserva posicionamento, sombras, bounding e alpha, sem redesenho por JS.
- O hook resolve classe `MouseTextCache` como referência, campos `rare/cursorText/buffTooltip/diff`, `Main.HoverItem`, tipos de parâmetros e a ABI de `Color`, `Vector2` e `SpriteEffects`. Se algum layout não corresponder, **não instala**; a renderização original continua. A substituição de `DrawString` só altera a cor do texto identificado dentro do escopo do mouse text, sem chamadas MonoVM durante o draw.
- `BunnyRarityTextColor.h` implementa conversão de canais com alpha e deixa os desenhos de sombra escura inalterados. Os testes C++ verificam opacidade 0/128/255 e as regras de sombra.
- `SetRarity<T>()` foi ampliado a `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`, além de `BunnyItem`. O helper `BunnyRarityReference` recusa classes abstratas e tipos de outro assembly. O `CompiledAssemblyProbe` envia `ITEM_RARITY` para todas as quatro famílias.
- `SourceLoader` valida a propriedade, a identidade do registro e o ID no host. Para `BunnyItem` usa `BasicRare`; para arma, acessório e poção utiliza o próprio `NativeDefaults` para registrar `rare:i:<ID>` após validação. Rejeita descritores conflitantes que tentem definir `rare` duas vezes.
- Os fixtures C# agora cobrem item comum, acessório, arma com projétil e poção com buff. As ligações de tipo e linhas geradas são verificadas pela CI de host.

**Evidência anterior de host:** [CI #39321582431](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39321582431) passou depois do hook de desenho e da adição das APIs. O teste de todas as famílias foi incluído em commits posteriores e exige CI verde nesta revisão.

**Limite:** o hook de texto do chão tem fail-closed e ainda não está validado por captura ou uso em Android. Não substitui `Main.MouseText_DrawItemTooltip` nem integra ainda tooltips e chat completos. Também faltam persistência, migração e multiplayer. Nenhum APK foi gerado.

## Checkpoint — tooltip C#/JS, nomes e identidade estável (19/10)

- Hook C++ condicional de `Main.MouseText_DrawItemTooltip(Main.MouseTextCache, int, byte, int, int)` usa o `SpriteBatch.DrawString` nativo já instalado pelo texto no chão. Valida assinaturas de parâmetros, ABI do `Item.expert`, `Main.HoverItem`, método `Item.AffixName()` e retorno. Só entra quando o ID pertence a uma raridade C# com cor cacheada, dificuldade normal e sem flag Expert.
- O nome é calculado **uma vez por tooltip** usando o método nativo `AffixName`. Dentro do escopo do tooltip, `DrawString` compara apenas até **256 caracteres UTF-16**; não altera sombras escuras nem as demais linhas, e não chama o MonoVM durante a renderização. Sem correspondência exata, o desenho original permanece inalterado.
- O `TooltipLoader.js` original passa a consultar `bl.csharp.rarityColor(id)` **somente no caminho em que ele já está desenhando um tooltip JS** e nenhuma raridade JS possui esse ID. O bridge nativo retorna o RGB do cache atômico, ou `null` caso desconhecido. Isso preserva cores especiais e evita obrigar a execução de JavaScript em itens C# comuns.
- O `NativeRarityRegistry` inclui `identityForType` (índice direto), `stableKeyForType` e `typeForStableKey`, para recuperar uma raridade C# pela identidade `mod/classe` independentemente da sequência de carregamento. O hook de `Item.Prefix` substituiu a varredura linear por esse lookup direto.
- Cobertura de teste C++: nomes UTF-16 idênticos/diferentes, limites 0 e 257, nulidade, IDs mistos e resolução da chave estável. Teste Node executa o `TooltipLoader.js` real e confirma cores JS, C#, fallback, Master/Expert e cores vanilla sem modificar o desenho do usuário.

**Importante:** a chave estável passou a ser gravada e restaurada nos arquivos v3 (ainda sem QA Android). O formato `.bl` atual é versão 2; alterá-lo sem preservar os dados de baús e personagens antigos tem risco de perda de itens. Requer projeto de migração e testes de ida/volta antes de ligar o recurso.

**Não há validação Android** dos novos hooks IL2CPP, suas assinaturas reais, cores desenhadas ou 60 FPS. A ponte QuickJS foi verificada por teste de comportamento JS, não por build Android; fallback original permanece nos hooks.

## Lote — salvamento de raridades em personagens e baús (19/10)

- O `ModItemSaveFormat.h` original ganhou um **oitavo campo opcional** `rarityKey`, nomeado `mod/classe`. `SavedItem` mantém os campos anteriores, inclusive `prefixKey` e `data`.
- O leitor reconhece os cabeçalhos `bunnyloader itens 1`, `2` e `3`. Uma linha v3 inclui o oitavo campo de raridade, validado antes da aplicação. O gravador continua escrevendo **v2 sempre que não existem raridades C# persistidas**; escreve v3 somente para saves que precisam guardar essas identidades.
- Antes da primeira gravação v3 sobre um arquivo v1/v2, `ModItemSave.cpp` salva uma cópia imutável `<arquivo>.before-rarity-v3.bak`. Se não conseguir preservá-la, **aborta** a gravação v3. A gravação normal continua usando arquivo `.tmp` e `rename`.
- O save de personagem captura `Item.rare` nativo por ID atual e serializa a chave estável (também antes da rotina de save original, quando necessário). A carga resolve essa chave no registro C#, após restaurar os prefixos; na ausência da raridade ou da ABI `Item.rare`, mantém o default e conserva a identidade como pendência sem atribuir um ID incorreto. O retry após o mundo pronto tenta resolver identidades pendentes. Ao mudar a posição do item durante a carga, a pendência acompanha o destino.
- O mapa nativo `ChestData` existente armazena agora o campo `rarityKey` além do `data`, sem alterar o layout IL2CPP de `ChestItem`. Os hooks `SetToItem` e `ExpandItem` transferem a identidade entre itens e baús; `Resize`/`Clone` preservam a associação e `Clear`/`SetDefaults` a invalidam. O save v3 dos baús usa as mesmas regras e permite registros de itens vanilla com raridade C#.
- `item_save::mergePending` impede registros duplicados quando um item vivo e a pendência de prefixo/raridade compartilham a mesma posição/chave. Prefixo e dados de item mais recentes têm precedência.
- Testes nativos de host verificam v1/v2/v3, campos opcionais, identidade estável após troca na ordem dos mods, prefixos, dados e rejeição de chaves inválidas. O workflow compila `ModItemSave.cpp` com `-Werror`, além dos demais hooks.

**Limites:** nenhuma escrita real de saves Android foi executada; os testes não provam que arquivos de personagens e mundos reais preservam todos os estados. Mods JS continuam com seus registros próprios; o armazenamento de raridade neste lote é do **C# nativo**. Restam QA Android de backup/rollback, baús, inventário, prefixos, restauração com mod desabilitado, reabertura e multiplayer. A publicação e o APK permanecem bloqueados até a validação integral.

## Revisão de integridade pós-save v3 (19/10)

- O leitor v3 agora é **atômico** quanto às linhas: a primeira linha de item inválida invalida a leitura inteira, sem aceitar um subconjunto que perderia itens silenciosamente.
- `ModItemSave.cpp` valida o arquivo existente antes de **substituí-lo ou apagá-lo** no autosave; um v3 danificado é preservado para recuperação manual ou a partir do backup em vez de ser descartado.
- Adicionados testes de v3 danificado após uma linha válida, raridade de nome inválido e quantidade inválida. Os testes verificam que o vetor anterior não é modificado quando a leitura falha.
- `Item.Prefix` passa a identificar o ID pertencente ao C# pelo **registro nativo**, não pela existência prévia da cor cacheada. A raridade original é restaurada se o prefixo falhar, se o callback não estiver pronto ou se retornar um ID externo não registrado como raridade C#; IDs vanilla ainda são válidos.

**QA ainda necessário:** observar o comportamento no jogo com mods faltando, abrir e salvar personagens e baús reais, testar migração/rollback v1/v2/v3 e captura visual. O bloqueio do Android não se resolve apenas com teste C++ de host.

## Auditoria de segurança dos saves e prefixos (continuação, 19/10)

- A leitura de arquivos v3 é atômica: uma linha inválida rejeita a leitura completa; versões anteriores mantêm sua política antiga de leitura para compatibilidade. O vetor de registros anterior é preservado se houver erro.
- O autosave consulta `item_save::canReplaceExisting` antes de sobrescrever ou remover um sidecar. Se o arquivo existente estiver corrompido ou inacessível, nenhuma gravação é feita por esse caminho.
- A rotina `preserveLegacyBeforeRaritySave` foi isolada em `ModItemSaveSafety.h`, permitindo testar a cópia v2→v3 com arquivos reais em disco. O backup `.before-rarity-v3.bak` continua imutável após a primeira criação; arquivo v3 existente precisa estar íntegro.
- Os testes de host exercitam ida e volta de formatos, entrada v3 corrompida após uma linha válida, recusa de substituição, migração v2→v3 e verificação do backup real após a conversão.
- O hook nativo `Item.Prefix` preserva as raridades C# mesmo quando a amostragem da cor ainda não aconteceu. Usa a identidade registrada, não a presença do cache visual, restaurando o ID original se a tentativa de prefixo falhar ou o C# não responder. IDs de raridade externa não reconhecidos são recusados.

**Não equivale a validação Android:** ainda faltam testes no processo IL2CPP real, em personagens e baús, compatibilidade de mods desativados, restauração de backup e perfilamento de FPS. Não gerar APK nem promover a PR a release apenas com os testes de host.

## Verificação de migração em disco e escrita estrita (19/10)

- `ModItemSaveSafety.h` encapsula a inspeção do sidecar existente e a criação segura de `.before-rarity-v3.bak`. O runtime usa exatamente essas funções e seus testes compilam e executam como código C++ nativo.
- Um teste cria um arquivo v2 real em diretório temporário, confirma a cópia exata de backup, grava um v3, confirma que o backup não foi sobrescrito e demonstra a recusa de arquivo v3 com raridade inválida.
- O leitor dos formatos v1 e v2 continua tolerante para fins de compatibilidade de carregamento. A **validação antes de qualquer escrita** utiliza `read(..., strict=true)` para impedir que linhas inválidas antigas sejam apagadas silenciosamente. A mesma exigência vale para versões v3.
- Os testes também exercitam um v2 inválido no disco: o carregamento legado continua possível para registros válidos, mas seu arquivo original não pode ser substituído por um autosave sem revisão.

Isso é teste de integridade de arquivos no host, **não** confirmação de salvamento seguro no Android. É necessário ensaio real de personagens, baús, migração, desativação de mods e rollback antes de liberar qualquer APK.

## Lote — regressões de raridade pendente e proteção de backup (19/10)

- **Integridade v3:** o leitor exige oito campos em **todas** as linhas de `bunnyloader itens 3`. Um registro truncado sem a última coluna de raridade invalida o arquivo inteiro e não substitui uma leitura anterior.
- **Inventário e baús vanilla:** os campos pendentes de prefixo e raridade são filtrados **independentemente** durante o save. Se o jogador já ganhou outro prefixo, mas a raridade C# ainda não pode ser restaurada, apenas a chave de raridade é mantida. Se ambos já foram resolvidos, o registro pendente é descartado. Os testes C++ cobrem a junção sem duplicar o item nem reverter seu prefixo atual.
- **Backups antigos:** um `.before-rarity-v3.bak` preexistente precisa ser um arquivo de save v1/v2 válido e legível; diretórios, arquivos vazios ou corrompidos bloqueiam a conversão para v3, evitando confiar em backups inutilizáveis. O backup válido permanece intacto.
- **Logs otimizados:** a instalação nativa continua reavaliando os hooks a cada 60 ticks, mas avisos repetidos de ABI indisponível são limitados a um a cada 600 ticks para cada tentativa, reduzindo a poluição.
- Testes e validação compilada com `-Werror` continuam exclusivamente no host. Nenhum teste real em Android foi executado.

## Lote — integridade de prefixos vanilla e movimentação de raridade pendente (19/10)

- O carregamento de um item vanilla em **baú** com sidecar que guarda somente `rarityKey` não zera mais o prefixo nativo que já estava salvo no `.wld`. O helper `hasSavedVanillaPrefix` é usado tanto no fluxo do jogador quanto no baú, e testes de host exercitam prefixos existentes.
- A raridade C# pendente de um item no inventário agora também é associada à **referência do objeto**, além de `container/slot`. Um item movido cuja referência é preservada mantém a chave estável; o item que ocupou o slot antigo não herda indevidamente essa raridade.
- `NativeRarityPending.h` concentra seleção e reconciliação testáveis: rastreia mudança de posição, restauração completa e remoção do item. A rotina de `retryPendingManagedRaritiesInWorld` usa esse seletor e remove apenas os registros correspondentes ao item restaurado.
- Depois de salvar o personagem, as posições pendentes são recalculadas **apenas quando há pendências**, evitando varreduras desnecessárias. Uma raridade customizada já aplicada no item não é sobrescrita pelo retorno tardio do mod.
- Testes de host simulam dois itens do mesmo tipo, a substituição no slot antigo, mudança de posição, resolução e remoção, sem executar JavaScript ou fazer leitura de ponteiros antigos.

**Limite:** a identidade da referência é preservada quando o próprio `Item` permanece o mesmo; cópias profundas ou reconstruções de objetos por APIs nativas exigem QA Android. O rastreador não é uma identidade persistente para rede/multiplayer nem substitui o save `mod/classe`. Nenhum APK criado.

## Lote — confirmação de escrita antes de atualizar pendências (19/10)

- A gravação nativa `ModItemSave.cpp::writeFile` agora retorna sucesso/falha. A substituição por `.tmp` + `rename` e a remoção de sidecars vazios só informam sucesso após a operação efetiva, e continuam recusando arquivos danificados ou inacessíveis.
- A reconciliação dos mapas `g_pendingRarity` e `g_pendingRarityByItem` acontece **somente quando a escrita do sidecar do personagem retorna sucesso**. Uma falha ao abrir, copiar backup, gravar, renomear ou remover deixa as referências pendentes como estavam em memória, para não avançar o estado enquanto o arquivo permanece antigo.
- Mensagens `itens gravados` do personagem e do mundo agora dependem da confirmação da escrita. Os diagnósticos de falha específicos continuam disponíveis.
- Os testes de host do lote anterior validam movimento de itens, remoção, reconciliação e carregamento via selector original do Bunny. O arquivo de integração nativa é compilado com `-Werror`; o fluxo IL2CPP real ainda depende de QA no aparelho.

## Estado da implementação do RarityLoader C#

O código de host abrange os recursos de raridade do ExampleModBunny original:

- Descoberta de classes `BunnyRarity`, reserva de ID compartilhada entre C# e JS e registro gerenciado por `mod/classe`. `SetStaticDefaults` e `TypeOf<T>(modId)` são atendidos pelo worker C# após a associação dos IDs nativos.
- `SetRarity<T>()` para `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`, com aplicação do ID pelo host.
- Cache de cor C++ sem callback gerenciado no desenho, suporte a `Main.DiscoR/G/B`, tabela `ItemRarity.Initialize` e popup de coleta.
- `Item.Prefix` restaura raridade mesmo antes da amostragem da cor; atualização por multiplicador e mudança para raridade superior/inferior, com resultado validado pelo registro.
- Texto do item no chão e nome no tooltip através dos métodos de desenho originais do jogo, com validação de ABI e fallback nativo se incompatível. A tabela de cores também fornece a cor para etiquetas `[i:]` quando usada pelo Terraria.
- Persistência opt-in v3 para personagens e baús por chave estável, backups imutáveis dos formatos antigos, preflight estrito antes de sobrescrever e tratamento de raridades pendentes quando o mod não está carregado ou o item se move.
- As classes `ExampleModRarity` (cor 200, 215, 230) e `ExampleHigherTierModRarity` (Disco animado) estão nos **fontes jogáveis** de `samples/ExampleModCSharp/Playable`. As duas usam os mesmos cálculos e mudanças de tier do ExampleModBunny JS. Itens, espada, acessório, arma e poção do exemplo utilizam `SetRarity<T>()` sem recriar sprites ou efeitos.
- A CI compila **as cinco classes de conteúdo jogável com referência à raridade**, junto com as duas classes de raridade e as dependências reais do projétil, buff, receita e bancada, sem stub de item.

**Ainda não é aprovação no Terraria Android.** Bloqueios externos à implementação do host: confirmar assinaturas/hook chain IL2CPP e cores reais, testes de drops/chão/coleta/tooltips/chat, salva e reabre personagens/baús v1/v2/v3, desabilitação/reabilitação de mods, ID misto JS/C#, mudança de mundo, multiplayer, memória e 60 FPS. Não há prova de compatibilidade de saves v3 com Bunny Loader antigo, que precisa usar backup para rollback.

**Política:** o RarityLoader permanece em PR draft até o QA; não gerar APK ou publicar release enquanto os cinco loaders e o ciclo de validação Android não forem concluídos.

## Verificação segura (host)

```bash
dotnet run --project Split/Bunny-Loader/tests/rarity_loader/RarityLoaderHost.csproj --configuration Release
```

Nenhum benchmark de host substitui a validação no Terraria Android.


## Gate ARM64 Android — compilação de fontes sem APK (19/10)

A PR #102 agora executa também `.github/workflows/check-csharp-rarity-arm64.yml` com NDK 27.2.12479018, alvo `aarch64-linux-android26` e `-Wall -Wextra -Werror -fsyntax-only`. O teste compila os arquivos nativos reais `NativeRarityGameplay.cpp`, `ModItemSave.cpp`, `SourceLoader.cpp` e `AutoSourceRuntime.cpp`, sem empacotar binários, APKs ou mods.

- O primeiro workflow falhou por depender de `android-actions/setup-android@v3`, que tentou instalar o pacote SDK descontinuado `tools`. Foi substituído pela chamada ao `sdkmanager` já presente no runner.
- [ARM64 compile-only #39348230840](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39348230840): **aprovado**, commit `b812f8d5e60d5eba9b7dd0ba4d11563ef43996a2`.
- [Host RarityLoader #39348041745](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39348041745): **aprovado**, incluindo as guardas de metadados nulos de `Vector2` e `SpriteEffects`.

**Limite técnico:** `-fsyntax-only` garante compatibilidade de compilação com os cabeçalhos/ABI de destino ARM64, não confirma linkedição, carregamento da biblioteca no processo Terraria, assinaturas IL2CPP na build real nem execução de hooks. Permanecem obrigatórios os testes presenciais no Android de `ItemRarity.Initialize`, `Item.Prefix`, popup, tooltip, nome no chão, reentrada, Disco, JS+C# misto, inventário, equipamentos, baús/cofres, backup/rollback de sidecars `.bl`, mudança da ordem dos mods, multiplayer e 60 FPS. Usar apenas saves e mundos descartáveis quando for autorizada uma build de QA. PR segue draft; sem APK/`.bmod` e sem declarar paridade final.
