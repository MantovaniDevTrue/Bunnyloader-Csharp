# API C# suportada: Bunny Loader Roslyn + MonoVM

Fonte principal de assinaturas: [BunnySourceScriptApi.cs](../../../experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs). O ExampleModBunny é a referência de estrutura e comportamento, não as classes do tModLoader.

| API do Bunny | Arquivo do ExampleModCSharp | Prova no Android |
| --- | --- | --- |
| BunnyWeapon.SetDefaults | ExampleWand.cs, ExampleWaveWand.cs | duas armas registradas na .72/.73 |
| BunnyProjectile.SetDefaults | ExampleBolt e ExampleWaveBolt | dois tipos ativos na .72/.73 |
| ProjetoMotion.TurnAtTick(int) | ExampleBoltMotion e ExampleWaveBoltMotion | duas curvas independentes, 120 amostras |
| BunnyProjectile.OnHitNPC / OnKill | ExampleBolt, efeito nativo OnFire 24 | efeitos e callbacks preservados |
| BunnyLiveEvents.OnHitNPC(int,int) / OnKill(int,int) | Common/Live/BunnyLiveEvents.cs | callback primário .72/.73 |
| ProjetoLiveEvents.OnHitNPC(int,int) / OnKill(int,int) | Common/Live/ExampleWaveBoltLiveEvents.cs | callback independente .73: 879/879 |
| BunnyBuff, BunnyBuffPotion, BunnyAccessory, BunnyPlayer, BunnyNPC | Content/ e Common/Players | exemplos presentes, validação específica por API |
| BunnyPlayer.MoveSpeedPercent e UpdateEquips | Common/Players/ExampleSwiftPlayer.cs e Content/Items/Accessories/ExampleSwiftAccessory.cs | **experimental**, +8% velocidade enquanto equipado (efeito nativo sem callback MonoVM por quadro) |
| BunnyPlayer.OnEnterWorld / ResetEffects / UpdateEquips | ExampleWelcomePlayer, ExampleFocusPlayer | .90.16 em validação no Android; retorno C# de OnEnterWorld virá como aviso na thread do jogo |
| BunnyPlayer.UpdateMovement(bool) | ExampleMomentumPlayer | .90.16 em validação; +6% aceleração nativa com acessório equipado |
| BunnyPlayer.FrameEffects(bool) | ExampleShadowPlayer | .90.16 em validação; sombra do jogador enquanto equipado |
| BunnyLiveNpc.AI(int,int) | Common/Live/ExampleLiveNpcAI.cs | protótipo experimental, não AI por frame |
| BunnyPrefix.SetDefaults / SetStats / ModifyValue / ApplyAccessoryEffects / ConfigureRollRules / Apply / ExtraTooltip / BunnyItemPrefixRules.Deny / AddTooltipLine | Content/Prefixes/ExampleMightyPrefix.cs, ExampleGuardedPrefix.cs | CI nativo C# e objeto ARM64 aprovados; **gameplay Android ainda não validado** |

## Tooltips adicionais do PrefixLoader C#

O ExampleMightyPrefix demonstra também `AddTooltipItemLine("Dano atual: {damage}, mana: {mana}, pilha: {stack}")`. Diferente de `AddTooltipLine` (estático), o compilador C# gera `PREFIX_TOOLTIP_ITEM` com um template limitado a 120 bytes UTF-8 e apenas as três variáveis nativas `{damage}`, `{mana}` e `{stack}`. O loader valida as variáveis e o número combinado máximo de quatro linhas adicionais; o hook nativo `Item.GetTooltipLines` renderiza a linha a partir dos atributos da instância atual. Em caso de offset nativo indisponível, apenas a parte dinâmica não é adicionada. O template não precisa do worker MonoVM; o overload `GetTooltipLines(BunnyPrefixItemSnapshot)` usa outro caminho opt-in com cache, e ambos dispensam QuickJS.

O `ExampleMightyPrefix` demonstra `ExtraTooltip` mais `AddTooltipLine` para acrescentar linhas estáticas ao inventário. O C# registra até quatro linhas UTF-8 durante `SetDefaults`; os hooks nativos do Terraria as desenham sem executar MonoVM no frame. Este é o subconjunto declarativo, não `GetTooltipLines(item)` dinâmico. Gameplay Android, FPS e multiplayer permanecem sem validação.

