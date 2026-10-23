using BunnyLoader.Source;

namespace ExampleModCSharp;

// Porta do ExampleHamaxe.js: machado e martelo nativos no mesmo item.
public sealed class ExampleHamaxe : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Hamaxe";
        Texture = "Content/Items/Tools/ExampleHamaxe.png";
        Melee = true;
        Axe = 30;           // Unidade interna: tooltip mostra 150% (30 * 5).
        Hammer = 100;       // Poder do martelo: 100%.
        Damage = 25;
        KnockBack = 6;
        Crit = 0;
        UseTime = 15;
        UseAnimation = 15;
        UseStyle = 1;       // Swing: animacao e hitbox melee sao do jogo.
        AutoReuse = true;
        Value = 50000;
        Rare = ItemRarityID.Green;
        UseSound = "Item1";
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(nameof(ExampleItem), 10)
            .AddTile(BunnyVanillaTiles.WorkBenches).Register();
    }
}
