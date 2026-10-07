export class ExampleSecondaryUsePlayer extends ModPlayer {
    PostUpdateMiscEffects(player) {
        if (player.whoAmI !== Terraria.Main.myPlayer || Terraria.Main.gameMenu) return;

        const secondary = ModInput.SecondaryUse;
        if (!secondary.JustPressed) return;

        const aim = ModInput.SyncAim(player);
        bl.log('SecondaryUse: aim ' + aim.X.toFixed(3) + ', ' + aim.Y.toFixed(3));
    }
}