## Filtro de prefixos por item no C#

O exemplo jogável `ExampleWand.cs` usa `ConfigurePrefixRules(BunnyItemPrefixRules)` para bloquear o prefixo vanilla ID 71 e o prefixo local `ExampleGuardedPrefix`. A política é resolvida no carregamento e aplicada nos hooks nativos, não invoca o MonoVM por reforja. Funciona como um subconjunto estático de `AllowPrefix`; não substitui os callbacks dinâmicos por instância do loader JS. CI C#/C++ e compilação ARM64 não equivalem a testes no dispositivo.

## Condições atuais por instância no PrefixLoader C#

O exemplo `ExampleMightyPrefix` usa `RequireDamageBetween`, `RequireManaBetween` e `RequireStackBetween(1, 10)`. O último é avaliado nativamente em `Terraria.Item.stack` a cada tentativa de rolagem, sem chamada síncrona à MonoVM. O descritor `PREFIX_CONDITION` de 6 campos permanece válido; os novos registros de 8 campos incluem os limites da pilha. A condição regula novas rolagens, não invalida prefixos recuperados de saves anteriores. Trata-se de condição declarativa por estado do item, não de `CanRoll(item)` C# arbitrário. Não houve teste de gameplay Android neste lote.

## Peso de prefixo conforme a pilha do item

O `ExampleMightyPrefix` também demonstra `OverrideRollWeightWhenStackBetween(3, 5, 6)`. Uma pilha com 3 a 5 unidades ganha peso nativo 6; fora do intervalo, continua valendo o peso padrão do prefixo ou o peso configurado por tipo de item. Um peso zero desativa a rolagem automática enquanto a condição se aplica, mas um `SetCanRoll` explícito continua tendo prioridade para aplicação direta. O contrato `PREFIX_STACK_WEIGHT` opcional é separado do `PREFIX_CONDITION`; o formato antigo segue aceito. A publicação do descritor é imutável e não exige callbacks MonoVM durante a reforja. É um subconjunto declarativo de `RollChance(item)`, não execução arbitrária de C#.

## Callbacks gerenciados de prefixo no worker (experimental)

O Bunny Loader C# dispõe de `BunnyPrefix.CanRoll(BunnyPrefixItemSnapshot item)` e `BunnyPrefix.RollChance(BunnyPrefixItemSnapshot item)`, com métodos executáveis exclusivamente pelo novo `BunnyPrefixRuntime.Dispatch`. O snapshot imutável inclui tipo, dano, mana, quantidade, prefixo atual e flags de acessório, consumível e distância. O valor nativo de dano `-1` é aceito exclusivamente para itens sinalizados como acessório; os demais campos negativos continuam inválidos. O endpoint `invokeManagedPrefix` reutiliza a fila MonoVM existente: limite de fila, espera de no máximo 3 ms (padrão 1,5 ms), cancelamento por prazo e descarte de resultados tardios. As instâncias C# são preparadas no worker antes das chamadas. O `ExampleGuardedPrefix` demonstra callbacks de elegibilidade, chance, eficácia, bônus e tooltip. `GetTooltipLines(BunnyPrefixItemSnapshot)` recebe dados imutáveis e devolve até quatro `BunnyPrefixTooltipLine`, cada uma com texto (até 120 bytes UTF-8) e flag negativa. O resultado usa wire hexadecimal estritamente validado; o inventário consulta o worker no máximo uma vez a cada 200 ms por snapshot e mantém as linhas estáticas quando o retorno não chega. As linhas dinâmicas são inseridas no inventário e no painel de reforja, até a capacidade nativa dos respectivos arrays. Cores e metadados especiais do JavaScript original ainda não são equivalentes nesta API.

**Importante:** além de `CanRoll` e `RollChance`, o worker aceita `AllStatChangesHaveEffectOn(BunnyPrefixItemSnapshot)` e `Apply(BunnyPrefixItemSnapshot, ref BunnyPrefixApplyStats)`. A eficácia de estatísticas rejeita apenas resultados `false` recebidos a tempo, e `Apply` acrescenta bônus não negativos ao resultado nativo com validação de limites e proteção contra acumulação nas próximas reforjas. O overload `Apply(ref BunnyPrefixApplyStats)` continua existindo como política compilada. Nesta fase `hookRoll` e `hookCanRoll` integram os overrides C# opt-in, somente após warmup antecipado. O intervalo máximo por tentativa é 2,5 ms, até quatro RPCs distintas, com limite de 650 µs cada e cache por prefixo; problemas de fila, timeout e resultados inválidos mantêm as regras declarativas originais, sem passar ponteiros IL2CPP para a MonoVM. A restauração de prefixos salvos não executa callbacks. O CI não substitui gameplay no Android, FPS ou multiplayer.

