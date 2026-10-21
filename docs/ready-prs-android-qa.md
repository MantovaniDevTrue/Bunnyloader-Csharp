# Sete PRs integrados — QA experimental Android

Bunny `1.0.9-item-use-diagnostics-qa` (versionCode 308), ExampleMod CSharp `0.1.63`.
Base: `v1.0` em `cfbe784ee3cb`. Branch: `integration/ready-prs-20261020`.

Este APK reúne os PRs #101 (Localization), #102 (Rarity), #114 (AnglerQuest),
#115 (Dust experimental), #116 (Invasion beta local), #117 (CommandLoader)
e #118 (SoundLoader). Os PRs originais continuam
intactos. Não certifica conclusão dos loaders nem substitui a release estável.
Os commits congelados estão em `ready-prs-integration.json`.

## Instalação

Instale o APK experimental assinado `Bunny Loader C# Test`, pacote
`com.bunnyloader.csharptest`, e importe `ExampleMod-CSharp-0.1.63.bl`.
Preserva o UID do ExampleMod e os identificadores das classes existentes.
Não é necessário limpar caches ou apagar saves; a identidade do compilador mudou
para reconstruir caches incompatíveis automaticamente. Faça cópia dos saves de
jogo usados para QA antes dos testes de invasão/persistência.

O Dust é habilitado apenas nesta variante (`bl.dustPreview=true`). A compilação
normal de `v1.0` mantém pacote, versão e flag de preview originais.

## Correções desta revisão

Corrige a recusa de `SetRarity<T>()` em `ExampleBuffPotion.SetDefaults` sem retirar
raridades ou alterar o comportamento dos exemplos. O bootstrap reserva nomes,
assets e referências; chamadas comuns/genéricas permanecem a cargo do Mono.
O HUD obtém o tipo `Color` dos parâmetros dos overloads nativos, valida seu
layout e não procura `Microsoft.Xna.Framework.Color` por suposição.

Repetir primeiro o carregamento com o pacote `0.1.63`: nenhuma falha do parser
na poção, arma ou acessório. Abrir os tooltips de Example Defense Potion,
Example C# Wand e Example Stat Accessory e conferir suas cores. Depois iniciar
a Example Invasion, conferir barra e texto sem erro de classe `Color`.
Anotar versão, ação e relatório da mesma sessão; Android continua pendente.

## Testes no ExampleMod

1. **Carregamento conjunto:** entrar, abrir o relatório e procurar falhas por mod,
   classe e etapa. Confirmar os cinco registros e abrir tooltips de Example Item,
   Example Dust Charm, Example Invasion Beacon e Example Upside-down Fish.
2. **Localization:** alternar Português/Inglês na opção de idioma do jogo,
   abrir novamente os tooltips do ExampleMod e repetir ao retornar ao mundo.
   Conferir títulos/textos e ausência de chaves órfãs. JSONs e o módulo
   `ExampleLocalizationModule` permanecem editáveis no pacote.
3. **Rarity:** conferir a cor do Example Item e da Example Melee Weapon,
   e a raridade animada do Example Stat Accessory. Aplicar prefixos para
   conferir subida/descida de raridade, guardar num baú, salvar/sair e reentrar.
   Os exemplos usam o UID real atribuído pelo loader, sem assumir o slug do mod.
4. **AnglerQuest:** em mundo Hardmode, obter a missão Example Upside-down Fish
   no pescador. O sorteio diário pode exigir vários dias. Pescar invertido,
   confirmar captura e entrega e comparar com pesca sem inversão. Ver
   `ExampleFishingPlayer.cs` e `ExampleQuestFish.cs`; o peixe é do ExampleMod,
   com sprite original. Validar textos em ambos os idiomas.
5. **Dust:** obter Example Dust Charm no catálogo do ExampleMod ou fabricar com
   1 Example Item (sem estação). Usar: devem surgir oito Example Spark Dust
   douradas no jogador, com movimento e desaparecimento definidos em C#.
   Repetir andando; sair/entrar e usar novamente. Emissão é limitada por fila,
   sessão de mundo e pool nativo. Recusa gera erro/aviso, não sucesso fictício.
6. **Invasion:** fabricar Example Invasion Beacon com 10 Example Items perto
   da Example Workbench, que conta como Work Bench. Usar de dia na superfície,
   sem invasão vanilla ativa e com os requisitos do `BunnyUseContext` satisfeitos
   (inclusive progressão de boss quando exigida). Conferir barra, Example Slime,
   Example Elite Slime após 10 pontos, conclusão aos 30 pontos e recompensa
   de 5 Example Items. Salvar/sair no meio e reentrar no mesmo mundo; depois
   entrar em outro mundo para verificar isolamento. Derrotas/despawns não devem
   pontuar duas vezes nem deixar o evento preso.
