# Lote A / 2: acessorios, vaidade e material animado

Base Android preservada: .90.28.4, commit `67e6be55d495472e0dfb8e3b6b0d411aa61e821e`.
Entrega: Bunny .90.28.5 / ExampleMod C# 0.1.35. Fontes existentes e UID preservados.

A referencia upstream agora e `bc4f240082a589823536eff74462a11acbdba8ec`:
199 JS, incluindo os 192 anteriores sem alteracao. Os sete novos exemplos entram
na matriz; shaders continuam fora desta etapa. Nenhuma entrada foi removida.

## API publica e caminho de execucao

`BunnyItem.Accessory`, `Vanity` e `Behavior` sao declarados em SetDefaults do C#.
`BunnyItemBehavior` permite texturas Head/Body/Legs/Beard/Back, cor do cabelo,
manto, efeitos aditivos/multiplicativos de equipamento, strongBees, clone de
ItemID, animacao vertical, luz de item no mundo e GetAlpha. Sao politicas reais,
instaladas nos hooks do ItemLoader/EquipLoader original, pela bridge tipada do
Bunny. Nao sao callbacks C# arbitrarios: AI e outros hooks continuam no backlog.

O C# executa SetDefaults no Mono e serializa as politicas. O host resolve assets,
slots, tabelas e hooks na thread do jogo. Cada recurso exige ACK; um erro deixa
seu descritor pendente com causa e stack, enquanto outros descritores prosseguem.
Os efeitos de equipamento usam o ciclo funcional/vaidade/reset do Terraria.
A luz e o alpha usam filtros nativos por tipo e WorldItem ativo. Nao ha chamada
Mono por quadro para estas politicas. Slots sbyte continuam limitados pelo
contrato real do Terraria; aumentar limite de classes nao permite exceder slots.

O V2 aceita ITEM de 20 campos (mods existentes) ou 21 (com comportamento hex).
Cache nativo e cache Roslyn recebem versoes novas. Foram preservados o continue
do dispatch de BunnyItem, a recuperacao de cache e Item.active opcional.
Campos integrais usam largura real, bool exige 1 byte e knockBack exige Single.
O registrador confere o contrato antes de reservar o item.

## Testes automatizados e limites

O CI compila as 51 fontes e executa o wire real do C# contra contratos nativos e
o ItemLoader real em doubles de Terraria. Verifica efeitos funcionais/vaidade,
slots, SetMatch, animacao, luz, alpha, clone, refresh e erros de assets.
Os testes incluem 192 classes em um pacote, 513 arquivos recusados, wire acima
do antigo limite de 8 KiB e rejeicao atomica de mais de 256 itens da categoria.
Limites novos: 512 fontes, 256 KiB/arquivo, 16 MiB de fontes, 1024 tipos,
256 registros/categoria, 2 MiB de defaults e 1 MiB por politica de movimento/combate.
Ainda existem budgets especificos em sistemas de callbacks: revisar ao expandir
cada categoria, sem remover limites de filas ou limites nativos de slots.

Diagnosticos desta bridge identificam mod, classe, arquivo, etapa, hook/campo,
ID, causa e stack quando disponiveis. Falhas de callback registram ate tres
ocorrencias e desativam somente aquela politica. Erros de instalacao ficam
pendentes, com novas tentativas limitadas; receitas de outros itens continuam.
Mensagens antigas de outros subsistemas ainda precisam de auditoria propria.

## Roteiro Android obrigatorio

1. Ativar ExampleMod 0.1.35 junto de um mod JS, iniciar o mundo e conferir seis
   mensagens `csharp item behavior ... instalado`. Pendencias aparecem em
   `bl.csharp.pendingItemBehaviors()`; campos `error`, `source`, `mod` e `type`
   identificam a falha. Nenhuma ausencia de asset deve receber ACK de sucesso.
2. **WaspNest:** equipar funcionalmente, conferir sprite nas costas e abelhas
   fortes; retirar, esconder visual e trocar para vaidade. O bonus so existe
   no slot funcional. Testar coexistencia com equipamento de abelhas vanilla.
3. **ExampleStatAccessory:** comparar os multiplicadores de melee/ranged/magic
   antes/depois (10%, mantendo minion), retirar e trocar loadout; vaidade nao
   concede bonus. Conferir tooltip e valor de venda originais.
4. **ExampleBeard:** equipar em ambos os slots; mudar cor do cabelo, virar,
   caminhar e saltar. Conferir barba nativa e cor do item ao recria-lo.
5. **ExampleBossMask:** usar em vaidade, caminhar, virar, saltar, remover e
   trocar loadout. Conferir sprite Head sem conceder defesa extra.
6. **ExampleRobe:** combinar com calcas, trocar sexo/direcao, caminhar e saltar.
   O SetMatch deve substituir as pernas pelo manto sem esconder as maos.
7. **ExampleSoul:** soltar no chao, verificar ausencia de gravidade, pulso,
   quatro quadros a seis ticks, luz e alpha; recolher e conferir inventario.
