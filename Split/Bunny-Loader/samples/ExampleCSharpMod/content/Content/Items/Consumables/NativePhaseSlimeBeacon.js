const { ItemID, TileID, ItemUseStyleID } = Terraria.ID;

export class NativePhaseSlimeBeacon extends ModItem {
    SetDefaults() {
        this.Item.width = 30;
        this.Item.height = 20;
        this.Item.maxStack = 1;
        this.Item.useTime = 25;
        this.Item.useAnimation = 25;
        this.Item.useStyle = ItemUseStyleID.HoldUp;
        this.Item.rare = ItemRarityID.Green;
        this.Item.consumable = false;
    }

    CanUseItem(item, player) {
        return Terraria.Main.netMode === 0 &&
            !Terraria.NPC.AnyNPCs(ModNPC.getTypeByName('NativePhaseSlime'));
    }

    UseItem(item, player) {
        if (player.whoAmI !== Terraria.Main.myPlayer) return true;
        const type = ModNPC.getTypeByName('NativePhaseSlime');
        Terraria.NPC.SpawnOnPlayer(player.whoAmI, type, 0, 0, 0, 0);
        return true;
    }

    AddRecipes() {
        this.CreateRecipe()
            .AddIngredient(ItemID.Wood, 10)
            .AddTile(TileID.WorkBenches)
            .Register();
    }
}
