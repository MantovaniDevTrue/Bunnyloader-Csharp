using BunnyLoader.Source;
public sealed class ContractHelmet : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Helmet"; Defense=5;
        var set=new BunnyArmorSet(nameof(ContractHelmet),nameof(ContractPlate),nameof(ContractLegs),"+20% damage");
        set.Effects.Add(BunnyArmorStat.MeleeDamage,0.2f);
        set.Effects.Add(BunnyArmorStat.RangedDamage,0.2f);
        set.Effects.Add(BunnyArmorStat.MagicDamage,0.2f);
        set.Effects.Add(BunnyArmorStat.MinionDamage,0.2f);
        Behavior.DefineArmorSet(set);
    }
    public override void AddRecipes() => CreateRecipe().AddIngredient(2).AddTile("ContractBench").Register();
}
