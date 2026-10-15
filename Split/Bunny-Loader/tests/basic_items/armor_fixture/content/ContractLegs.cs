using BunnyLoader.Source;
public sealed class ContractLegs : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Legs"; Defense=5;
        Behavior.Armor.Add(BunnyArmorStat.MoveSpeed,0.05f);
    }
}
