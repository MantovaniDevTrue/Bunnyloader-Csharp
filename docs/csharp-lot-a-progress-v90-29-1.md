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
