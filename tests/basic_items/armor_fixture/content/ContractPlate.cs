using BunnyLoader.Source;
public sealed class ContractPlate : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Plate"; Defense=6;
        Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2,20);
        Behavior.Armor.Add(BunnyArmorInteger.MaxMinions,1);
        Behavior.Armor.ImmuneTo("OnFire");
    }
}