## Peso de prefixo por dano ou mana atuais

`BunnyPrefixItemConditions` também aceita `OverrideRollWeightWhenDamageBetween(min,max,weight)` e `OverrideRollWeightWhenManaBetween(min,max,weight)`. O `ExampleGuardedPrefix` demonstra mana 0–8 com peso nativo 4. O worker emite `PREFIX_ATTRIBUTE_WEIGHT` opcional, indicando atributo `D` ou `M`; o loader valida o descritor e o gameplay usa diretamente `Item.damage` ou `Item.mana` da instância. Fora do intervalo, prevalece o peso do tipo de item ou o peso padrão. Peso 0 impede a seleção ponderada no intervalo. Apenas **uma** substituição dinâmica de peso é permitida por prefixo: pilha, dano **ou** mana, evitando conflitos. A restauração de prefixos salvos preserva o fluxo separado. Sem callbacks MonoVM síncronos durante a reforja.

## Bloqueio condicional por instância — AllowPrefix C#

O `ExampleWand` usa `BunnyItemPrefixRules.DenyWhenStackBetween(72, 3, 5)`: o prefixo 72 é vetado apenas quando a pilha contém entre 3 e 5 unidades. A mesma API aceita um nome de prefixo do próprio mod; nomes externos não são resolvidos por atalhos. O compilador emite `PREFIX_DENY_STACK` somente quando há regras desse tipo. `ManagedPrefixBridge` valida o mapeamento de cada prefixo e publica um snapshot imutável; os hooks de `GetRollablePrefixes`, `CanRollPrefix`, `RollAPrefix` e `Item.Prefix` verificam o estado atual do item em C++ sem MonoVM. Bloqueios permanentes existentes continuam ativos. Prefixos salvos previamente não são invalidados por essa condição mutável. O contrato é declarativo, não o callback arbitrário `GlobalItem.AllowPrefix(item,prefix)` do JS.

## AllowPrefix por dano e mana atuais

A API `BunnyItemPrefixRules` agora expõe `DenyWhenDamageBetween(prefix, min, max)` e `DenyWhenManaBetween(prefix, min, max)` com overload para ID numérico (1–255) ou nome local de prefixo. Os valores de dano (0–100000) e mana (0–10000) vêm do `Terraria.Item` real no instante da seleção. Uma mesma identidade pode ter regras de ambos os atributos; cada uma bloqueia independentemente (OU). O Roslyn emite `PREFIX_DENY_ATTRIBUTE` opcional; o bridge resolve IDs na inicialização, valida duplicatas e publica snapshot imutável. Em runtime o filtro consulta apenas offsets IL2CPP, não há chamada síncrona MonoVM nem varredura por inventários. Se o offset de um atributo necessário estiver indisponível, somente aquela política falha fechada. Prefixos já aplicados e a restauração de saves continuam preservados. São condições declarativas; callbacks C# arbitrários não são executados na thread de reforja.

## ChoosePrefix condicional por pilha

`BunnyItemPrefixRules.ChooseWhenStackBetween(prefix, min, max)` permite ao item escolher um prefixo (ID vanilla ou nome local) quando o seu `stack` real está dentro do intervalo. O `ExampleWand` escolhe `ExampleMightyPrefix` com pilhas de 1 a 2, quando esse prefixo está elegível. O C# emite a linha opcional `PREFIX_CHOOSE_STACK`; o host valida a identidade do item e resolve o prefixo na inicialização. Na rolagem, o C++ tenta primeiro essa escolha, antes dos pesos e das preferências por tipo: somente a aceita quando passa por `AllowPrefix` e `CanRoll`; caso contrário usa a seleção habitual. Prefixos já salvos seguem pela restauração nativa anterior. Uma regra por item; o código não executa callbacks arbitrários `ChoosePrefix(item, random)` na thread de reforja.

## PrefixChance conforme o estado da pilha

