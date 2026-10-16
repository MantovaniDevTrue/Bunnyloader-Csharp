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
- HidesHands/UseHairColor e outras tabelas utilizadas sao crescidas pela propria
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
correcoes, os 11 modos de itens passam, incluindo campos menores, propriedades,
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

Estas variacoes reproduzem as suposicoes incorretas, mas nao substituem um
dump dos metadados nem um teste no Xiaomi. A confirmacao Android deve repetir
barba, manto, alma animada, escudo, conjunto, voo e troca de loadout, e verificar
se as pendencias de receitas/conjuntos desaparecem. Os timeouts e gates de
entrada no mundo permanecem preservados. Nao ha nova afirmacao de 60 FPS.
