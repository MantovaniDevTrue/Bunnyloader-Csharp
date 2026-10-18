# Buffs e receitas — ExampleMod C# 0.1.49 / Bunny .90.29.90

Este pacote é um checkpoint experimental. Não representa a conclusão de todos os
buffs, minions, montarias ou exemplos do lote. A .89 permanece a base validada no
Android; estas novas bridges ainda precisam de execução no Terraria.

## Receita da bancada C#, com C# de verdade

Leia `Content/Items/Placeable/Furniture/ExampleWorkbench.cs`. A receita original e
as duas alternativas da .89 permanecem. A quarta receita demonstra:

```csharp
var materials = new BunnyRecipeGroup("ExampleMaterials", "Any Example material")
    .AddItem<ExampleModCSharp.ExampleItem>().AddItem("ExampleSoul");
CreateRecipe().AddRecipeGroup(materials, 10).AddNativeRecipeGroup("Wood", 10)
    .AddCondition("Night and low health", c => !c.IsDay && c.CurrentLife < c.MaxLife / 2)
    .AddConsumeIngredientCallback((c, type, amount) => type == 9 ? 0 : amount)
    .AddOnCraftCallback(c => c.GiveItem<ExampleModCSharp.ExampleItem>())
    .Register();
```

O núcleo cria um `Terraria.RecipeGroup` real, registra seus membros e instala
`Recipe.RequireGroup`. O nome pertence ao mod; IDs são resolvidos no registro
compartilhado, sem fixar o número de ExampleItem/ExampleSoul. A descrição é texto
UTF-8. Reutilizar o mesmo nome com membros diferentes é recusado: não altera um
grupo que outra receita já utiliza. O grupo é instalado quando uma receita o usa.

O predicado e os callbacks são métodos/delegates C# executados no worker MonoVM.
O snapshot contém vida atual/máxima, dia/noite, deserto, neve, água próxima, tipo
resultante, quickCraft e quantidade disponível do ingrediente. Não contém objetos
vivos do Terraria. Um mod somente de itens também inicializa esse worker.

`AddConsumeIngredientCallback` recebe `(contexto, tipo, quantidade)` e retorna a
quantidade a consumir. O núcleo consulta a disponibilidade e limita a resposta aos
materiais realmente disponíveis. Zero conserva o ingrediente. Para grupos, o
`tipo` é o placeholder do grupo (madeira: ItemID.Wood, 9), como no Bunny JS.

`AddOnCraftCallback` executa após a concessão normal do resultado. `GiveItem`
(int vanilla, chave local/UID-Nome ou tipo C#) solicita concessões nativas na thread
do Terraria. Até oito comandos são validados antes de qualquer concessão. Isso
não implementa ainda spawn de fogos, consumedItems ou destinationStack do PC.

Há orçamento de até 3 ms para uma resposta; no filtro, o orçamento é compartilhado
pelas receitas. Um timeout oculta a receita condicionada, conserva o consumo
original ou deixa de aplicar o efeito adicional e gera diagnóstico limitado.
Inicialização dos handlers ocorre antes de reconhecer a receita como instalada.
Não bloquear callbacks nem colocar I/O pesado neles.

`Clone()` copia as listas de ingredientes, condições e callbacks. Adicionar ou
remover algo na cópia não modifica o descritor original. Referências capturadas
por um delegate continuam sendo referências C# normais. `RemoveRecipeGroup(nome)`
remove o grupo personalizado do descritor; não altera grupos já registrados no jogo.

### Teste no Terraria: sempre a ExampleWorkbench

1. Com ExampleMod habilitado, obtenha Example Items, Example Souls e madeiras.
2. Confirme as três receitas anteriores da bancada C#.
3. Para a quarta receita, tenha dez materiais do grupo ExampleMaterials e dez
   madeiras. Fique à noite com menos da metade da vida; a receita deve aparecer.
4. Durante o dia ou com metade da vida ou mais, essa receita deve desaparecer.
5. Crie a ExampleWorkbench: os materiais de exemplo são consumidos, a madeira
   permanece e o callback concede um Example Item adicional.
6. Repita usando Example Souls no grupo e outra variante de madeira.
7. Coloque/quebre a bancada, use as receitas de armadura, salve e volte ao mundo.
8. Habilite também um mod JavaScript; confira se suas receitas continuam normais.

## Buffs e debuffs

`BunnyBuff` expõe `Description`, `IsDebuff`, `NoSave`, `NoTimeDisplay`,
`NurseCannotRemove`, `DefenseBonus`, `DefenseMultiplierPercent` e
`MinionProjectile`. Flags e descrição vêm dos valores executados em `SetDefaults`,
inclusive condições e métodos auxiliares. Para defaults arbitrários e herança,
use o contrato de identidade `[BunnyContent(...)]` documentado no SDK.

`{0}` na descrição é substituído pelo bônus de defesa. Outros textos podem ser
calculados normalmente pelo mod. `DefenseMultiplierPercent=75` multiplica a
defesa após os equipamentos, demonstrado em `Content/Buffs/ExampleDefenseDebuff.cs`.
Não acumula uma redução permanente: o Terraria reconstrói os atributos a cada tick.

Teste `ExampleDebuffPotion`: observe a defesa antes, use a poção e confira a
redução de 25% por dez segundos. Depois confirme que a defesa volta ao valor
normal. Verifique também troca de armadura, morte, respawn e remoção do debuff.
A poção é uma demonstração adicional do SDK; reutiliza a textura da ExampleBuffPotion.

O processamento visita todos os slots ativos, e não o índice do jogador passado
a `Player.UpdateBuffs`. Remoções usam o `Player.DelBuff` real, percorrendo os slots
de trás para frente para não pular buffs deslocados.

## Pendências explícitas

- ExampleDefenseBuff: texto dinâmico/localizado do tempo restante.
- ExampleMinionBuff: contador no tooltip; minion e cajado com IA, alvo manual,
  colisão, animação, iluminação e summon slots ainda não estão portados.
- Blocky: estado do jogador/acessório, condição de moradores, movimento e visuais.
- ExampleMountBuff e ExampleMinecartBuff: registro nativo de montarias/carrinhos,
  movimento, sons, efeitos, animações e dados por jogador.
- Receitas: merge/edição de grupos existentes, múltiplas estações, condições com
  todo o estado do jogo, fogos originais, shimmer e alterações globais.

Nenhuma dessas pendências é declarada limitação comprovada do Android. Elas são
trabalho de implementação ainda necessário. Não usar o sucesso do CI como prova
de funcionamento ou de 60 FPS no dispositivo.

## Continuidade .91

O ExampleMod 0.1.50 demonstra os mesmos hooks numa classe de comportamento.
Consulte `RECIPE_LOADER_V91.md` para a API, os estágios e o teste da bancada C#.
As pendências deste documento continuam no escopo; não representam limitação
comprovada do Android.