A configuração existente `BunnyItem.ConfigurePrefixRules` agora permite `SetPrefixChanceWhenStackBetween(3, 5, true)`. O compilador emite a linha opcional `PREFIX_CHANCE_STACK`; o host publica o descritor imutável e só consulta `Item.stack` no hook nativo de tentativa automática (`Item.Prefix(-1/-2)`). Dentro do intervalo, `true` força a tentativa que antes dependeria do acaso; `false` impede a tentativa. Fora dele, `BunnyItem.PrefixChance` segue valendo, ou a lógica vanilla quando a propriedade é `null`. O callback não intervém em prefixos explicitamente escolhidos nem na restauração de saves. Se o offset de `stack` estiver indisponível, o item com essa regra falha com segurança para tentativas automáticas. É uma política nativa declarativa, não `PrefixChance(item, rand)` arbitrário em MonoVM.

## Diferença entre três rotas

1. **Definição do conteúdo:** classes BunnyWeapon e BunnyProjectile declaram sprites, dano, Shoot, TimeLeft e demais propriedades através de SetDefaults.
2. **Movimento pré-calculado:** a curva TurnAtTick é avaliada em 120 amostras pelo compilador Roslyn, e o código nativo aplica as amostras sem chamar MonoVM a cada tick. O movimento do segundo projétil foi validado com 2.393 atualizações em .72 + mod 0.1.8.
3. **Eventos reais no MonoVM:** métodos estáticos OnHitNPC e OnKill recebem (call, damage) no worker. BunnyLiveEvents é a classe primária; cada tipo adicional pode implementar uma classe NomeDoProjetilLiveEvents em content/Common/Live/ com métodos públicos estáticos retornando int.

Os novos hooks de BunnyPlayer da .90.16 são avaliados pelo Roslyn durante a compilação para gerar descritores. Depois, OnEnterWorld é despachado pela thread principal ao entrar no mundo, e os efeitos de movimento e sombra são aplicados por hooks C++ nativos por quadro. Não se trata ainda de executar arbitrariamente o método C# em cada quadro; a API suporta somente esses retornos limitados, com verificação de ativação e desativação.

Os métodos de instância do BunnyProjectile que geram políticas nativas não são os mesmos callbacks estáticos do worker MonoVM. Não confundir políticas nativas com execução gerenciada.

A fila compartilhada permanece com capacidade 64 e limite agregado de 25.000 eventos por sessão. Há um tipo principal e até sete vínculos adicionais, compartilhados entre mods e projéteis. Dois mods independentes foram validados na .75; pausa/retomada na .77 e reentradas de mundo na .78. Esses resultados cobrem os cenários descritos em [STATUS](../../../../../docs/STATUS.md), sem garantir isolamento completo de falhas, multiplayer ou hot-unload.

## Diagnóstico (.74 em diante)

Use **bl.csharp.managedAvailable()** para verificar inicialização do MonoVM e **bl.csharp.hostInfo()** para consultar runtimeMode, monoVmReady, sourceModCount, nativeAotPrototypeAvailable e nativeAotExternalCount.

**bl.csharp.available() é uma API legada**: verifica o protótipo NativeAOT libbunnycs.so, que não será embutido nem inicializado por padrão na .74. Um resultado false não significa que o MonoVM ou a compilação C# falhou.

Para eventos e movimento, consulte bl.csharp.sourceAutoStatus(), bl.csharp.sourceLiveHitStatus(), bl.csharp.sourceLiveProjectileStatus() e bl.csharp.sourceProjectileAIStatus(modId, nome).

## Limites importantes

A .73 foi validada funcionalmente no Android para 985 callbacks processados de 985 aceitos, incluindo 879/879 de ExampleWaveBolt. Zero falhas, descartes e lockDrops. O FPS da .73 não foi informado nesta rodada. A .74 também foi validada em gameplay; o histórico atualizado está em [STATUS](../../../../../docs/STATUS.md). A telemetria adicional da .79 ainda precisa da validação específica indicada ali e nas notas dessa versão.

Não copiar ModItem, ModProjectile, ModNPC, ModBuff, AI e outros métodos do tModLoader, nem afirmar compatibilidade automática com mods de PC, netcode ou APIs .NET não testadas.


## Contrato atual para novos exemplos

