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
