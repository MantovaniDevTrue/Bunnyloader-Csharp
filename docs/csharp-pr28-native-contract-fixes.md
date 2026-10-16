# PR 28: contratos nativos e ajuda do editor

Base: PR #27, com o isolamento de delegates e os contadores da .52/.53.
Este lote nao cria outro APK nem altera a versao do ExampleMod.

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
nao gera APK nem afirma validacao visual Android ou 60 FPS.
