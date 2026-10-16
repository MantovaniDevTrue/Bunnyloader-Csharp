# PR 28: contratos nativos e ajuda do editor

Base: PR #27, com o isolamento de delegates e os contadores da .52/.53.
A versao do ExampleMod permanece 0.1.40. APKs .55 e .56 gerados a pedido para
validacao das correcoes no dispositivo.

O relato da .52 identificou EquipmentCountUnavailable (ExampleBeard),
EquipmentHandFlagUnavailable (ExampleRobe), VanillaCloneIdUnavailable
(ExampleSoul), EquipmentSetsUnavailable (Shield) e
WingJumpSpeedContractUnavailable (ExampleWings).

## Correcoes

- Count, ItemID e BuffID sao lidos pela largura integral real de 8, 16 ou
  32 bits, com sinal e limites verificados. Count e gravado pela mesma largura;
  overflow e campos constantes sem escrita continuam sendo rejeitados.
- Valores estaticos podem ser campos ou propriedades. Getters/setters precisam
  ser estaticos e ter assinaturas compativeis; excecoes de getter mantem o
  recurso pendente e permitem retry. Arrays continuam passando pelo GC do jogo.
- Escudos e outros equipamentos sem flags de Sets nao exigem uma classe Sets
  inexistente. Body, flags explicitas e WingStats ainda exigem seus contratos.
- MissingHand/HidesHands/UseHairColor e outras tabelas utilizadas sao crescidas pela propria
  referencia validada, inclusive quando nao foram encontradas por TypeTables
  por serem propriedades ou terem tamanho inicial diferente de Count.
- jumpSpeed aceita campo Single ou getter estatico Single com ponteiro nativo
  validado. O voo chama o getter diretamente, sem boxing por frame.
- A resolucao de IDs de buffs e do PumpkinHelmet usado pelo registro de
  conjuntos tambem aceita constantes menores que Int32.
- A ajuda do editor e exibida separadamente do historico compartilhado de logs,
  e acompanha o idioma de codigo ao abrir e ao alternar entre C# e JS.

Nenhuma dependencia de QuickJS foi introduzida. Falhas de contrato nao sao
transformadas em sucesso nem resolvidas com IDs/offsets fixos.

## Evidencia e limites

O teste novo de campos Android, ligado ao NativeItemPolicies anterior ao lote,
reproduz EquipmentCountUnavailable, EquipmentHandFlagUnavailable e
VanillaCloneIdUnavailable e termina com policies ainda pendentes. Com as
correcoes, os 14 modos de itens passam, incluindo campos menores, propriedades,
layout boxed, getter que lanca excecao, overflow e recuperacao.

Os 13 modos de armadura passam, incluindo ItemID/BuffID Int16. A matriz de
valores estaticos cobre os seis tipos integrais e rejeicao de overflow sem
alterar o valor anterior. Ha 10.000 atualizacoes de voo e 10.000 desenhos por
modo, sem novos objetos IL2CPP durante esses loops.

ASan/UBSan passa no modo de propriedades/boxed e no registro de armadura com
IDs menores (deteccao de leaks desativada no host simulado). O CI tambem roda
os testes reais de delegates ref/out, falhas e Unhook da PR #27 e a ajuda do
editor. A referencia publica foi regenerada do SDK compilado (43 tipos),
incluindo delegates, handles, contadores de falha e RegisterHooks. O gerador
agora distingue `out` de `ref` e identifica RegisterHooks como registro no
worker. A suite gerenciada de hooks tambem passou localmente com compilacao
Roslyn e execucao .NET reais.

## Retorno da .54 e contratos confirmados

O relatorio do Xiaomi da .54 confirma MonoVM verificado, entrada no mundo,
callbacks de dano e salvamento. Restaram ExampleRobe/EquipmentHandFlagUnavailable
e ExampleSoul/AnimationMethodsUnavailable; conjuntos, texturas e receitas
apareceram sem pendencias. O historico de SINAL 9 e paradas por installPackageLI
nao demonstra um crash nativo da sessao atual.

A [inspecao de metadados](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38888015328)
usa o runtime Terraria 1.4.5.8.6 cujo SHA-256 e verificado no workflow. Ela
exporta apenas assinaturas selecionadas, sem APK, assets ou dump completo:

