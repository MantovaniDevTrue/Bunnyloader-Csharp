using BunnyLoader.Source;

namespace ExampleModCSharp;

// Porta do ExamplePickaxe.js. O Terraria executa a mineracao/melee nativos;
// nao e um projetil nem um callback que simula quebrar blocos.
public sealed class ExamplePickaxe : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Pickaxe";
        Texture = "Content/Items/Tools/ExamplePickaxe.png";
        Melee = true;
        Pick = 220;         // Poder da picareta em porcentagem, como no JS.
        Damage = 20;
        KnockBack = 6;
        Crit = 0;           // SetWeaponValues(20,6,0) original.
        UseTime = 10;
        UseAnimation = 10;  // SetDefaultWeaponStyle(10,true) usa ambos iguais.
        UseStyle = 1;       // ItemUseStyleID.Swing do Terraria Android.
        AutoReuse = true;
        Value = 50000;      // sellPrice(0,1,0,0): value interno e 5x o preco de venda.
        Rare = ItemRarityID.Green;           // ItemRarityID.Green.
        UseSound = "Item1"; // O host atribui o LegacySoundStyle real de SoundID.
    }

    public override void AddRecipes()
    {
        // nameof resolve a classe deste mod; nao grave IDs de mod fixos.
        CreateRecipe().AddIngredient(nameof(ExampleItem), 10)
            .AddTile(BunnyVanillaTiles.WorkBenches).Register();
    }
}
