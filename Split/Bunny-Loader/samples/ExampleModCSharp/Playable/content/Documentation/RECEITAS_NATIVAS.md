# Receitas C# — .90.29.1 / ExampleMod 0.1.36

Os fontes ExampleItem, ExamplePickaxe e ExampleHamaxe mostram a API executada
pelo MonoVM. CreateRecipe/AddIngredient/AddTile/Register produzem o descritor;
o host registra Terraria.Recipe na thread de update, sem executar a API JS.
Register nao significa que a receita ja entrou no menu: a confirmacao nativa
espera o setup do jogo, dependencias e os indices de materiais/crafting.

```csharp
public override void AddRecipes()
{
    CreateRecipe()                        // Uma unidade deste item como resultado.
        .AddIngredient("ExampleItem", 10) // Classe do mesmo mod; o host qualifica com seu UID.
        .AddTile(BunnyVanillaTiles.WorkBenches) // TileID vanilla, nao indice da paleta do mapa.
        .Register();                      // Enfileira; o Terraria confirma depois de recriar lookups.
}
```

Mod independente pode usar AddIngredient("UID/Classe", count) e
AddTile("UID/Estacao") para conteudos de outro mod JS ou C#. Use o UID do
manifest, nao o nome de exibicao. Nunca grave IDs dinamicos entre sessoes.
O registro nativo resolve as chaves e aguarda dependencias carregadas depois.
Nao copiar IDs de exemplo: sao atribuidos conforme a ordem de carregamento.

O AddTile(int) aceita os 754 tipos vanilla. AddTile(string) e para mod tiles.
Uma receita tem uma estacao; omitir AddTile permite fabricar sem estacao.
Cada item aceita ate 16 receitas, cada receita ate 15 ingredientes. Quantidade
do resultado nao pode exceder MaxStack real. Quantidade de ingrediente tambem
e validada contra seu MaxStack nativo; ela nao e silenciosamente reduzida.

Diagnosticos indicam mod, classe, arquivo, receita, item, estacao e etapa.
Dependencia/setup pendentes geram WARN e permanecem na fila para tentar de novo;
falha real de metadata/registro gera ERROR. Uma receita defeituosa nao impede
as independentes. Nao ha ACK antes dos lookups nem duplicacao no retry.

Limites atuais: a bancada C# propria ainda esta pendente. A API AddTile(string)
resolve estações ja registradas no core; nao cria o tile por si mesma.
Equipment policies ainda utilizam a bridge JS; isto nao e independência de todo
o SDK. Recipes acima de Recipe.maxRecipes ainda precisam de suporte nativo do
menu/Guia/variantes, hoje existente no loader JS; o servico C# recusa o excedente
com diagnostico explicito. Nenhuma dessas pendencias e “limitacao comprovada”
do Terraria, nem uma porta completa deve ser inferida a partir desta API.

No Android: com terra e bancada vanilla, fabricar ExampleItem; com esse material,
fabricar Pickaxe/Hamaxe. Conferir menu/Guia, pilhas, Save/Exit e reabertura.
Testar dependencias intermod e ordem inversa, observando os logs por receita.
O teste sem QuickJS foi executado no host IL2CPP controlado, nao no celular.
Esta build nao recebe garantia nova de 60 FPS sem medicao real no Android.

Na .90.29.2, os itens tambem validam os offsets contra o tamanho real da
instancia, e structs de som usam metadata nativa independente da bridge JS.
O teste de offsets nao equivale a colocar uma bancada nem a validar gameplay.
