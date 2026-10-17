# Bunny C# .90.29.89 — grupos nativos e composicao de receitas

Base: .88.8, codigo af844936dfa0b9435388723d02724daa5024985f e documentacao e74d3c510295ffa66272756886600a9fc9579c8d. Branch: feat/csharp-native-recipe-groups-v89. Preservar a .88.8, seus contratos de cena, Lights 0.4.2.3, Mono/AOT, JavaScript, prefixos, equipamentos, receitas e IDs persistidos.

## APIs publicas implementadas

- BunnyRecipe.AddNativeRecipeGroup(string groupField, int count): um ingrediente alternativo de qualquer campo suportado de Terraria.ID.RecipeGroups. Resolve por metadata e chama RequireGroup no objeto Recipe real; nao fixa IDs nem escolhe somente a primeira variante.
- AddIngredient<TItem> e AddTile<TTile>: validam classe concreta/categoria, inclusive BunnyContentAttribute, e serializam a identidade local. UID/Nome continua sendo o contrato entre mods.
- Clone: copia independente de uma receita declarada pelo BunnyRecipe, mesmo apos Register; preserva resultado/stack/estacao e exige Register da copia.
- RemoveIngredient (vanilla/chave/tipo) e RemoveNativeRecipeGroup: retornam bool e recusam alteracoes de descritor ja registrado.

A nova representacao gNome:quantidade e aditiva ao wire V2. Todos os descritores antigos continuam aceitos. O fingerprint do compilador ja inclui a identidade das referencias/SDK; as alteracoes do SDK e dos fontes invalidam o cache correspondente. Nao foram alterados IDs persistidos.

## Bridge e seguranca

NativeRecipes le o campo estatico RecipeGroups, confere o tipo RecipeGroup e as assinaturas GetPlaceholderItemType/RequireGroup. O retorno boxing Int32 e validado por tipo, tamanho, layout normalizado de m_value e tamanho da instancia antes da leitura. Nao depende de um export object_unbox inexistente na camada atual do Bunny. Contratos ficam em cache; nao ha callback Mono nem busca de classes por frame. Roots de GC sao liberadas no sucesso e nas falhas.

A fila BasicItems transporta o grupo ao servico independente de QuickJS. Grupo ainda nulo fica pendente ate o setup. Metadata incorreta, retorno incorreto, falta de exports, grupo duplicado e excecoes recebem diagnosticos concretos. Falha antes do commit descarta a receita nova e preserva currentRecipe; os testes incluem receita independente apos erro e retry sem duplicacao.

## Exemplo e cobertura

ExampleMod CSharp 0.1.48 conserva a receita original da ExampleWorkbench e acrescenta duas receitas documentadas: dez madeiras aceitas pelo grupo Wood + dez Example Items; ou duas barras aceitas por IronBar + dez Example Items. A segunda usa Clone e remocao do grupo Wood. O teste Android e sobre a bancada C# real, com assets e tile ja existentes; nao cria um item vanilla ou uma demonstracao visual adicional.

Referencia oficial lida: tModLoader/tModLoader 130d914e622c1db30007e647d311cb00e4f7a3d1, Content/ExampleRecipes.cs. Este arquivo continua PARCIAL. Grupos customizados/merge/membros, multiplas estacoes, condicoes, callbacks, shimmer e PostAddRecipes ainda nao foram portados. Nao se considera o lote A concluido nem um novo exemplo oficial integralmente portado por adicionar essas APIs.

## Validacao

Local: testes executaveis do servico e da fila de producao sem QuickJS; grupos/quantidades, inicializacao tardia, layout boxed/raw, erro de tipo/tamanho/assinatura/export, excecoes e isolamento passaram. GCC 14 do host deprecia as operacoes atomic/shared_ptr antigas do runtime; a supressao local limita-se a esse aviso de codigo preservado. CI mantem -Wall -Wextra -Werror no toolchain do workflow.

CI previsto: compilar fonte C# do fixture e executar o wire no pump real; fixtures de tipos genericos; referencia publica gerada por reflection; regressao completa, nove suites independentes, compilacao/assinatura Android. A entrega e experimental e depende da aprovacao desses gates. Estado Android: aguardando validacao real. Nao afirmar consumo alternativo, estabilidade, <=5 s ou 60 FPS no Xiaomi antes do teste.

Roteiro e explicacoes dentro do bmod: samples/ExampleModCSharp/Playable/content/Documentation/RECEITAS_NATIVAS.md.

## Continuidade

Continuar pelas lacunas de receitas compartilhadas da referencia oficial e pelo Lote A, auditando o que ja existe antes de implementar. O inventario dos 449 fontes ativos/112 legados permanece completo e separado da matriz JavaScript de 206 arquivos. Atualizar a linha funcional ExampleRecipes sem promover outros arquivos por nome/compilacao.