- ArmorIDs.Body.Sets possui MissingHand, nao HidesHands. ShowHands escreve
  MissingHand=true; o contrato alternativo HidesHands continua usando false.
  A polaridade oposta tambem e documentada nas
  [notas oficiais de portabilidade do tModLoader](https://github.com/tModLoader/tModLoader/blob/1.4.5/PortingNotes_1.4.5.md).
- DrawAnimationVertical recebe (int ticksperframe, int frameCount,
  bool pingPong=false). A instalacao usa esse construtor com false e conserva
  suporte ao contrato de dois argumentos. O resolvedor global de assinaturas
  e suas verificacoes de nomes/tipos nao foram afrouxados.
- Falhas de registro, inicializador, construtor, hook, root e tabelas de
  animacao agora informam a etapa exata. Uma excecao nao publica uma animacao
  parcialmente construida; a instalacao pode ser repetida.

Os tres cenarios adicionais reproduzem os contratos da .54 nos layouts raw e
boxed e uma excecao recuperavel no construtor de tres argumentos. Conferem
polaridade da flag, preservacao dos slots vanilla, ticks/frames/pingPong e
registro novamente apos InitializeItemAnimations sem reconstruir o objeto.
A matriz continua incluindo os 11 contratos anteriores e execucao sem QuickJS.
Contra o NativeItemPolicies anterior, o novo cenario reproduz exatamente
EquipmentHandFlagUnavailable e AnimationMethodsUnavailable e falha por policies
pendentes; com a correcao, ambos ficam prontos.

A ajuda do editor ganhou fonte maior, duas linhas minimas, fundo e separacao
do cabecalho/saida. O carregamento inicial sem conteudo pendente aparece em
DEBUG, enquanto compila; pendencias reais e espera apos o registro continuam
em WARN. O relatorio deixa de chamar as etapas normais do Mono de CAUSA:
separa as quatro ultimas etapas como contexto e destaca indicios de falha,
sem afirmar que sejam a causa confirmada. Falhas Mono com detail ou fases
failed/invalid/missing/exception continuam destacadas. O classificador Java
usado pelo relatorio e compilado e testado sem Android, localmente e no CI.
Timeouts e gates de entrada no mundo permanecem preservados.

Ainda falta testar estas novas correcoes no Xiaomi para confirmar o visual
do manto, a animacao da alma e o espaco da ajuda em janela/tela cheia. Este lote
nao afirma validacao visual Android ou 60 FPS; as builds de teste estao descritas abaixo.


## Retorno da .55: Color e ordem dos canais

A .55 entrou no mundo e salvou, mas passou a repetir a busca por
Microsoft.Xna.Framework.Color. O teste anterior ignorava o namespace na
resolucao simulada, portanto aceitava um contrato que nao existe neste jogo.
A simulacao agora confere esse namespace: o codigo anterior reproduz
ExampleSoul/ColorAbiMismatch com as policies pendentes.

A [segunda inspecao do runtime verificado](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38892035086)
confirma que o tipo ja existe: Microsoft.Xna.Framework.Graphics.Color,
com os campos Byte A/B/G/R nas posicoes 0/1/2/3 e Item.GetAlpha(Color newColor).
Nao e preciso criar uma classe substituta no Mono ou alterar os assemblies do
jogo. A API C# ja oferece WorldAlpha e cores ARGB para os efeitos visuais.

A nova ponte NativeColorLayout recebe o tipo real pela assinatura/campo e
valida tamanho de quatro bytes, canais Byte nao estaticos, offsets dentro do
valor e sem sobreposicao. GetAlpha exige que retorno, parametro e Item.color
sejam exatamente o mesmo tipo antes de instalar o hook. Falhas continuam
pendentes e com diagnostico; nao ha busca repetitiva por namespaces candidatos.

O empacotamento usa os offsets verificados, tanto na cor da alma quanto em
DrawData dos efeitos visuais. A cor do item fica pre-calculada na instalacao;
a conversao de visuais nao cria objetos IL2CPP. A visibilidade continua baseada
no alpha do comando, independente de onde esse canal esta no valor nativo.

Os 17 modos incluem o namespace Graphics com layout ABGR, o contrato Framework
com layout RGBA, layouts raw/boxed, rejeicao de parametro/retorno incompatíveis
e recuperacao. Ha 10.000 chamadas GetAlpha sem novas alocacoes IL2CPP e
preservacao da cor recebida para itens vanilla, alem dos loops de voo/desenho.
O renderer tambem verifica azul opaco (R=0) e vermelho transparente (A=0).

O APK .56 (versionCode 216) inclui essa ponte e a validacao do marcador de
layout no binario final. A confirmacao de cores e transparencia no Xiaomi
continua sendo feita no aparelho; os testes do host nao substituem essa etapa.


## Retorno da .56: luz e WorldItem.active

O usuario confirmou os efeitos dentro do jogo na .56 e a sessao atual deixou
de repetir o erro de Color. Restou ExampleSoul/Field=active/NativeFieldContract.
A instalacao de animacao e cor ocorre antes da etapa de luz; ve-las em jogo
nao significa que toda a policy ficou pronta.

A [inspecao completa de campos e propriedades](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38893907331)
confirma WorldItem.get_active() como getter bool de instancia e WorldItem.inner
como referencia a Item. position/width/height pertencem a Entity e sao
resolvidos pela hierarquia. A inspecao anterior selecionava campos e por isso
nao mostrou o getter: ela nao era suficiente para validar esse membro.

A luz agora aceita o campo Boolean validado ou o getter nativo bool de
instancia sem argumentos e com ponteiro valido. O getter e chamado diretamente,
apos o UpdateItem original, somente para itens com policy de luz, sem boxing
ou runtime_invoke por quadro. O tipo de inner e o layout X/Y de position tambem
sao conferidos. Falhas de getter, escala, AddLight e hook informam a etapa.

Os 21 modos de itens modelam geometria herdada, propriedade active e contrato
alternativo com campo. Rejeitam getter estatico, retorno diferente de Boolean
e ponteiro ausente e verificam recuperacao. O codigo da .56 reproduz exatamente
Field=active/NativeFieldContract nesse cenario corrigido. O teste realiza
10.000 updates de luz sem novas alocacoes IL2CPP e cobre item inativo,
desativacao pelo update original, item vanilla e referencia inner nula.

As filas transitórias durante o registro inicial vao para DEBUG, incluindo
waiting-native-install sem erro. Falhas reais de policy continuam em WARN;
gates, timeouts e aviso de entrada com assets ainda pendentes sao preservados.
O APK .57 (versionCode 217) agrega as correcoes para validacao no Xiaomi,
mantendo os mesmos pacotes de mods.
