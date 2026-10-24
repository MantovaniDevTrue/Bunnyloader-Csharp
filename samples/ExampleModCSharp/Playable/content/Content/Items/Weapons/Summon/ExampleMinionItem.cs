using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinionItem : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example Minion Staff";
        Texture = "Content/Items/Weapons/Summon/ExampleMinionItem.png";
        Damage = 30;
        UseTime = 36;
        ShootSpeed = 0f;
        // Mesmo limite nativo do JS: a mira não permite invocar fora do alcance do jogador.
        SpawnAtCursor = true;
        NativeDefaults.SetInt("mana",10).SetFloat("knockBack",3).SetInt("width",32).SetInt("height",32)
            .SetInt("useAnimation",36).SetId("useStyle","ItemUseStyleID","Swing")
            .SetInt("value",BunnyItem.SellPrice(gold:30)).SetId("rare","ItemRarityID","Cyan")
            .SetBool("autoReuse",false).SetVanillaUseSound("Item44")
            .SetItemStaticBool("GamepadWholeScreenUseRange",true).SetItemStaticBool("LockOnIgnoresCollision",true)
            .SetStaffMinionSlotsRequired(1);
        Shoot = nameof(ExampleMinion);
        Summon = true;
        MinionBuff = nameof(ExampleMinionBuff);
        MinionBuffTime = 2;
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient<ExampleItem>()
            .AddTile(18).Register();
    }
}
