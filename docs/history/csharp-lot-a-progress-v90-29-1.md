# Progresso Lote A — .90.29.1

Base preservada .90.28.7; .90.28.4 continua referencia Android anterior.
Runtime: 051766a6c0a34b9b27def096ff891f5667933820. ExampleMod permanece
0.1.36, com 51 fontes/37 classes registraveis/26 PNG; a atualizacao inclui
documentacao das receitas e matriz 201, sem adicionar classes. Runtime .90.29.1 compilado/assinado por 38596246405, entrega experimental
privada com .bmod 0.1.35. A atualizacao documental 0.1.36 exige novo pipeline.

Implementado inclui estados “Implementado, aguardando validacao” e
“Validado por testes automatizados”. Validacao automatizada nunca e prova
de equivalencia completa no celular. Nenhum exemplo foi promovido neste grupo.

| Categoria | Escopo | Implementados | Validacao automatizada | Android integral | Parciais | Nao implementados |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Accessories | 7 | 3 (42.86%) | 3 (42.86%) | 0 | 3 | 1 |
| Ammo | 1 | 0 (0.00%) | 0 (0.00%) | 0 | 0 | 1 |
| Armor | 6 | 2 (33.33%) | 2 (33.33%) | 0 | 0 | 4 |
| Consumables | 3 | 0 (0.00%) | 0 (0.00%) | 0 | 1 | 2 |
| Items | 4 | 0 (0.00%) | 0 (0.00%) | 0 | 1 | 3 |
| Materials | 1 | 1 (100.00%) | 1 (100.00%) | 0 | 0 | 0 |
| Prefixes | 3 | 0 (0.00%) | 0 (0.00%) | 0 | 2 | 1 |
| Quests | 1 | 0 (0.00%) | 0 (0.00%) | 0 | 0 | 1 |
| Rarities | 2 | 0 (0.00%) | 0 (0.00%) | 0 | 0 | 2 |
| Tools | 5 | 2 (40.00%) | 0 (0.00%) | 0 | 0 | 3 |

Total: 33 exemplos, 8 implementados (24,24%), 6 com validacao automatizada
(18,18%), 7 parciais e 18 nao implementados. Nenhum arquivo omitido. Relatos
Android anteriores de efeitos isolados/mask/robe permanecem registrados, mas
nao promovem equivalencia integral do arquivo JS sem os testes aplicaveis.

## Entrega desta infraestrutura

- Receita C# ligada diretamente a Recipe/Item nativos, sem chamar API JS.
- Registro de itens e tiles compartilhado separado da instalacao/renderizacao.
- Dependencias tardias por UID/nome, isolamento, diagnostico por receita,
  limites de memoria tipados e rollback; lookup retry sem duplicar registro.
- Cache de compilacao atualizado; contrato vanilla de tiles corrigido para754.
- Independencia executada no host com registradores/adapter de producao,
  BL_HAVE_QUICKJS=0; nao e boot completo do APK sem QuickJS.

## Pendencias que esta entrega nao resolve

Bancada C# completa, item colocavel, TileObjectData e hooks de suporte/drop/
dust/map/housing/adjacencia/icone; receitas originais das armaduras; portas
restantes dos 33 exemplos; equipment policies ainda dependentes de QuickJS;
menu/Guia/variantes de recipes alem de maxRecipes; validacao Android real.
Essas pendencias sao implementacoes faltantes, nao limitacoes tecnicas
comprovadas do Terraria. Ver os contratos precisos em csharp-lot-a-development.md
e csharp-independence-audit.md.

## Teste Android deste artefato experimental

Abrir ExampleMod, criar ExampleItem com terra perto de bancada vanilla; usar
ExampleItem para fabricar Pickaxe/Hamaxe. Verificar crafting/Guia/pilhas e
reabrir mundo. Com mod independente, experimentar ingrediente/estacao por
UID/nome de outro mod JS/C#, inclusive dependencia ausente e ordem invertida.
Confirmar equipamentos, prefixos, asas, armaduras e desempenho anterior.
Nao procurar bancada C# propria neste artefato: ainda nao foi incluida.

Atualizacao da referencia original: e608114efb15a3a021887a6132d9e2a487ba6b62
(mais seis commits), matriz de 201 JS. Dois exemplos de touch buttons foram
contabilizados como nao implementados no Lote F; todos os 199 anteriores e
os 33 do Lote A permanecem. O core original atualizado nao foi transplantado.

