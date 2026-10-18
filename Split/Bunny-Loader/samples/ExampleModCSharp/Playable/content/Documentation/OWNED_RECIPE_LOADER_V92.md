# Loader de receitas próprio — ExampleMod 0.1.51

`BunnyRecipeSource` reúne `CreateRecipe`, `AddRecipes` e as definições de
`BunnyItem`, `BunnyWeapon`, `BunnyAccessory` e `BunnyBuffPotion`. Não é uma
categoria de conteúdo para registrar diretamente. As quatro classes conservam
suas identidades anteriores e usam o mesmo serviço nativo de receitas.

Abra `Content/Items/Weapons/ExampleWand.cs`,
`Content/Items/Accessories/ExampleFocusAccessory.cs` e
`Content/Items/Consumables/ExampleBuffPotion.cs`. Os três demonstram
`AddIngredient<ExampleItem>(10)`, `AddTile<ExampleTiles.Furniture.ExampleWorkbench>()`
e `Content/Recipes/ExampleCraftingReward.cs`, compartilhado sem alterar o núcleo.
O callback solicita um ExampleItem adicional após fabricar; a concessão ocorre
na thread do Terraria. Não há dependência de assemblies tModLoader nem execução
QuickJS nesse percurso. O endpoint anterior BunnyRecipeRuntime permanece compatível.

## Testar os exemplos no Terraria Android

1. Atualize APK e este pacote juntos. Use um mundo de teste e desative probes antigos.
2. Pegue **ExampleItem** e a **ExampleWorkbench do C#** no catálogo do ExampleMod.
   Coloque a bancada; não use uma bancada vanilla para este teste.
3. Com pelo menos 30 ExampleItem, fique junto dela: devem aparecer as receitas
   **Example C# Wand**, **Example Focus Accessory** e **Example Defense Potion**.
4. Fabrique cada um. Cada fabricação gasta dez ExampleItem e o callback devolve
   um ExampleItem; confirme o saldo em cada operação (saldo líquido -9).
5. Afaste-se da bancada: essas três receitas não devem continuar disponíveis.
6. Teste a varinha contra um NPC, equipe/remova o Focus e use a poção. Confirme
   os efeitos já existentes, tooltips e texturas; o loader não substitui esses sistemas.
7. Salve/saia, reentre e repita a fabricação na mesma bancada. Confirme que ela
   permaneceu no mundo, sem duplicar receitas ou recompensas.
8. Repita com seu mod JS habilitado e verifique o relatório/logcat. Registre a
   versão, o item, a operação e o diagnóstico completo se houver falha.

Os testes automatizados executam compilação C#, reconstrução/callbacks Mono
em host .NET e a fila/serviço C++ com metadados IL2CPP controlados, QuickJS=0.
Isso NÃO comprova ABI, gameplay ou FPS do dispositivo. Todos esses testes Android
continuam pendentes para .92. Este pacote não conclui o Lote A nem todos os loaders.
Condições e consumo personalizado continuam demonstrados nas quatro receitas da
bancada e no guia `RECIPE_LOADER_V91.md`.
