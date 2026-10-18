const { ItemID, TileID, SoundID } = Terraria.ID;

export class NativeWaveWand extends ModItem {
    SetDefaults() {
        this.Item.ranged = true;
        this.Item.shoot = ModProjectile.getTypeByName('NativeWaveBolt');
        this.Item.shootSpeed = 14;
        this.SetWeaponValues(22, 2, 0);
        this.SetDefaultWeaponStyle(18, true);
        this.Item.noMelee = true;
        this.Item.rare = ItemRarityID.Green;
        this.Item.UseSound = SoundID.Item5;
    }

    AddRecipes() {
        this.CreateRecipe()
            .AddIngredient(ItemID.Wood, 10)
            .AddTile(TileID.WorkBenches)
            .Register();
    }
}
