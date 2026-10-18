# Recipe Loader — Bunny .91 / ExampleMod 0.1.50

A .91 mantém as correções nativas de grupos e OnCraft da .90.3.1. As quatro
receitas da ExampleWorkbench continuam com as mesmas identidades e resultados.
O quarto exemplo agora usa uma classe separada em
`Content/Recipes/ExampleWorkbenchRecipeBehavior.cs`; a bancada permanece em
`Content/Items/Placeable/Furniture/ExampleWorkbench.cs`.

## Classes próprias e hooks reais

```csharp
public sealed class MyRecipeRules : BunnyRecipeBehavior {
    public override string ConditionDescription => "Night and low health";
    public override bool IsAvailable(BunnyCraftContext c)
        => !c.IsDay && c.CurrentLife < c.MaxLife / 2;
    public override int ConsumeIngredient(BunnyCraftContext c, int type, int amount)
        => type == 9 ? 0 : amount;
    public override void OnCraft(BunnyCraftContext c)
        => c.GiveItem<ExampleModCSharp.ExampleItem>();
}
```

Na receita, use `.AddBehavior(new MyRecipeRules())` antes de `.Register()`.
Outros mods devem trocar a referência do item pelo seu próprio tipo ou por uma
chave `UID/Nome`; números temporários de conteúdo de mod não são estáveis.

Apenas os métodos sobrescritos são vinculados. Herança indireta, interfaces e
helpers podem ser usados normalmente. Uma classe sem hooks sobrescritos ou que
esconde um hook com `new` é recusada com classe/método/etapa no diagnóstico.
Vários comportamentos são avaliados na ordem de adição: condições usam AND,
consumo recebe o resultado anterior e OnCraft executa em sequência.

Os hooks passam pelas mesmas bridges reais dos delegates:

- `IsAvailable`: filtro de `Recipe.FindRecipes`.
- `ConsumeIngredient`: `Recipe.GetIngredientCraftingDiscount`, limitado aos itens disponíveis.
- `OnCraft`: após `Main.CraftItem_GrantItem`, usando o contrato nativo validado.

O loader prepara apenas o dono da receita pedida; um erro de reconstrução em
outra classe não impede handlers independentes. O assembly é reutilizado no
worker Mono existente, sem instanciar todos os itens a cada dono.

## Condições reutilizáveis

`BunnyRecipeCondition` aceita descrição e `Func<BunnyCraftContext,bool>`.
`All`, `Any` e `Not` compõem condições; as listas de entrada são copiadas.
Existem condições de dia, noite, deserto, água/pia próxima e neve porque esses
campos já estão conectados ao snapshot nativo. Não representam todos os biomas
ou todos os estados do Terraria.

Use `AddCondition(condition)` e `RemoveCondition(condition)`. A remoção usa a
mesma instância. Clonar uma receita copia sua lista; remover na cópia não altera
a original. Os delegates e instâncias de comportamento conservam a semântica
normal de referências C#: o estado capturado pode ser compartilhado.

As descrições identificam as condições no SDK; a apresentação localizada delas
na interface nativa ainda está pendente. Não é uma nova UI de receitas.

## Estágios e segurança

`context.Stage` informa `Availability`, `Consumption` ou `Craft`. `GiveItem`
só funciona em `Craft`; usar nos outros estágios gera erro explícito, pois não
é seguro conceder itens enquanto o menu consulta a disponibilidade/consumo.
O contexto é um snapshot, sem ponteiros IL2CPP vivos. Até oito comandos de
concessão são pré-validados e aplicados na thread do jogo após resposta válida.

Continuam valendo os limites de 32 condições, 16 callbacks de cada espécie,
16 receitas por item e o orçamento de resposta de até 3 ms. Timeout mantém o
fallback documentado em `BUFFS_E_RECEITAS_V90.md`; não faça I/O nos hooks.
Os testes de host não certificam tempo de resposta ou FPS no Xiaomi.

## Teste no ExampleMod

1. Instale APK .91 e ExampleMod 0.1.50; confirme carregamento sem erros.
2. Obtenha dez Example Items ou Example Souls e dez madeiras.
3. De noite e com menos da metade da vida, confira a quarta receita da bancada C#.
4. De dia ou com metade da vida ou mais, confirme que essa receita desaparece.
5. Fabrique a ExampleWorkbench: material de exemplo consumido, madeira conservada,
   uma bancada e um Example Item adicional concedido pelo OnCraft.
6. Repita usando Souls e outra madeira. Confira as três receitas anteriores.
7. Coloque a bancada C#, fabrique as armaduras do exemplo, salve/saia e reabra.
8. Repita com um mod JS habilitado, verificando receitas, inventário e relatório.

Esses passos ainda aguardam validação no Android para a .91. Não concluem o
port completo de ExampleRecipes, minions, Blocky, montarias ou demais loaders.
O inventário completo dos módulos e as pendências continuam no repositório.
