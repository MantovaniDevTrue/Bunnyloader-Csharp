class UserInterface {
    constructor() {
        this.CurrentState = null;
    }

    get IsVisible() {
        return this.CurrentState !== null;
    }

    SetState(state) {
        if (state === this.CurrentState) return;

        if (this.CurrentState) this.CurrentState.Deactivate();
        this.CurrentState = state || null;
        if (this.CurrentState) this.CurrentState.Activate();
    }

    Update(gameTime) {
        const state = this.CurrentState;
        if (!state || !state.IsActive) return;

        Safe.Run(state.constructor.name + '.Update', () => state.Update(gameTime));
    }

    ReserveInput() {
        const state = this.CurrentState;
        const Main = Terraria.Main;
        if (!state || !state.IsActive || Main.gameMenu) return false;

        let region = Safe.Run(state.constructor.name + '.GetInputRegion', () => state.GetInputRegion());
        if (!region && state.CapturesMouse === true) {
            region = { X: 0, Y: 0, Width: Main.screenWidth, Height: Main.screenHeight };
        }

        if (!region) {
            const over = Safe.Run(state.constructor.name + '.ContainsPoint',
                () => state.ContainsPoint(Main.mouseX, Main.mouseY)) === true;
            if (over && Main.LocalPlayer) Main.LocalPlayer.mouseInterface = true;
            return over;
        }

        const x = Number(region.X !== undefined ? region.X : region.x || 0) | 0;
        const y = Number(region.Y !== undefined ? region.Y : region.y || 0) | 0;
        const width = Math.max(0, Number(region.Width !== undefined ? region.Width : region.width || 0) | 0);
        const height = Math.max(0, Number(region.Height !== undefined ? region.Height : region.height || 0) | 0);
        if (width <= 0 || height <= 0) return false;

        let accepted = true;
        try {
            accepted = GUIInputRegionManager.Instance['bool RegisterInputRegion(Rectangle rect)'](
                Rectangle.new(x, y, width, height));
        } catch (e) {
            Safe.Once('UserInterface.RegisterInputRegion',
                'UserInterface: GUIInputRegionManager indisponivel; usando apenas mouseInterface');
        }

        const mx = Main.mouseX;
        const my = Main.mouseY;
        const over = accepted && mx >= x && mx < x + width && my >= y && my < y + height;
        if (over && Main.LocalPlayer) Main.LocalPlayer.mouseInterface = true;
        return over;
    }

    Draw(spriteBatch, gameTime) {
        const state = this.CurrentState;
        if (!state || !state.IsActive) return;

        Safe.Run(state.constructor.name + '.Draw',
            () => state.Draw(spriteBatch || Terraria.Main.spriteBatch, gameTime));
    }

    CreateInterfaceLayer(name, scaleType = InterfaceScaleType.UI) {
        return new LegacyGameInterfaceLayer(
            name,
            () => {
                this.Draw(Terraria.Main.spriteBatch, null);
                return true;
            },
            scaleType,
            () => this.ReserveInput());
    }

    Recalculate() {
        const state = this.CurrentState;
        if (state && state.IsActive) Safe.Run(state.constructor.name + '.Recalculate', () => state.Recalculate());
    }

    Clear() {
        this.SetState(null);
    }
}
