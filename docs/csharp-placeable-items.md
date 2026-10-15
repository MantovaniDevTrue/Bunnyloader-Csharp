# Colocacao de tiles por itens C#

API: `BunnyItem.DefaultToPlaceableTile(int vanillaTileId, int style = 0)` ou
`DefaultToPlaceableTile(string modTileName, int style = 0)`.

O metodo implementa os defaults do Bunny original: createTile/placeStyle,
UseStyle=1, UseAnimation=15, UseTime=10, MaxStack=9999, UseTurn, AutoReuse e
Consumable. Propriedades podem ser ajustadas depois da chamada. O desenvolvedor
nao precisa compilar DLL nem escrever bridge. As chamadas sao executadas pelo
MonoVM no SetDefaults e serializadas pelo compilador automatico.

```csharp
public sealed class MyStationItem : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/MyStationItem";
        // Chave nativa do tile de outro mod. Nao e um ID persistido fixo.
        DefaultToPlaceableTile("other-mod-uid/Workbench", 0);
        Width = 28; Height = 14; Value = 150;
    }
    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(36).AddIngredient("MyMaterial", 10).Register();
    }
}
```

O tile precisa existir: esta API vincula um item a um tile e nao cria o tile.
Uma chave local e qualificada pelo UID do mod; UID/Nome consulta o registro
nativo compartilhado com JS. Nenhuma chamada QuickJS e feita para resolucao,
SetDefaults ou instalacao das receitas. A independencia integral das policies
de equipamentos continua pendente e nao e comprovada por este teste.

Cada campo de colocacao e resolvido na hierarquia real de Item, validado como
inteiro de 8/16/32 bits, campo de instancia e offset dentro de class_instance_size.
createTile precisa aceitar -1. placeStyle respeita tanto o limite publico quanto
a largura nativa. Nao se exige esses campos de mods antigos que nao os utilizam.
As escritas copiam apenas a largura validada; caches antigos sao invalidados.

Dependencia ausente ou ainda nao instalada desativa somente a colocacao daquele
item (`createTile=-1`), registra Mod/Class/File/ContentId/Tile/Stage/Expected/Actual/
Result/Reason e tenta novamente na thread do jogo. O retry usa round-robin de
ate oito pendencias por 120 updates, sem busca de IDs por frame. Quando o tile
fica disponivel, os defaults e previews sao atualizados. Mensagens iguais nao
sao repetidas; campo obrigatorio invalido recusa os defaults antes da publicacao.

Teste executavel: `tests/basic_items/placeable_fixture` compila C# real e gera
wire de 22 campos. `native_recipes_test --placement-wire` aplica esse wire ao
registrador, defaults, fila e receitas de producao sem QuickJS, com metadata
Terraria emulada. Cobre falta/registro tardio de tile e ingrediente, estilo,
consumo, animacao de uso, sentinelas adjacentes, preview e restauracao da
transacao de receitas. Isso nao simula um mundo Android nem prova colocacao,
quebra, persistencia de tiles ou FPS no aparelho.

Pendencia explicita da bancada C#: criar o contrato publico de tile, instalar
Style2x1/CoordinateHeights [18] com a janela readOnlyData correta, housing/mapa,
adjacencia, suporte/frame, drop unico, DustType/NumDust e icone no Guia. Os dois
ExampleWorkbench ainda nao sao ports completos e nao foram promovidos na matriz.


## Teste pronto para importar

`samples/NativePlacementProbe/Playable` gera NativePlacementProbe-v0.1.0.bmod.
Possui tres itens C# completos de teste: bancada vanilla (tile18), plataforma
vanilla estilo0 e plataforma vanilla estilo1 (tile19). Uma terra fabrica dez
unidades de cada item, sem estacao. Os fontes e o guia COMO_TESTAR.md acompanham
o pacote, com PNGs de inventario reutilizados do ExampleModBunny original.

O pacote usa UID independente, nao altera ExampleMod 0.1.36 e nao requer DLLs,
compilador externo ou mod JS. Requer runtime .90.29.5. Apos importar/habilitar,
procurar "Teste C#" no crafting/menu de itens. Validar consumo de uma unidade,
aparencia por estilo, crafting perto da bancada, drops vanilla e persistencia
apos salvar/reabrir. Os drops vanilla sao intencionais: este teste coloca tiles
vanilla, nao implementa a estacao C# propria do Lote A.

Compiler/CompiledAssemblyProbe executam os quatro fontes e geram tres registros.
O --probe-wire usa essa saida real no registro/defaults/receitas nativos de
producao sem QuickJS. O registro antecipado parseia os mesmos fontes e verifica
cada textura; o pacote e validado por manifesto, PNGs e hashes. Testes no host
nao comprovam colocacao, persistencia de mundo ou FPS no Android.