8. Save/Exit, reentrada, morte/respawn, bau e bancos para cada item. Identidades
   persistentes sao UID/classe, nao os numeros temporarios de registro.
9. Medir FPS e memoria com os efeitos presentes, tanto em repouso como combate.
   O relato de 60 FPS da .90.28.4 nao valida desempenho desta entrega.

## Continuidade

Lote A nao esta concluido. Proximo grupo: infraestrutura de defesa/UpdateEquip,
bonus de armadura e receitas de estacao de mod. Portar ExampleWorkbench como
dependencia verdadeira e depois Helmet/Breastplate/Leggings; nao trocar a
estacao original por bancada vanilla. Depois Costume/CostumePlayer, Wings,
Boots/Dash completos, StatBonusAccessory/DamageClass, ferramentas com projeteis,
prefixos, tooltip desenhado e moeda de ExampleItem. Os 199 arquivos permanecem
contabilizados, incluindo todos os lotes posteriores. Nao avancar ao Lote B
como se A estivesse completo.

## Evidencia de host registrada

Commit de runtime `2d9c534464750c1f626df6158fbec8373528b3f7`: auditoria/itens
38559202417, combate 38559216905, prefixos 38559202414 e as regressoes de
filas, ciclo de vida, sinal Mono e GC aprovados. Dez casos C# e contratos
nativos/JS aprovados. Este registro promove os seis novos exemplos somente a
Validado por testes automatizados. Validado no Android permanece pendente.

## Correcao .90.28.6 — contrato de textura

A execucao Android da .90.28.5 recusou ExampleBeard no registro. Os seis novos
fontes declaram Texture sem extensao, enquanto NativeContent exigia .png e
retornava -1 antes de registrar a causa. Os testes anteriores de wire e
comportamento nao cobriam essa precondicao; seu sucesso nao validou o carregamento
Android. Nenhum dos seis exemplos recebe validacao Android por esses testes.

A .90.28.6 normaliza stems relativos Content/ para .png, preserva caminhos
com extensao e rejeita extensoes diferentes, traversal, barras invalidas,
controles e caminhos acima de 160 caracteres depois da normalizacao. A mesma
funcao usada no registrador e testada com os nove BunnyItem dos fontes reais;
o CI confere tambem os nove PNG no pacote. Falhas de preflight agora preservam
mod/classe/arquivo/expected/actual/reason no erro retornado ao carregador.
ExampleMod permanece 0.1.35: nao houve alteracao dos fontes ou identificadores.

Repetir primeiro o passo 1 do roteiro Android. Se houver falha, o relatorio
precisa incluir a causa concreta, e nao apenas NativeRegistrationRejected.
Depois executar os testes de cada equipamento/material acima.

Pipeline .90.28.6 aprovado: run `38563527063`, runtime commit
`cec309b023f4092723d29c43567865c3ebb2fe33`. Oito suites independentes passaram,
incluindo auditoria/itens `38563527368`. APK assinado e pacote fonte 0.1.35
gerados pelo fluxo existente. Evidencia final de CI registrada depois do build;
o pacote desse run contem a matriz e o roteiro do commit de runtime.
Teste no Android da correcao continua pendente.

## Correcao .90.28.7 — slots nativos de mascara e robe

O usuario confirmou que .90.28.6 carrega, mas mascara e robe nao podem ser
equipados. O SetDefaults nativo intercepta BunnyItem e retorna sem aplicar
os slots mantidos somente pelo host. Agora o instalador sincroniza Head, Body,
Legs, Beard e Back para o registro nativo, atraves de setNativeItemEquipSlot.
O binding valida campos integrais de instancia, seus tamanhos, offsets e range
do slot; escreve somente a largura real e recusa campos ausentes ou incompativeis.
Slot instalado e reaplicado em cada SetDefaults sem consulta JS por instancia.
A API publica C# EquipTexture continua a mesma e vale para qualquer mod.

Testes nativos criam nova instancia apos o registro e verificam Head Int32,
Body/Legs Int16 e Beard SByte com sentinelas. Recusam overflow, campo ausente,
tipo desconhecido e item nao registrado. O teste host exige sincronizacao dos
slots antes do ACK. Teste no celular ainda pendente: criar/copiar os itens pelo
inventario, equipar mascara em cabeca de vaidade e robe no corpo de vaidade,
trocar loadout, virar/caminhar/saltar e repetir apos Save/Exit.
Nao atribuir Accessory=true para tornar armaduras equipaveis: preservar slots
de armadura e defaults originais. Esta entrega e corretiva; Lote A nao concluido.

Correcao .90.28.7: pipeline `38566058511` aprovado, runtime commit
`e50e613f649e8806f8a82c919859dbc4c60309db`; oito suites de regressao passaram,
APK assinado e ExampleMod 0.1.35 gerados. Evidencia de CI final registrada apos
o build; o pacote inclui a matriz/roteiro do commit de runtime. Validacao
Android da mascara e do robe pendente. Lote A permanece incompleto.