- Callbacks estáticos `OnHitNPC(int call, int damage)` e `OnKill(int call, int damage)` executam no worker Mono, fora da thread do jogo. `call` é uma sequência da sessão compartilhada, não o ID do NPC nem do projétil. O retorno `int` é diagnóstico; ele não altera automaticamente dano ou objetos do Terraria.
- O caminho de políticas nativas de `BunnyProjectile.OnHitNPC/OnKill` é separado desses callbacks. Cada exemplo deve dizer qual dos dois utiliza.
- A fila de 64 posições pode recusar eventos quando cheia. Não prometer entrega garantida. O teto agregado de 25.000 eventos continua sendo uma limitação experimental por sessão; não é um limite por mod.
- Pausar um mod adicional descarta eventos pendentes desse mod; não descarrega sua assembly nem reinicializa os campos estáticos C#. A pausa é recusada se um callback desse mod estiver em execução. O mod principal ainda não possui pausa independente.
- A troca de mundo refaz os vínculos, mas mantém o runtime e os caches. O estado estático de um exemplo deve considerar essa duração de vida.
- A API de mod não deve chamar comandos `bl.csharp.*` do console para se inicializar. Esses comandos servem a diagnóstico e controle do loader.

Veja a [base para a API de modders](../../../../../docs/csharp-api-foundation-v79.md) para as correções de lifecycle e os próximos contratos a estabilizar.

## Bônus de acessórios dependente da vida do jogador (experimental)

`ExampleGuardedPrefix` demonstra `ApplyAccessoryEffects(BunnyPrefixPlayerSnapshot player, ref int defenseBonus)`. O snapshot imutável leva vida atual e vida máxima do jogador. O registro opt-in marca bit 32 em `PREFIX_CALLBACK`. O hook nativo mantém o bônus declarativo anterior e aplica o retorno gerenciado dentro de -100..100 quando o worker estiver pronto. O resultado é guardado por jogador/prefixo/vida durante no máximo 250 ms, com tentativa global limitada a uma a cada 150 ms por thread e espera individual de no máximo 650 µs. Indisponibilidade de campos, worker ocupado, timeout e payload inválido mantêm somente o bônus declarativo. Nenhum objeto `Player` IL2CPP é passado ao MonoVM. Ainda não há equivalência geral a `ApplyAccessoryEffects(player)` do Bunny JavaScript, nem validação de FPS ou multiplayer no aparelho.

## Prefixo de item em C# — escolha, chance e efeitos dinâmicos

As classes `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion` oferecem três novos métodos opt-in além de `AllowPrefix(BunnyPrefixItemSnapshot,int)`:

```csharp
public override int ChoosePrefix(BunnyPrefixItemSnapshot item)
    => item.Stack == 2 ? 73 : -1;

public override bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request)
    => request == -1 ? item.Damage >= 20 : null;

public override void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix, ref BunnyPrefixApplyStats effects)
{
    if (prefix == 72 && item.Damage >= 20)
    {
        effects.DamageBonus = 3;
        effects.ValueBonus = 15;
    }
}
```

São exemplos do fixture C#; não alteram os valores dos itens jogáveis. O compilador reconhece overrides reais, registra `ITEM_PREFIX_CALLBACK` sem depender do JavaScript, o worker MonoVM resolve a classe e executa `ChoosePrefix` (tipo 3), `DecidePrefixChance` (tipo 4) e `ApplyPrefix` por instância (tipo 5). A integração nativa preserva o fluxo de reforja, vetos explícitos e o contador de RPC: até 4 por operação, prazo individual de 650 microssegundos, total de 2,5 ms.

A escolha retorna -1 quando não há preferência; um valor positivo precisa estar entre 1 e 255 e passar por elegibilidade. A chance é `null` (decisão nativa), `false` (veto) ou `true` (forçar tentativa, sem ignorar filtros). O efeito soma somente bônus positivos validados de dano e valor após a aplicação do prefixo. O callback `ApplyPrefix(ref BunnyPrefixApplyStats)` continua como política estática compilada. O snapshot não contém RNG nativo, mundo ou objetos mutáveis: esta API **não** equivale a `GlobalItem` nem a todas as assinaturas do JavaScript original. Gameplay Android, persistência e FPS continuam sem certificação.

## Item AllowPrefix em C# — worker ligado aos hooks (experimental)

