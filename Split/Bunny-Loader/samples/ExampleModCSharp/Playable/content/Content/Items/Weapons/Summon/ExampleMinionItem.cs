using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinionItem : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName="Example Minion Staff";
        Texture="Content/Items/Weapons/Summon/ExampleMinionItem.png";
        Damage=30;UseTime=36;ShootSpeed=8f;Shoot=nameof(ExampleMinion);
        // Vinculação por nome, nunca por ID temporário. O host aplica o buff ao spawn.
        Summon=true;MinionBuff=nameof(ExampleMinionBuff);MinionBuffTime=2;
    }
    public override void AddRecipes()
    {
        // O novo dono summon usa o mesmo loader público da varinha/acessório/poção.
        CreateRecipe().AddIngredient<ExampleItem>()
            .AddTile<ExampleTiles.Furniture.ExampleWorkbench>().Register();
    }
}
