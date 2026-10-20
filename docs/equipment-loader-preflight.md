# Pré-auditoria paralela — equipamentos C#

**Base:** o lote `loaders-02` do planejamento de 50 sistemas. Este trabalho é independente do grupo World Loaders (PR #98); não altera Tile, Wall, Tree, TileEntity, GlobalTile, MonoVM, Roslyn, SDK, runtime nativo, pacotes ou APKs.

## O que já pode ser reutilizado

| Loader | Exemplo original escolhido | Base C# já presente | Principal auditoria pendente |
| --- | --- | --- | --- |
| ItemLoader | `ExampleGun.js` | `ExampleGun.cs`, `BunnyItem` e `BunnyWeapon` | Ciclo de uso, holdout, disparo e hooks específicos do item. Uma arma compilada não demonstra todo ItemLoader. |
| EquipLoader | `ExampleBeard.js` | `ExampleBeard.cs`, `BunnyItemBehavior.EquipTexture` | Slots, camadas, textura funcional/vaidade, loadouts e restauração das tabelas nativas. |
| ArmorSetLoader | `ExampleHelmet.js`, `ExampleBreastplate.js`, `ExampleLeggings.js` | Três peças C#, `BunnyArmorSet` e policies nativas | Bônus somente com conjunto completo, imunidades, texto localizado, efeitos de frame, sombras, cabelo e desenho. |
| WingLoader | `ExampleWings.js` | `ExampleWings.cs` e `SetWingStats` | Velocidade vertical/horizontal, atualização da asa, gravidade invertida, referência por slot e animações. |
| PrefixLoader | `ExamplePrefix.js`, `ExampleAccessoryPrefix.js` | `ExampleMightyPrefix.cs`, `ExampleGuardedPrefix.cs`, `BunnyPrefix` | RollChance, CanRoll, escolha por categoria, efeitos aplicados, valores, tooltips e persistência da reforja. |

A referência dos exemplos é **o ExampleModBunny original** e a árvore de fonte do Bunny. Não usar tModLoader ou ExMod para copiar implementações nesse runtime. O documento apenas identifica contratos a revisar, não afirma que todos estão ausentes. API C# pública, policy compilada e callback efetivo de gameplay são três coisas diferentes.

## Verificador reproduzível

A partir da raiz de `Split/Bunny-Loader`:

```bash
python3 tools/equipment_preflight.py
python3 tools/equipment_preflight.py --json --output /tmp/equipment-preflight.json
python3 tools/equipment_contract_surface.py
python3 tools/equipment_contract_surface.py --json --output /tmp/equipment-contract-surface.json
python3 -m unittest discover -s tests -p equipment_preflight_test.py -v
python3 -m unittest discover -s tests -p equipment_contract_surface_test.py -v
```

O verificador lê `docs/csharp-loader-batches.json`, os cinco sistemas do grupo 2, todos os módulos JS atribuídos no plano, os oito arquivos dos exemplos originais, as oito fontes C# correspondentes e a referência pública compilada do SDK. Registra SHA-256 de cada fonte e confirma que os símbolos indicados existem nos exemplos JS. Não altera nem reescreve nenhum desses arquivos. Uma fonte ausente causa falha explícita.

**Limite importante:** localizar um nome de método em JS, uma classe em C# ou um arquivo na pasta não comprova equivalência funcional. A auditoria semântica hook a hook, os testes nativos com QuickJS desabilitado, os testes do exemplo e a validação Android ainda são obrigatórios antes de chamar qualquer um dos cinco loaders de completo.

## Testes futuros por categoria

1. **Item:** uso contínuo, munição, som próprio, disparo com direção sincronizada com a mira antes do tiro, coleta e clone vanilla; testar item por item e regredir com armas já existentes.
2. **Equip:** sprite vestido, sprite de vaidade, slots de back/shield/shoes, morte, respawn, troca de loadout, entrar/sair do mundo, saves e iluminação.
3. **ArmorSet:** três peças completas, peças misturadas, buffs imunizados, bônus numéricos sem duplicação, tela de inventário, texto e visual no mundo.
4. **Wing:** subida, queda, hover, gravidade invertida, mudanças de frame e velocidade horizontal; nenhuma chamada por quadro ao MonoVM se a policy nativa resolve o efeito.
5. **Prefix:** rolagem, reforja, preço, dano/defesa, categorias, tooltip, gravação no item e reentrada no mundo.

Todos os efeitos originais devem permanecer. Medir FPS/GC e reconhecer separadamente: *fonte presente*, *SDK compilado*, *contrato nativo instalado*, *teste host passou* e *validado no Android*. Nunca inferir 60 FPS do resultado do CI.

## Integração posterior

Este branch não altera o gate de publicação dos cinco loaders. Ao concluir o grupo World, aplicar somente a ferramenta e o documento de pré-auditoria, conferir hashes novamente e iniciar o grupo Equipamentos sem refazer a investigação dos arquivos-base. Nenhum loader fica marcado como completo automaticamente.

## Inventário dos callbacks originais

O segundo verificador lê os métodos de `ModItem.js` e `ModPrefix.js` diretamente dos fontes reais do Bunny e compara **somente os nomes** com a referência compilada do SDK C#. Seu foco atual são 35 callbacks selecionados dos cinco sistemas; a API inteira requer auditoria adicional.

Por exemplo, `BunnyPrefix.SetStats` e `ModifyValue` aparecem na referência C#, enquanto `RollChance`, `CanRoll`, `AllStatChangesHaveEffectOn`, `Apply` e `GetTooltipLines` não têm um método com o mesmo nome em `BunnyPrefix`. Isso não demonstra que não exista um serviço nativo correspondente: a próxima implementação deve conferir o host C++, o tratamento de prefixos no Terraria IL2CPP e os casos do ExampleModBunny. `WingUpdate` também não aparece como método público de nome idêntico nos tipos C# analisados, mesmo que o SDK exponha `SetWingStats`.

A ferramenta registra o número de linha e SHA-256 da fonte JS e do SDK. Se um callback original for renomeado ou um tipo público deixar de existir, o teste falha explicitamente. Nenhuma rotina altera o runtime, instala hooks ou afirma equivalência PC.

## Prefixos C# — melhoria nativa independente do pacote Tile

O `ManagedPrefixBridge` agora publica uma visão imutável de descritores após a validação e o registro completo. `ManagedPrefixGameplay` lê um `shared_ptr<const vector<ManagedPrefixEntry>>` atomicamente, sem copiar toda a lista de descritores nem bloquear `g_prefixMutex` a cada rolagem, cálculo de status, concessão de bônus ou tooltip. A política nativa de categoria, multiplicadores, reforja e localização continua igual.

`snapshotManagedPrefixes()` permanece disponível para chamadas de compatibilidade. O acesso otimizado por `managedPrefixView()` retém os dados anteriores até o último leitor terminar. Os novos descritores são validados e a nova visão é montada antes da publicação. A estrutura de prefixos existentes e os registros do loader original não são alterados.

A regressão C++ em `tests/prefix_v89/managed_prefix_snapshot_test.cpp` verifica views antigas, publicação em duas etapas, duplicação/rejeição, ordem de tipo e leitura concorrente. O teste original de tabelas frias em `tests/managed_prefix_cold_tables.cpp` também continua executado. A compilação host é um gate preliminar: o Android não foi medido, e esta alteração não implementa por si só `RollChance(item)` e `CanRoll(item)` dinâmicos do ExampleModBunny.

## Lote de regras C# por item: etapa nativa verificada

`BunnyPrefix.ConfigureRollRules(BunnyPrefixRollRules rules)` é executado uma vez pelo compilador gerenciado, sem JavaScript. `rules.SetWeight(itemType, weight)` ajusta a frequência de um prefixo para um tipo específico; `rules.Deny(itemType)` determina peso zero e `rules.Allow(itemType, weight)` oferece a forma equivalente com peso positivo. Os tipos não configurados continuam usando `RollWeight` original.

```csharp
public sealed class ExampleSelectivePrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Seletivo";
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 2;
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Allow(1, 3);
        rules.Deny(2);
        rules.SetWeight(512, 7);
    }
}
```

Regras são processadas na ordem de tipo e transportadas por um campo opcional (versão `PREFIX` de 17 colunas), preservando o formato anterior de 16. O `SourceLoader` valida tipos de 1 a 32767, pesos de 0 a 20, 32 regras no máximo por prefixo e proíbe IDs duplicados no wire. A política nativa respeita também as categorias e verificações de item já existentes; uma regra positiva **não** ignora categoria, munição ou consumo. A nova visão imutável anterior continua sendo usada.

**Limites importantes:** números de tipo de conteúdo modificado podem variar entre sessões e ordens de carregamento, portanto este primeiro contrato é mais seguro para tipos vanilla estáveis. Ainda não há resolução de nomes de itens de mod, nem execução arbitrária de `RollChance(item)` ou `CanRoll(item)` no MonoVM para cada instância. A implementação é um subconjunto declarativo de elegibilidade e chance por tipo, não paridade integral do callback do ExampleModBunny. Os testes de host não substituem validação Android.

Os testes `tests/prefix_v89/PrefixFixture/Content.cs`, `tests/prefix_v89/Program.cs`, `tests/managed_prefix_cold_tables.cpp` e `tests/prefix_v89/managed_prefix_snapshot_test.cpp` verificam serialização C#, retrocompatibilidade, seleção ponderada nativa, negação por item e rejeição de regras malformadas. Nenhum APK foi produzido automaticamente para esta etapa.

## Regras estáveis para itens do mesmo mod

`BunnyPrefixRollRules` também aceita o nome registrado da classe de item do próprio mod (`string`), além do ID numérico (`int`). Isso impede que uma regra para `ExampleGun` passe a apontar para outro item quando os IDs mudarem entre carregamentos.

```csharp
public override void ConfigureRollRules(BunnyPrefixRollRules rules)
{
    rules.Allow("ExampleGun", 3);
    rules.Deny("ExampleCharm");
}
```

As versões `Allow(string, int)`, `Deny(string)` e `SetWeight(string, int)` aceitam somente identificadores de classe com 1–64 caracteres ASCII (letras, dígitos e sublinhado, sem iniciar por dígito). O compilador mantém o formato opcional de 17 campos `PREFIX`, transportando entradas `ExampleGun:3`, além das entradas numéricas anteriores. O `SourceLoader` rejeita nomes malformados e a ponte nativa resolve `<modId>/<itemName>` via `modItemTypeByKey` **antes** de publicar as novas políticas.

Nomes inexistentes, colisão entre regra numérica e regra por nome resolvida para o mesmo tipo, peso fora de 0–20 ou mais de 32 regras provocam rejeição completa; nunca publicação parcial. A consulta em gameplay permanece exclusivamente por ID nativo resolvido, em visão imutável, sem busca textual nem chamada gerenciada por tentativa. A resolução por nome, nesta etapa, é deliberadamente limitada a itens do próprio mod; referenciar itens de terceiros exigirá um contrato separado de ordem de dependências/carregamento.

O teste host `tests/prefix_v89/managed_prefix_snapshot_test.cpp` cobre sucesso e falha de resolução, colisões e rollback. O teste managed de `PrefixFixture` cobre o serializador que combina `int` e `string` e mantém um prefixo legado com 16 campos.

## PrefixCategory.Custom — seleção explícita

A categoria `PrefixCategory.Custom` (valor 6) da API C# agora é validada e registrada pelo MonoVM/native. Assim como no `PrefixLoader.js` original, ela **não entra na lista natural** de `GetRollablePrefixes`. Para oferecer `CanRoll` e permitir aplicação direta/restauração em um item, exige uma regra de item explícita que resulte em permissão: peso positivo por padrão ou `SetCanRoll(item, true)` após declarar o peso. Um peso zero sozinho impede a aplicação; peso zero com permissão explícita permite aplicar diretamente, mas nunca adiciona o prefixo à rolagem natural.

```csharp
public sealed class ExampleCustomPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.Custom;
        DisplayName = "Especial";
        RollWeight = 1;
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Allow("ExampleGun", 1);
        rules.Deny("ExampleCharm");
    }
}
```

A lógica nativa consulta somente o mapa de tipos já resolvidos, sem JS nem callback Mono por reforja. Um pedido explícito permitido pode atravessar o caminho nativo de `CanRollPrefix`, `RollAPrefix` forçado e aplicação de status, mas a categoria nunca é acrescentada à rolagem natural. Isso é uma **interpretação declarativa restrita** dos contratos originais `CanRoll(item)`/`RollChance(item)`, não suporte a predicados C# arbitrários por instância. A integração real com um item que escolha um prefixo (`ChoosePrefix`) ainda requer suporte e validação separados.

Os testes de host cobrem um prefixo `Custom` gerenciado (5 classes compiladas), uma regra positiva por nome, uma negativa por tipo, impedimento de rolagem natural, aplicação direta do prefixo permitido e rejeição do não permitido. Não declarar suporte completo antes de validar persistência, multiplayer, gameplay e FPS no Android.

## Permissão independente do peso — CanRoll versus RollChance

O `ModPrefix.js` original define `CanRoll(item)` por padrão a partir de `RollChance(item) > 0`, mas permite que um mod sobrescreva os dois métodos separadamente. O C# agora representa essa diferença por duas decisões compiladas:

```csharp
public override void ConfigureRollRules(BunnyPrefixRollRules rules)
{
    rules.Allow("ExampleGun", 0);
    rules.SetCanRoll("ExampleGun", true);
    rules.Allow("ExampleStaff", 5);
    rules.SetCanRoll("ExampleStaff", false);
}
```

`ExampleGun` pode receber o prefixo pela rota explícita de `CanRollPrefix` e rolagem forçada, mas não aparece na rolagem aleatória (peso zero). `ExampleStaff` tem peso declarado 5, mas é recusado por `CanRoll=false`, inclusive na montagem dos candidatos naturais. `SetCanRoll` exige `SetWeight`/`Allow`/`Deny` anterior para o mesmo item e rejeita regras órfãs; `Deny(item)` limpa uma autorização anterior e mantém o bloqueio usual.

A extensão mantém os mesmos 16 campos antigos quando não há regras e o campo opcional 17 com duas formas compatíveis por item: `nome:peso` (sem autorização explícita, comportamento anterior) e `nome:peso:0`/`nome:peso:1`. IDs numéricos usam o mesmo protocolo. O parser nativo aceita somente permissão 0 ou 1; associa cada permissão a um item realmente declarado; resolve nomes próprios do mod antes de publicar a visão imutável.

**Escopo:** são políticas declarativas por tipo de item, não callbacks C# arbitrários por instância. O contrato não cobre estado de item, jogador, mundo, tempos ou RNG do método JS original. Nenhuma chamada ao MonoVM ou JavaScript é acrescentada no hot path. É necessário validar gameplay e FPS no Android antes de certificar o loader completo.

## PreferFor — primeira política ChoosePrefix nativa

O `PrefixLoader.js` original chama `ModItem.ChoosePrefix(item, rand)` e `GlobalItem.ChoosePrefix(item, rand)` antes da rolagem natural. O SDK C# agora expõe um subconjunto estático, preparado no MonoVM, `BunnyPrefixRollRules.PreferFor(int itemType)` e `PreferFor(string ownModItemName)`. A preferência pertence ao `BunnyPrefix` e não constitui um callback arbitrário do item:

```csharp
public override void ConfigureRollRules(BunnyPrefixRollRules rules)
{
    rules.Allow("ExampleGun", 0);
    rules.SetCanRoll("ExampleGun", true);
    rules.PreferFor("ExampleGun");
}
```

O método `PreferFor` só aceita itens já declarados em `Allow` ou `SetWeight` com `CanRoll` efetivamente positivo. Caso o peso seja zero, uma autorização separada `SetCanRoll(item, true)` é obrigatória. Uma chamada posterior a `Deny(item)` ou `SetCanRoll(item, false)` cancela a preferência. No wire opcional da linha `PREFIX`, preferência aparece como `item:weight:1:1`, mantendo `item:weight` e `item:weight:0/1` compatíveis.

`SourceLoader.cpp` valida o quarto campo e o `ManagedPrefixBridge` resolve os nomes do próprio mod para IDs da sessão. Durante o registro atômico, impede duas preferências para o mesmo item — inclusive entre mods C# já carregados. `ManagedPrefixGameplay::hookRoll` preserva a restauração de prefixo acima de tudo; caso não haja restauração, seleciona o prefixo preferido elegível antes de chamar o sorteio natural. Sem preferência, todo o caminho anterior continua. Não adiciona consulta gerenciada por reforja, apenas busca no snapshot imutável.

Os testes C# de `PrefixFixture` exercitam `PreferFor`, serialização, cancelamento e rejeição de itens não autorizados. Os testes C++ cobrem resolução por nome, exclusividade, rollback e a escolha real no hook nativo, inclusive peso natural zero.

**Limites:** esta é uma escolha declarativa por tipo, não a API `ModItem.ChoosePrefix(item, rand)` completa do ExampleModBunny; RNG customizado, `GlobalItem`, estado por instância, `PrefixChance`, `ApplyPrefix` e comportamento multiplayer não foram implementados nesta etapa. A verificação do SDK global pode apresentar diferenças relacionadas à PR de Tiles em paralelo; nenhum teste de host equivale a validar APK/FPS no Android.

## PrefixChance por item — política nativa de geração

O `PrefixLoader.js` original executa `ModItem.PrefixChance(item, pre, rand)` e `GlobalItem.PrefixChance(item, pre, rand)` no método `Terraria.Item.Prefix(int, out bool)`. Este lote adiciona a primeira variante **declarativa e exclusiva dos itens C#**, sem callback gerenciado por tentativa:

```csharp
public sealed class ExampleWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        Damage = 22;
        Shoot = "ExampleBolt";
        PrefixChance = true;
    }
}
```

`PrefixChance` é um `bool?` e pertence às classes `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`. `null` (padrão) não interfere no Terraria; `true` transforma o pedido de geração `-1` em `-2`, pulando a chance de sair sem prefixo; `false` rejeita tentativas de geração `-1` e `-2`. Os pedidos explícitos de tipo de prefixo continuam com o comportamento original.

O compilador gerenciado acrescenta somente quando há política um registro `PREFIX_CHANCE\tNomeDoItem\t0|1`, mantendo as linhas anteriores `ITEM`, `WEAPON`, `ACCESSORY` e `POTION` inalteradas. O SourceLoader valida a identidade e a unicidade do item já registrado. A ponte nativa publica o mapa imutável por tipo de item após verificar o hook. O método `bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)` é interceptado nativamente, com leitura do snapshot, sem bloquear a execução ou chamar o MonoVM por rolagem.

O caminho interno `applyManagedPrefixViaNativeRoll`, usado na restauração de prefixos salvos, conserva prioridade e **não é bloqueado** por essa política de geração. Isso evita perda de prefixo durante o carregamento de personagem. Nenhuma regra não configurada altera a geração vanilla.

Os testes `tests/prefix_v89/PrefixFixture` verificam serialização de `PrefixChance=false`. `tests/prefix_v89/managed_prefix_snapshot_test.cpp` valida duplicatas e snapshots imutáveis, enquanto `tests/managed_prefix_cold_tables.cpp` verifica os pedidos `-1`, `-2`, explícitos, sem política e de restauração.

**Limitação:** esta é a política estática por tipo, não os callbacks C# dinâmicos `PrefixChance(item, pre, rand)` com RNG, estado de instância ou composição com `GlobalItem`. `ApplyPrefix(item, pre)` não faz parte deste lote. A compilação host e a documentação não substituem validação de APK, multiplayer, saves reais ou FPS no Android.

## ApplyPrefix C# — efeitos de item após sucesso nativo

O `PrefixLoader.js` do Bunny chama `p.Apply(item)` e depois `ModItem.ApplyPrefix(item, prefix)`/`GlobalItem.ApplyPrefix(item, prefix)`, **somente após** `Terraria.Item.Prefix` confirmar aplicação e haver um prefixo diferente de zero; o pedido `-3` nunca aplica efeitos.

O SDK C# agora inclui `BunnyPrefixApplyStats` e o callback `virtual void ApplyPrefix(ref BunnyPrefixApplyStats effects)` em `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`. Nesta fase o MonoVM executa o callback na preparação do conteúdo e registra os dois bônus aditivos, sem chamar código gerenciado por reforja:

```csharp
public sealed class ExampleWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        Damage = 22;
        Shoot = "ExampleBolt";
    }

    public override void ApplyPrefix(ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = 4;
        effects.ValueBonus = 125;
    }
}
```

O compilador cria a linha opcional `PREFIX_APPLY\tExampleWand\t4\t125` apenas quando há bônus. `SourceLoader` exige que o item já esteja registrado e limita os bônus a `DamageBonus` entre 0 e 1000 e `ValueBonus` entre 0 e 100000, rejeitando duplicatas. A ponte nativa publica um mapa imutável por ID de item, independente de `PrefixChance`.

O hook `Item.Prefix(int, out bool)` chama primeiro a implementação anterior. Somente após sucesso, prefixo final não zero e pedido diferente de `-3`, soma os bônus a `Item.damage` e `Item.value` na thread do Terraria. Os dois campos são validados antes de qualquer escrita: números negativos e overflow de 32 bits anulam **ambas** as alterações. Sem política, o resultado vanilla fica intacto. Um prefixo salvo restaurado pela rota interna recebe o mesmo efeito de pós-aplicação; `PrefixChance=false` continua sem bloquear a restauração.

O contrato C# é **deliberadamente limitado**: o callback é amostrado na preparação do mod, sem objeto `Item`, ID de prefixo, RNG, mundo ou jogador. Não representa o `ApplyPrefix(item, pre)` dinâmico do JS, não compõe `GlobalItem.ApplyPrefix` e não executa ainda `BunnyPrefix.Apply(item)`. O efeito pós-aplicação depende do Terraria recalcular os atributos-base em cada chamada válida; testar no Android aplicações repetidas, saves e reforja antes de confirmar ausência de acúmulo ou paridade PC.

Os testes de host incluem execução real do override C#, wire de quatro colunas, registro nativo sem duplicatas, snapshots imutáveis, aplicação em item com e sem política, restauração, consulta `-3`, falha do original e proteção contra overflow. Nenhum resultado de teste de host implica estabilidade do APK, 60 FPS ou sincronização multiplayer.

## Aplicação repetida — fingerprint nativo limitado

O caminho `ApplyPrefix` agora guarda apenas as **16 aplicações recentes por thread**, sem heap, para reconhecer quando a mesma instância de item mantém exatamente o último tipo, ID de prefixo, dano e valor já ajustados. Antes de uma nova chamada válida a `Item.Prefix`, o hook remove **somente os próprios** bônus anteriores e deixa a rotina anterior trabalhar com os atributos normalizados. Após retorno bem-sucedido, reaplica uma única vez os bônus e renova a assinatura; em falha sem modificação de atributos, restaura o par anterior e mantém o fingerprint.

A detecção compara ponteiro da instância, tipo, prefixo, dano e valor; se qualquer um mudar, a assinatura é descartada para não descontar bônus de um estado externo desconhecido. O cache é circular e thread-local: não faz varreduras da lista de itens em jogo, não cresce sem limite e não introduz comunicação com JavaScript/MonoVM no hook de reforja.

**Limite:** esta é uma mitigação de acúmulo para reaplicações recentes com atributos correspondentes, não um sistema de identidade persistente para itens. Após expulsão do cache, recriação do objeto ou mudanças externas que imitarem a assinatura anterior, não há garantia universal de idempotência. A validação de itens reais, clones, troca de mundo, reforjas repetidas e multiplayer no Android ainda é obrigatória.

O teste host usa uma implementação de `Item.Prefix` que **não reseta** dano/valor e comprova quatro reaplicações seguidas sem acumular bônus, falha/restauração, alternância entre instâncias e invalidação após alteração externa. Há ainda regressão independente de overflow em valor alto. A PR temporária [#104](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/104) executou o CI específico [#39317397028](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39317397028) com sucesso; as correções e testes foram copiados para a #100.

A suíte global associada ao ramo de Tiles ainda falha em `tests/basic_items/Program.cs` por `expected=13 actual=20` e em `ManagedRuntime.cpp` por avisos `-Werror=misleading-indentation`. Esses arquivos não foram alterados neste lote.

## Verificação cruzada Android ARM64 do PrefixLoader

A pipeline isolada `.github/workflows/check-csharp-prefix-android-arm64.yml` compila os arquivos de produção `ManagedPrefixBridge.cpp`, `ManagedPrefixGameplay.cpp` e `SourceLoader.cpp` usando o NDK 30, clang++ C++20, target `aarch64-linux-android26`, `-Wall -Wextra -Werror` e `-fPIC`, com passes separados de verificação sintática e objetos nativos. `llvm-readelf` confere que cada objeto tem arquitetura AArch64. Não requer assets comerciais, segredos, nem publica binários privados ou APK em artifacts públicos.

A PR de verificação temporária [#105](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/105) executou [Android ARM64 #39319100681](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39319100681) com **sucesso**, assim como [Equipment CSharp preflight #39319100726](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39319100726), incluindo delegação vanilla quando o hook ainda não inicializou. O workflow e a regressão foram copiados para a PR #100.

**Não confundir com teste Android real:** somente três unidades de tradução foram compiladas para ARM64 e verificadas por `llvm-readelf`. Não ocorreu link da biblioteca compartilhada completa, instalação do APK, execução no Xiaomi, verificação de saves, multiplayer ou medição de FPS. A construção de APK completa deve respeitar a entrega privada existente e os bloqueios de lote da base do projeto.

## Prefixo com Apply, eficácia de stats e tooltip adicional

Conferido com `script/js/mod/ModPrefix.js` e `script/js/mod/Loaders/PrefixLoader.js`: o prefixo pode executar um `Apply(item)` depois de `Item.Prefix`, informar quando todos os multiplicadores afetam realmente o item e acrescentar linhas de tooltip. O contrato C# atual introduz uma versão **declarativa e tipada** desses três comportamentos, sem dependência do runtime JS.

```csharp
public sealed class TestMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 5;
        ExtraTooltip = "Bonus adicional do prefixo";
        RequireEffectiveStats = false;
    }

    public override void Apply(ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = 3;
        effects.ValueBonus = 40;
    }
}
```

O compilador chama uma vez `Apply(ref BunnyPrefixApplyStats)` e envia a linha opcional `PREFIX_POST\tName\t3\t40\t0\tBonus adicional do prefixo`. O parser valida o nome de um prefixo do próprio mod, limites, comprimento, duplicatas e rejeita linhas órfãs antes de publicar o descritor. Prefixos sem políticas adicionais mantêm seu formato antigo de 16/17 campos. Os limites são 0–1000 para dano, 0–100000 para valor e 120 caracteres para tooltip.

O hook de `Item.Prefix` soma os efeitos do prefixo e do item original somente após sucesso, com checagem de overflow antes de escrever os dois atributos. A assinatura thread-local de 16 slots reconhece **o total aplicado**, para evitar somar repetidamente a combinação dos bônus. `RequireEffectiveStats=false` dispensa a condição nativa de que todos os multiplicadores alterem de fato o item; a opção padrão `true` mantém a regra anterior. `ExtraTooltip` é anexado após as linhas vanilla no hook de tooltip do inventário, respeitando capacidade, tipos IL2CPP e write barrier.

**Não é paridade integral com os callbacks originais por instância:** `Apply(item)`, `AllStatChangesHaveEffectOn(item)` e `GetTooltipLines(item)` no JS podem acessar estado dinâmico, alterar outros campos ou retornar várias linhas; o novo contrato C# limita-se a ajustes estáticos de dano/valor, uma linha fixa e um sinalizador booleano. Não substitui validação no Android (itens clonados/salvos, multiplayer, GC, tooltips e FPS). Não declarar o PrefixLoader integralmente completo com base apenas nos testes de host.

## AllowPrefix C# — bloqueio por item de prefixos vanilla e próprios

O `PrefixLoader.js` do Bunny chama `ModItem.AllowPrefix(item, pre)` e `GlobalItem.AllowPrefix(item, pre)` para filtrar opções. O C# oferece agora um subconjunto **declarativo por tipo**, `ConfigurePrefixRules(BunnyItemPrefixRules)`, em `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`. Ele é compilado uma vez e aplicado pelo C++ nativo em `GetRollablePrefixes`, `CanRollPrefix` e aplicações explícitas em `Item.Prefix`:

```csharp
public override void ConfigurePrefixRules(BunnyItemPrefixRules rules)
{
    rules.Deny(71);
    rules.Deny(nameof(ExampleGuardedPrefix));
}
```

`Deny(int)` aceita IDs 1–255, e `Deny(string)` aceita somente prefixos **do próprio mod**, registrados e resolvidos por nome estável. Cada item aceita até 32 bloqueios. Os dois tipos de nome são serializados em `PREFIX_DENY\tItemName\t71,ExampleGuardedPrefix`, preservando as linhas de itens e prefixos antigas. Fonte e compilador rejeitam IDs fora do limite, nomes externos, referências inexistentes, duplicatas e políticas inconsistentes. A ponte nativa faz resolução atômica dos IDs e publica snapshots imutáveis.

O hook `GetRollablePrefixes` remove prefixos bloqueados das opções vanilla e adicionais, mantendo a distribuição dos demais. Caso tudo seja bloqueado, retorna lista vazia; na rolagem somente vanilla com política, a seleção nativa respeita a lista filtrada. `CanRollPrefix` e pedidos explícitos também rejeitam o prefixo bloqueado. **Restauração de um prefixo salvo** pela rota interna é preservada. Políticas ausentes não alteram a rolagem normal. Falhas de alocação da nova lista com bloqueios ativos evitam retornar candidatos que foram proibidos.

A suíte C# compila `TestWand` com uma negação vanilla e outra por nome, enquanto os testes C++ conferem resolução por nome, publicação sem duplicatas, rollback, remoção da lista de prefixos, `CanRoll`, tentativa explícita e bypass de restauração. O `ExampleWand` jogável oferece a mesma API com `ExampleGuardedPrefix`.

**Escopo real:** não equivale a `AllowPrefix(item, pre)` dinâmico com estado da instância, nem implementa composição de `GlobalItem` e hooks próprios de outros mods. A verificação host/ARM64 não confirma os dados reais do Terraria, saves/GC, netcode nem FPS.

## Predicados nativos avaliados por instância (dano e mana)

O `BunnyPrefix.CanRoll(item)` e o `RollChance(item)` originais recebem o item atual. O SDK C# ganhou o subconjunto nativo `BunnyPrefix.ConfigureItemConditions(BunnyPrefixItemConditions)`, para declarar limites que o hook verifica **no objeto IL2CPP real em cada tentativa**, sem chamada síncrona ao MonoVM:

```csharp
public override void ConfigureItemConditions(BunnyPrefixItemConditions conditions)
{
    conditions.RequireDamageBetween(10, 25);
    conditions.RequireManaBetween(0, 8);
}
```

`RequireDamageBetween` aceita extremos de 0 até 100000, `RequireManaBetween` de 0 até 10000, com mínimos não superiores aos máximos. Condições não declaradas não alteram os callbacks nativos. O compilador chama a configuração somente na preparação do mod e envia o registro opcional `PREFIX_CONDITION\tNomePrefixo\t10\t25\t0\t8`. O SourceLoader valida IDs, limites e unicidade; os campos entram no mesmo descritor imutável de prefixos. `eligible(item,prefix,...)` lê `Item.damage` e `Item.mana` em tempo real e filtra `CanRollPrefix`, `GetRollablePrefixes`, rolagem preferencial e aplicação de prefixos C# conforme suas regras anteriores.

Os testes C# verificam os valores do descritor e os limites inválidos, e os testes C++ exercitam dano 9/12/26 e mana 0/8/9 no mesmo item. Não existe varredura de itens, heap por reforja ou callback MonoVM nesse caminho.

**Limite:** trata-se de um predicado nativo configurável em C#, não de execução arbitrária de `CanRoll(item)` ou `RollChance(item)` no MonoVM. O callback gerenciado completo precisaria de uma ABI estável de snapshots, agendamento do worker, política de timeout, tratamento de concorrência e validação Android. Sem isso, não afirmar paridade integral, saves ou 60 FPS comprovados.

## Restauração de saves e bônus de acessórios após mudança de condições

As condições `ConfigureItemConditions` limitam **novas** rolagens, não a existência de um prefixo já atribuído. A rota de restauração `ModItemSave` usa `applyManagedPrefixViaNativeRoll` e, em casos limitados, `applyManagedPrefixStatFallback`. Esses caminhos não devem rejeitar um prefixo previamente salvo porque dano ou mana mudaram. A restauração continua exigindo compatibilidade estrutural de categoria; para prefixos `Custom`, é necessária autorização explícita do item (`CanRoll=true` ou peso positivo). Prefixos de arma não podem ser restaurados em acessórios. Os hooks `RollAPrefix`, `CanRollPrefix` e `TryGetPrefixStatMultipliersForItem` agora reconhecem explicitamente o escopo de restauração e permitem o descritor registrado sem usar a elegibilidade de novas rolagens; o filtro de prefixos negados permanece obrigatório para novas aplicações.

Da mesma forma, `hookGrant` de acessórios consulta o prefixo **já equipado**, não sua elegibilidade para sorteios novos. Um bônus `ApplyAccessoryEffects` não deve desaparecer apenas porque o item equipado mudou de atributos após a aquisição do prefixo.

Os testes host exercitam prefixo salvo 99 em item com dano 9/mana 9 (fora dos limites atuais 10–25 e 0–8) e verificam que novas rolagens o rejeitam, mas a restauração e os multiplicadores nativos continuam válidos. Um acessório com prefixo atribuído recebe seu bônus de defesa mesmo quando uma condição posterior bloqueia novas rolagens. Restauração de objetos Android, save real e multiplayer ainda precisam de QA no dispositivo.

### Regressão de restauração — CI

A PR temporária [#110](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/110) confirmou [Equipment preflight #39331584562](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39331584562) e [NDK ARM64 #39331584571](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39331584571), ambos **aprovados**. O teste diferencia prefixo salvo com dano 9/mana 9, restauração inválida de prefixo de arma em acessório, prefixos `Custom` sem autorização e preservação dos efeitos de acessórios equipados. A solução permanece restrita ao escopo de restauração; novas rolagens continuam obedecendo aos filtros e aos valores do item.


## GetTooltipLines — múltiplas linhas C# estáticas

Além de `ExtraTooltip`, os prefixos C# aceitam `AddTooltipLine(string text)` durante o `SetDefaults`. A API permite até **quatro linhas adicionais**, com até 120 bytes UTF-8 cada, sem caracteres de controle. O compilador C# chama `SerializeTooltipLines()` na preparação do mod e acrescenta a linha opcional `PREFIX_TOOLTIP\tNomeDoPrefixo\tHEX1,HEX2,...`, preservando `PREFIX_POST` e todos os descritores anteriores.

```csharp
public override void SetDefaults()
{
    ExtraTooltip = "Bonus adicional do prefixo";
    AddTooltipLine("Dano extra do prefixo");
    AddTooltipLine("Valor adicional");
}
```

O SourceLoader valida propriedade do prefixo, quantidade, tamanho, hexadecimal maiúsculo e caracteres de controle. As linhas passam ao snapshot imutável do prefixo. `hookInventoryTooltip` chama o original antes, inclui `ExtraTooltip` e depois cada linha adicional, respeitando o menor tamanho entre os arrays `lines/modifier/bad` e usando `gc_wbarrier_set_field` no IL2CPP. Nenhuma chamada ao MonoVM acontece durante o desenho.

Os testes de host verificam o wire UTF-8, rejeição atômica de entradas excessivas, três linhas inseridas corretamente com flags de prefixo e capacidade final do array. O recurso cobre o texto **estático**; a implementação de `ModPrefix.GetTooltipLines(item)` que calcula múltiplas linhas arbitrárias a partir da instância ainda não está disponível. Android real, idioma local, saves e FPS continuam pendentes.

## Prefixos misturados com outros mods — seleção com pesos

O loader JS original mantém prefixos vanilla e de outros mods na lista de candidatos, aplicando `RollChance(item)` para os seus prefixos e peso padrão 1 para os demais. A implementação C# não pode devolver a rolagem ao jogo apenas por detectar um prefixo externo, porque isso ignora os pesos C# e as políticas `AllowPrefix` do item. `hookRoll` agora preserva os candidatos externos e vanilla dentro do array que já foi filtrado por `hookRollable`, com uma chamada à seleção nativa ponderada para os candidatos gerenciados. A decisão não introduz chamadas ao MonoVM no caminho quente.

O teste C++ injeta uma lista `vanilla 71, externo 100, C# 98` com peso C# de 3; a seleção nativa percorre `71,100,98,98,98` por valores determinísticos de `NextDouble()`. Quando `Deny(71)` está ativo, a lista passa a `100,98,98,98`; nenhuma das seleções volta ao sorteio vanilla original. A rota original ainda é preservada para listas sem prefixo C# e sem filtro de item.

Este comportamento foi testado em host, não garante paridade com hooks dinâmicos `GlobalItem` de mods terceiros, não executa C# por reforja e não foi validado em Android com mods externos reais.

## AllowPrefix por instância via worker (20 de outubro)

A PR #100 agora integra `BunnyItemPrefixRuntime.Dispatch` aos hooks nativos para os quatro tipos de item C#, somente quando a classe sobrescreve o método de snapshot. `ITEM_PREFIX_CALLBACK` é validado por owner/tipo/máscara, publicado como mapa imutável e vinculado ao assembly depois dos defaults. O worker é aquecido antes do uso, com protocolo de prazo e descarte de resposta atrasada. O hook observa veto declarativo primeiro e, após isso, permite até quatro RPCs de 650 µs cada dentro do orçamento total de 2500 µs por transação; os resultados são cacheados por prefixo e campos atuais do item. A regra de restauração gerenciada existente bypassa esses filtros.

O caminho vanilla não usa o seletor ponderado quando não há veto real nem prefixo gerenciado na lista, preservando o RNG original. A fixture de hooks em C++ valida aceitação, veto, alteração de estado, repetição de consulta, falha do worker, limite de chamadas, tentativa explícita, restauração e rolagem vanilla. O CI verifica SDK/host e objetos ARM64, não uma build Android assinada ou gameplay. A PR permanece draft, sem merge com Tile #98 e sem publicação.