As quatro famílias `BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion` oferecem `AllowPrefix(BunnyPrefixItemSnapshot item, int prefix)` como override opt-in. O exemplo jogável `ExampleWand` veta o prefixo vanilla 74 quando o dano atual é inferior a 20. O compilador emite `ITEM_PREFIX_CALLBACK` apenas quando encontra um override concreto. O loader valida o item, tipo, máscara e duplicatas; o bridge publica por snapshot imutável a identidade mod/classe, e o startup vincula o assembly e agenda warmup independente.

O `BunnyItemPrefixRuntime.Dispatch` recebe os seis campos suportados de `BunnyPrefixItemSnapshot` e o prefixo ID 1–255 no worker MonoVM. Os hooks de `GetRollablePrefixes`, `CanRollPrefix`, `RollAPrefix`, `Item.Prefix` e `TryGetPrefixStatMultipliersForItem` consultam o worker somente depois do warmup. Filtros nativos declarativos têm prioridade. Uma transação permite até quatro RPCs, em até 2,5 ms totais, com prazo de 650 µs cada; decisões repetidas são armazenadas por prefixo e assinatura do item na transação. Timeout, fila indisponível e saída inválida preservam as políticas nativas, sem usar resposta tardia. Sem veto real nem candidato C# a seleção original do Terraria e seu RNG são mantidos.

A restauração gerenciada com escopo próprio ignora o callback; a preservação universal de prefixos vanilla por todos os caminhos de carregamento requer QA no dispositivo. Testes host C#, C++ e compilação ARM64 validam contrato, segurança e hooks simulados, mas não FPS, saves reais, clones, multiplayer nem uma APK do Terraria em execução. Esta API de snapshot não é paridade integral com `ModItem/GlobalItem.AllowPrefix(item,prefix)` do Bunny JavaScript.

## BunnyGlobalItem — filtros globais de prefixo por snapshot

`BunnyGlobalItem` permite aplicar a mesma política a itens vanilla e de outros mods, sem possuir um item próprio. Diferentemente de `BunnyWeapon` e `BunnyItem`, não precisa de `SetDefaults`, imagem ou registro de item. Um mod que contém **somente** uma classe global é aceito pelo parser e pelo compilador nativo, com `GLOBAL_PREFIX_CALLBACK` e identidade verificada no carregador.

```csharp
using BunnyLoader.Source;

namespace MyPrefixMod;

public sealed class PrefixRules : BunnyGlobalItem
{
    public override bool AppliesTo(BunnyPrefixItemSnapshot item)
        => item.Damage >= 10;

    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix)
        => prefix != 80;

    public override int ChoosePrefix(BunnyPrefixItemSnapshot item)
        => item.Stack == 3 ? 74 : -1;

    public override bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request)
        => request == -1 ? false : null;

    public override void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix,
                                     ref BunnyPrefixApplyStats effects)
    {
        if (prefix == 73)
        {
            effects.DamageBonus = 2;
            effects.ValueBonus = 5;
        }
    }
}
```

Os callbacks são opcionais e o compilador codifica somente os overrides presentes na máscara `1/2/4/8`. `AppliesTo` verifica a elegibilidade no worker. O host publica uma lista imutável de políticas globais, incluindo mods diferentes; **qualquer veto a AllowPrefix prevalece**, a primeira escolha válida é usada, **false vence true** para `DecidePrefixChance`, e os bônus pós-aplicação validados são somados. O limite compartilhado continua sendo quatro solicitações síncronas por operação, com 650 µs por chamada e 2,5 ms no total. Onde o resultado não chega dentro do orçamento, o host preserva a política nativa.

Para itens de mods, a lista de candidatos também incorpora, sem repetir IDs, os prefixos vanilla dos grupos `PrefixLegacy.Prefixes` disponíveis no Terraria: espada, ranged, magic, summon, armas genéricas e acessórios. Itens vanilla seguem o caminho original quando nenhuma regra altera a rolagem; não há varredura de todos os itens por reforja.

**Limitações:** a implementação de C# usa snapshots imutáveis, não recebe `Item`, `Player` ou `UnifiedRandom` reais; não substitui o ciclo de vida completo de `GlobalItem` JavaScript, nem expõe mutações arbitrárias de itens. A compilação e os fixtures host não comprovam funcionamento, persistência, multiplayer ou FPS no Android. A PR segue em draft até essa validação.