## Checkpoint final desta continuidade

Runtime .90.29.3 / code168, ExampleMod 0.1.36. Commit:
6fac739d8e748ca390d83cc7ebe55ba4e7b9f44a. Oito suites de regressao aprovadas;
Android build/assinatura e entrega experimental privada aprovados em
[38609568759](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38609568759).
PR de acompanhamento em rascunho: [21](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/21).
Nao e release oficial do Lote A. Nenhuma validacao de gameplay/FPS nova no Android.

Lote A permanece: 33 exemplos; 8 implementados, 6 com evidencias automatizadas,
7 parciais e 18 nao implementados. Matriz global 201, sem remover os anteriores.
Os 51 fontes/26 texturas do ExampleMod foram preservados. Nao foram adicionados
ports neste grupo. A versao 0.1.36 atualiza guia/matriz/catalogo.

Proximo passo concreto continua sendo a bancada C# completa: identidade por
categoria, SDK publico de tile/estacao/item colocavel; registro e TileObjectData;
frames/ancoras, destruicao/drop/dust, mapa/housing, adjacencia e icone do Guia.
Usar os registradores independentes e NativeRecipes existentes; nao criar
registro paralelo nem voltar a chamar RecipeLoader JS. Portar ambos os arquivos
ExampleWorkbench e as receitas originais das armaduras depois dos contratos
funcionais, mantendo os demais 33 exemplos no escopo. Policies de equipamento
e suporte de recipes alem do limite ainda precisam de desacoplamento.


## Correção experimental .90.29.4 — ABI de referência de receitas

O relatório Android da .90.29.3 enviado pelo usuário registra dois SIGSEGV na
UnityMain, PIDs 4610 e 4424, com fault address 0x10000004b. A build anterior
passou na CI, mas falhou no dispositivo; sua aprovação automática não é evidência
de estabilidade Android. O relatório não contém os primeiros frames legíveis
para atribuir sozinho o endereço de falha a uma função específica.

A inspeção encontrou um defeito concreto: NativeRecipes passava `&recipe` e
`&previous` a `il2cpp_field_static_set_value` para o campo de referência
Recipe.currentRecipe. O ABI exige o objeto diretamente para referências e o
endereço do valor somente para tipos de valor. Isso instalava uma referência à
pilha como objeto gerenciado, inclusive ao restaurar a transação anterior.
As duas escritas foram corrigidas, preservando o isolamento de receitas e a
transação JavaScript existente.

O mock foi corrigido para reproduzir o ABI de produção e recusar referências
que não pertencem aos objetos alocados. O código anterior foi executado com esse
mesmo teste e abortou na verificação de referência; o código corrigido passa nas
execuções normal, pump, tipo inválido, bounds inválidos e assinatura ausente.
A execução normal verifica registro, rollback e restauração de currentRecipe.
O cache de fontes e o cache Roslyn foram incrementados; ExampleMod permanece
0.1.36, pois seus fontes e assets não mudaram.

Estado: correção testada no host; build/CI e nova validação Android pendentes.
Não há novos exemplos portados nem conclusão do Lote A.

Teste Android: instalar o APK experimental .90.29.4, abrir o mesmo personagem e
mundo com ExampleMod 0.1.36, aguardar os registros de receitas, abrir crafting,
fabricar itens acessíveis, salvar/sair e reabrir. Conferir ausência de SIGSEGV,
registro das receitas e preservação de itens/prefixos. Repetir com um mod JS
habilitado. Enviar o relatório se houver qualquer falha. FPS requer medição no
aparelho; não é garantido pela CI.


### Evidências de entrega .90.29.4

Commit de runtime: `cf1a24d9196aac51feb0d96e9e30029a374771c6`.
Oito suítes aprovadas no mesmo commit: audit 38613198853, combat 38613198723,
prefix 38613198765, lifecycle 38613198818, monoGc 38613198960,
monoSignals 38613198782, visual 38613198669, nativeVisual 38613198759.
[Build Android 38613198798](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38613198798)
compilou, assinou V3 e entregou o APK experimental privado com ExampleMod 0.1.36.
Matriz verificada: 201 JS canônicos, 178 na referência do fork, 51 fontes C#;
33 exemplos do Lote A preservados, sem novos estados de conclusão.
Validação da correção no aparelho continua pendente.


## Continuidade .90.29.5 — vinculo item/tile nativo

