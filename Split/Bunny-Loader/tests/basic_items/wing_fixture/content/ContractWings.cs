using BunnyLoader.Source;
public sealed class ContractWings : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/ContractWings"; Accessory=true;
        Behavior.EquipTexture(BunnyEquipKind.Wings,"Content/ContractWings_Wings");
        Behavior.SetWingStats(180,9,2.5f,.85f,.15f,1,3,.135f);
    }
}
