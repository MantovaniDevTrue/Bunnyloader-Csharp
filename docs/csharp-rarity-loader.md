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

## Bloqueios para declarar o loader completo

1. Concluir o handoff dos IDs nativos para as instâncias `BunnyRarity` no worker, inicializando `SetStaticDefaults` no momento correto, e validar a ponte compartilhada entre mods C# e JS em Android e persistência.
2. Aplicar `Rare` de armas, acessórios e itens às novas identidades; suportar `TypeOf` cruzado e persistência/reidratação sem perda de ID.
3. Registrar cores de raridade em `Terraria.GameContent.UI.ItemRarity.Initialize` no C++/IL2CPP, incluindo chamadas de reinicialização.
4. Implementar `Item.GetPopupRarityColor` para textos flutuantes e `Item.Prefix` com out/ref, multiplicadores e caso `Prefix(-1)` que não pode voltar a roxo.
5. Implementar nome de item no chão em `Main.MouseTextInner` com enquadramento e alpha equivalentes ao mobile e nome colorido em tooltips, sem alterar a lógica vanilla.
6. Dar suporte a cores animadas do `Main.DiscoR/G/B` sem MonoVM bloqueante por draw; estabelecer a periodicidade de atualização e orçamento de renderização.
7. Portar os dois exemplos originais com assets e um item utilizável, cobrindo prefixos bons/ruins, chão, coleta, tooltip e etiqueta de chat `[i:]`.
8. Validar ABI, ciclo de carregamento, troca de mundo, descarregamento, mais de um mod, saves e multiplayer; rodar CI nativo e testes reais de Android, verificar fidelidade visual, memória e 60 FPS.

**Regra de entrega:** nem esta etapa nem um teste de host aprovado contam como `RarityLoader` completo. A política de cinco loaders completos com exemplo original e validação Android antes de APK permanece. Não modificar workflow de build nem disparar `workflow_dispatch` de APK.

## Verificação segura (host)

```bash
dotnet run --project Split/Bunny-Loader/tests/rarity_loader/RarityLoaderHost.csproj --configuration Release
```

Nenhum benchmark de host substitui a validação no Terraria Android.