O usuario confirmou que a .90.29.4 abriu corretamente e que as receitas do
ExampleMod nao tiveram erros. Isso valida essa sessao de carregamento/receitas;
nao prova cada receita fabricada, multiplayer, saves de bancada ou FPS.

Implementada API publica DefaultToPlaceableTile vanilla/mod e bridge direta
para createTile/placeStyle, com larguras/offsets reais, dependencias tardias,
retry limitado, refresh das instancias e preservacao dos wires de 20/21 campos.
Wire novo de 22 campos possui marcador explicito de behavior ausente. A API
usa registradores nativos compartilhados, sem JS como intermediario.

O fixture C# independente e compilado de verdade e seu wire entra na fila e
no adapter de receitas de producao sem QuickJS. Nao foi tratado como um port do
ExampleWorkbench nem contabilizado como novo exemplo concluido. Nenhum dos
201 exemplos foi removido; 33 do Lote A continuam no escopo. Ver
[cookbook e contratos](csharp-placeable-items.md).

Proximo passo da bancada permanece o tile nativo, especialmente sua instalacao
apos TileObjectData.Initialize: os defaults compilados chegam depois do registro
antecipado, e repetir SetStaticDefaults JS seria incorreto. A configuracao C#
precisa de janela nativa propria com restauracao de readOnlyData e cache de forma,
sem sobrescrever callback JS nem perder o formato ao trocar de mundo. Em seguida
implementar frames/apoio/drop/dust/adjacencia/Guia e portar ambos os fontes.

Versao .90.29.5/code170, cache Roslyn v19; ExampleMod permanece 0.1.36. Build
experimental e validacao Android pendentes; Lote A nao concluido.


### Evidencias de entrega .90.29.5

Runtime `f0ba6804d6cc53b546e9b6133182083ad7321394`: oito suites aprovadas
(audit 38617948795, combat 38617948838, prefix 38617948716,
lifecycle 38617948914, monoGc 38617948691, monoSignals 38617948980,
visual 38617948879, nativeVisual 38617948676).
[Android 38617948804](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38617948804)
compilou, assinou V3 e entregou artefato experimental privado; ExampleMod
permanece 0.1.36. O teste --placement-wire usa a saida real de Compiler e
CompiledAssemblyProbe, nao apenas um wire escrito manualmente.

Escopo entregue: API e bridge de itens colocaveis; 0 novos ports do ExampleMod.
Escopo ainda aberto: bancada C# e receitas originais dependentes dela; Lote A
permanece incompleto. Validacao Android nova nao executada nesta entrega.


## Pacote de teste Android da API de colocacao

NativePlacementProbe 0.1.0 e um mod separado, UID
943c352f-5b07-4b3a-b565-620fc7ea63d3, com fontes editaveis e PNGs do
ExampleModBunny original. Requer APK .90.29.5; nao modifica o runtime ou o
ExampleMod 0.1.36. Tres itens colocam tile vanilla 18/estilo0 e tile19/estilos0 e1.
Cada receita usa uma terra para dez unidades, sem estacao. README no bmod indica
colocacao, consumo, crafting perto da bancada, estilos, drops vanilla,
salvar/reabrir e coexistencia com C#/JS.

Compiler/CompiledAssemblyProbe compilam os quatro fontes e geram tres registros.
O --probe-wire executa o wire resultante no registrador, defaults e receitas
nativos de producao sem QuickJS, com metadata Terraria emulada. Packaging verifica
manifesto, fontes, ausencia de JS/DLL/SO, dimensoes e hashes dos PNGs reais.
Este pacote NAO implementa o tile novo da bancada C#; nao promove nenhum dos
201 exemplos da matriz. Teste Android da nova API permanece pendente.


### Entrega do NativePlacementProbe 0.1.0

Commit `4a15074920521d812a964a71568d1a26ed660f3e`, oito suites aprovadas
(audit 38620813424, combat 38620814061, prefix 38620813444,
lifecycle 38620813593, monoGc 38620813870, monoSignals 38620813842,
visual 38620813699, nativeVisual 38620813712).
[Build 38620813619](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38620813619)
aprovou compilacao Android, assinatura e publicacao experimental privada do APK
.90.29.5 e NativePlacementProbe-v0.1.0.bmod. Quem ja usa .90.29.5 precisa apenas
importar/habilitar o pacote novo. ExampleMod 0.1.36 e runtime preservados.
Validacao de gameplay, salvamento e FPS no Android pendente.