7. **Comandos C#:** no chat, executar `/echo teste`, `/dizer teste`, `/heal 30` e `/addTime 3600`. Confirmar texto, cura real e avanço do relógio. Repetir `/heal` sem argumento e conferir mensagem de uso, sem crash. Sair e reentrar, verificar que os comandos continuam disponíveis. Em multiplayer, testar autorização, sem permitir ações do cliente que exijam servidor.
8. **SoundLoader:** acertar um inimigo com o Example Wave Bolt e ouvir o efeito `ExampleGun.ogg` posicionado. Repetir a colisão algumas vezes e conferir limite de três instâncias; testar esquerda/direita, pause/retorno e sair/entrar no mundo. Conferir relatório nativo, volume, ausência de travamento e FPS.
9. **Regressão:** na Example Workbench fabricar equipamentos/armaduras;
   equipar/desequipar asas, escudo, botas, acessórios e armadura, morrer/reviver,
   testar o Example Minion e cancelar seu buff. Salvar itens com prefixo no
   inventário e no baú. Repetir com um mod JS conhecido e Lights And Shadows.

## O que os testes automáticos demonstram

`tools/ci/test_ready_prs.py` executa as suítes originais contra as fontes
combinadas, incluindo compilação Roslyn real, registros/contratos nativos,
fixtures IL2CPP executáveis, revogação de assemblies, prazos de Dust,
integração de pesca e round trips C#/C++ da invasão. `tests/ready_prs/run.sh`
executa a emissão de Dust de verdade contra uma fixture IL2CPP: argumentos de
NewDust, recusa de tipo incompatível, limite de fila, sessão obsoleta e jogador
inativo. O mesmo runner percorre os 96 fontes reais com o parser nativo e valida
os contratos de registro antecipado, assets e dependências. A fixture de HUD
executa a resolução/instalação/desenho de retângulos e texto com layouts
boxed/raw e byte order distinto; recusa tipos inconsistentes ou fora do contrato.
A suíte Roslyn executa os defaults reais e verifica as cinco raridades de itens.
As regressões gerais e a compilação ARM64 também bloqueiam publicação.

São evidências de host/CI, não testes executados no aparelho. Android, qualidade
visual, consumo de memória, 60 FPS, multiplayer e salvamento em nuvem continuam
aguardando validação real. Dust e Invasion mantêm os limites documentados em
seus PRs; nenhum é ocultado para chamar o lote de completo.

## Entrega e diagnóstico

O workflow `build-ready-prs-preview.yml` recompila SDK/compilador e módulos AOT,
verifica hashes dos inputs privados, compila/assina o APK e valida as DLLs
incluídas antes de publicar uma prerelease em `Bunnyloader-Csharp-Builds`.
Inclui fontes C# editáveis no `.bl`, commit exato, SHA256SUMS e resultados dos
contratos. Não publica binários do Terraria em artifacts do repositório público.

Se houver falha, exportar o relatório da mesma sessão após reproduzir a ação.
Registrar qual exemplo, passo, idioma, estado do mundo e mods ligados.

## Integração dos sete PRs

O SoundLoader e o CommandLoader foram integrados a partir das fontes originais das PRs #117 e #118, sem substituir os callbacks das cinco PRs anteriores. O arquivo `BunnySourceSdk.dll` e o compilador são recompilados na execução do APK; o manifesto `framework.sha256` é validado dentro do pacote.

A pipeline da branch executa compilação de sintaxe ARM64 para os nove arquivos nativos compartilhados, ligação nativa completa sem gerar APK, testes Roslyn para todos os exemplos, ações dos comandos, invariantes dos sons e regressões anteriores. O APK só é produzido na pipeline de QA depois da aprovação dos gates; sucesso de CI não substitui teste Android real.

## Correções de QA após relatório 21/10

A execução de OnHitNPC já estava confirmada no relatório Android, mas o estado de impacto não incluía X/Y; a posição agora vem do último TickProjectile do mesmo slot, geração e owner. A tela de tooltip reportava `RarityTooltipSignatureMismatch`; esta revisão instala um fallback restrito ao nome do item em `SpriteBatch.DrawString` quando a ABI de MouseText não corresponde, em vez de forçar hook incompatível. Para o Dust Charm e Invasion Beacon, a atualização de mundo solo pode usar o jogador nativo caso o snapshot esteja indisponível; a primeira execução registra `[CSharp][ItemUse] Stage=FirstWorldTick` somente uma vez por mod. Nenhum dos três reparos foi confirmado no dispositivo até novo relatório.

## QA 1.0.5: após o relatório do ItemUse

