// ExampleMod C# | Content/Items/Weapons/ExampleWand.cs
// Arma: Shoot usa nameof da classe de projetil; Texture e um caminho dentro de content/.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 18;
        UseTime = 20;
        ShootSpeed = 11f;
        Shoot = nameof(ExampleBolt);
        SetRarity<ExampleModRarity>();
    }
    public override void ConfigurePrefixRules(BunnyItemPrefixRules rules)
    {
        rules.Deny(71);
        rules.Deny(nameof(ExampleGuardedPrefix));
        rules.DenyWhenStackBetween(72, 3, 5);
        rules.DenyWhenDamageBetween(73, 10, 25);
        rules.DenyWhenManaBetween(nameof(ExampleGuardedPrefix), 2, 8);
        rules.SetPrefixChanceWhenStackBetween(3, 5, true);
        rules.ChooseWhenStackBetween(nameof(ExampleMightyPrefix), 1, 2);
    }

    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix)
        => prefix != 74 || item.Damage >= 20;

    public override void AddRecipes()
    {
        // A mesma API atende armas, acessórios e poções. O tipo resolve a bancada
        // C# deste mod; nenhum ID temporário do inventário é persistido na receita.
        CreateRecipe().AddIngredient<ExampleItem>(10)
            .AddTile<ExampleTiles.Furniture.ExampleWorkbench>()
            .AddBehavior(new Recipes.ExampleCraftingReward()).Register();
    }
}

