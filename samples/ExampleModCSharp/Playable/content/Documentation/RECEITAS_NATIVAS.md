# Receitas C# — Bunny .90.29.89 / ExampleMod 0.1.48

As receitas sao descritores C# compilados automaticamente e instalados pelo
nucleo na thread do Terraria. Nenhuma chamada a QuickJS e necessaria para
resolver ingredientes, grupos nativos, estacoes ou registrar a receita.

## Exemplo real: a bancada C#

Em Content/Items/Placeable/Furniture/ExampleWorkbench.cs, a receita original
continua exigindo uma bancada vanilla e dez Example Items. Duas receitas
adicionais demonstram a API Bunny no mesmo item colocavel C#:

```csharp
var wood = CreateRecipe()
    .AddNativeRecipeGroup("Wood", 10) // Qualquer madeira aceita pelo grupo do Terraria.
    .AddIngredient<ExampleModCSharp.ExampleItem>(10); // Conteudo local por tipo, sem ID fixo.
wood.Register();

var metal = wood.Clone(); // Copia independente, mesmo apos registrar a original.
metal.RemoveNativeRecipeGroup("Wood");
metal.AddNativeRecipeGroup("IronBar", 2).Register(); // Ferro OU chumbo, conforme o grupo nativo.
```

Clone preserva o item resultado, a quantidade e a estacao. Copia as listas de
ingredientes; remover algo da copia nao altera a original. So Register da copia
enfileira a nova receita. RemoveIngredient aceita ID vanilla, chave UID/Nome ou
tipo C#; RemoveNativeRecipeGroup remove um grupo. Ambos retornam false quando
nao encontram o ingrediente e recusam alteracoes depois de Register.

AddIngredient<TItem> aceita as categorias de item do SDK e classes declaradas
com BunnyContent de categoria equivalente. AddTile<TTile> aceita BunnyTile ou
declaracao de tile. Use classes concretas do mesmo mod. Para dependencias
externas C# ou JavaScript, use AddIngredient("UID/Classe", quantidade) e
AddTile("UID/Estacao"): o nucleo resolve o registro nativo compartilhado.

## Contratos de registro

AddNativeRecipeGroup recebe o NOME de um campo de Terraria.ID.RecipeGroups,
como Wood ou IronBar. O nucleo confere campo estatico, tipo RecipeGroup e
assinaturas reais, le o item-modelo como Int32 com layout validado, prepara a
quantidade e chama Recipe.RequireGroup. O item-modelo nao substitui o grupo:
e o proprio Terraria que aceita suas variantes e calcula consumo/menu/Guia.

Nome inexistente ou metadata incompativel gera diagnostico por mod, classe,
arquivo, receita, grupo e etapa. Grupo ainda nulo aguarda o setup do jogo.
Excecoes nao publicam a receita parcialmente montada; registros independentes
continuam. A confirmacao so ocorre depois dos lookups nativos, sem duplicar
receitas no retry. Nao grave IDs de itens, tiles ou grupos entre sessoes.

Uma receita ainda possui uma estacao nativa, ate 15 ingredientes, e cada item
ate 16 receitas. Ingredientes de grupo contam nesse limite. Quantidades sao
conferidas com os limites reais do item-modelo; nao sao reduzidas silenciosamente.
Grupos repetidos na mesma receita sao recusados; informe sua quantidade total.

## Teste Android com ExampleMod

1. Desative o antigo NativePlacementProbe; mantenha o ExampleMod CSharp atualizado.
2. Obtenha Example Items e dez unidades de uma madeira aceita pelo grupo.
   Fabrique a Example Workbench C# pela receita de madeira e confira o consumo.
3. Repita com outra madeira aceita pelo grupo; a mesma receita deve funcionar.
4. Sem madeira, use dois Iron Bars e dez Example Items; repita com Lead Bars.
   Confira que essa receita nao exige nem consome madeira.
5. Coloque a bancada C#, fabrique suas armaduras de exemplo e recupere a bancada.
   Salve, saia e entre novamente; confirme receitas, itens, prefixos e tile.
6. Confira o Guia e o relatorio: receitas instaladas pelo nucleo, sem erro em
   ResolveRecipeGroup/RequireGroup/RebuildLookups, e sem duplicacao na reentrada.

Os testes do host executam o servico e a fila de producao sem QuickJS, com um
ambiente IL2CPP controlado. CI tambem compila e serializa o fixture C# real antes
de enviar seu wire a fila nativa. Isso nao equivale a executar o Terraria Android
nem comprova FPS ou primeira compilacao em ate cinco segundos.

## Pendencias da referencia oficial

Este grupo ainda nao porta integralmente Content/ExampleRecipes.cs do tModLoader:
criacao/mesclagem de grupos customizados, alteracao de membros de grupos, multiplas
estacoes, condicoes de ambiente/delegates, callbacks de consumo/OnCraft, shimmer
e modificacao das receitas globais permanecem pendentes. Clone aqui copia apenas
receitas declaradas pelo proprio BunnyRecipe, nao receitas vanilla existentes.
Nenhuma dessas lacunas foi marcada como limitacao comprovada do Terraria.

A bancada C# ja existe; AddTile por si so nao registra um tile. Equipamentos e
outros sistemas conservam seus contratos e auditorias de independencia proprios.
Receitas acima do limite nativo do menu ainda sao recusadas com diagnostico;
nao se presume que hooks do RecipeLoader JavaScript estejam disponiveis ao C#.

## Evolução .90 / ExampleMod 0.1.49

As três receitas acima estão preservadas. A quarta receita adiciona um grupo
personalizado, uma condição C# e callbacks reais. Leia
[BUFFS_E_RECEITAS_V90.md](BUFFS_E_RECEITAS_V90.md) e os comentários na ExampleWorkbench.
A tabela de limitações anterior descreve a entrega .89; o novo guia distingue o
que foi implementado na .90 do que permanece pendente.