O relatório Android 1.0.4 confirmou `WorldTickSnapshot Ready=1 Mode=0`, `Stage=Bind Result=Ready` para Charm e Beacon, além de `Stage=ActualUse Result=Queued Flags=0`. Nesta revisão os indicadores são lidos independentemente, evitando zerar `PlayerAlive` e `WorldReady` se `Main.gameMenu` for propriedade ou `worldSurface` tiver outro tipo numérico. O contrato Surface exige coordenada verificada, e multiplayer continua sob autoridade do host. O relatório deverá mostrar, nessa ordem: `ActualUse`, `Consumed`, e, no Dust Charm, `DustEmission Stage=EmitRequest`; resultado do callback ainda depende de validação no Android. Para a raridade Disco, conferir não só o nome mas as linhas do tooltip; o fallback é limitado à região visual abaixo do nome e não altera as cores fora dela.

## QA 1.0.6: callbacks de tooltip C#

O SDK usa `BunnyItem.ModifyTooltips(List<BunnyTooltipLine>)` e `BunnyItem.PreDrawTooltipLine(BunnyDrawableTooltipLine)`. São callbacks de configuração executados ao compilar os defaults, não hooks gerenciados por quadro. A configuração fica no registro nativo do item, exposta via `bl.csharp.tooltipPlan(type)` ao mesmo `TooltipLoader` de Bunny JS. O próprio renderizador executa a onda e o arco-íris no `SpriteBatch`. Códigos arbitrários de desenho C# em tempo real ainda não são suportados.

Ative **ExampleMod CSharp 0.1.63** e use o `ExampleTooltipItem`, que contém as mesmas três linhas do exemplo JS: `Exemplo de Tooltip`, `Bunny Loader` em arco-íris com onda e a descrição dourada. Conferir também um item vanilla e um item JS, para garantir ausência de alterações indevidas, e observar o FPS. As condições de AnglerQuest agora relatam especificamente qual contrato falhou. Dust e Invasion ainda exigem validação dos callbacks de uso no dispositivo.

## QA 1.0.7: correção da ativação do tooltip C#

O relatório 1.0.6 mostrou o item `Exemplo de Tooltip` renderizado como vanilla, sem a linha `Bunny Loader`. Causa identificada nas fontes: `TooltipLoader.Install` era chamado por callbacks JS de `ModItem`, `GlobalItem` e outros, mas nunca por mods exclusivamente C#. Agora `Exports.js` registra esse hook uma vez pelo mecanismo `Hooks.Once` quando a API `bl.csharp.tooltipPlan` está presente, mantendo a decisão de desenhar somente itens com plano C# e o caminho vanilla para os demais. O log nativo `[CSharp][Tooltip] Stage=PlanStored` confirma a instalação do plano no tipo do item. Verifique o mesmo `ExampleMod CSharp 0.1.63` e reporte se as três linhas (nome, arco-íris ondulado, descrição dourada) aparecem sem alterar itens vanilla. O áudio validado não foi modificado.

## QA 1.0.8: renderização da onda por letra

A ativação C# introduzida na QA 1.0.7 foi mantida sem alterações; a chamada de `SpriteBatch.DrawString` na animação agora usa diretamente o receptor `spriteBatch` do renderizador do ExampleModBunny, sem separar a referência ao método. Esse ajuste corrige um possível problema de contexto no desenho dos glifos. O plano C# continua compilado uma vez, sem executar callbacks Mono por quadro, e o hook só desenha itens com plano visual ativo.

Repetir com `ExampleTooltipItem`: três linhas, `Bunny Loader` com onda arco-íris e `Tooltip0` dourado; comparar com o tooltip JS e um item vanilla. Registrar FPS em movimento e parado, trocar de mundo, abrir/fechar inventário e exportar relatório em caso de falha. Os testes automatizados não substituem a validação visual Android. Os loaders Dust, Invasion e AnglerQuest não receberam alterações nesta revisão.

## QA 1.0.9: resultado de ItemUse, sem novos efeitos

O teste Android 1.0.8 confirmou o tooltip animado, mas o Example Dust Charm não apresentou partículas. No console, o evento `ActualUse` foi registrado e `Consumed` confirmou entrega ao MonoVM; não apareceu `DustEmission Stage=EmitRequest` no trecho enviado. Isso não é evidência de conclusão de `OnUse`.

Esta build isola exceções de `OnUse` por item para que outros itens não fiquem bloqueados. `ManagedRuntime` envia `Stage=OnUseResult` ou `Stage=WorldTick Result=Error` para o logger do jogo apenas uma vez por mod e coloca `lastUse` no começo do status limitado a 384 bytes. Mensagens com `Error=` são avisos. Não modifica `BunnyDustEffects`, o emissor nativo, física, quantidade ou desenho das partículas.

Teste: usar `Example Dust Charm` e `Example Invasion Beacon` em sessão limpa; exportar o relatório depois de usar ambos. Procurar `OnUseResult lastUse=ExampleDustCharm` com `Applied=1` ou `Error=`, assim como `DustEmission Stage=EmitRequest`. A invasão deve ser testada de dia, na superfície; flags esperadas `63`. Não considerar Dust ou Invasion corrigidos antes da validação visual Android. A versão 1.0.8 continua sendo baseline do tooltip.
