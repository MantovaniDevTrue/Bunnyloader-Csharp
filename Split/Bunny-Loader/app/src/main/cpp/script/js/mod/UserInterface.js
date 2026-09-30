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

        const Main = Terraria.Main;
        if (Main.gameMenu) return;

        let capture = state.CapturesMouse === true;
        if (!capture && typeof state.ContainsPoint === 'function') {
            capture = Safe.Run(state.constructor.name + '.ContainsPoint',
                () => state.ContainsPoint(Main.mouseX, Main.mouseY)) === true;
        }

        if (capture && Main.LocalPlayer) Main.LocalPlayer.mouseInterface = true;
    }

    Draw(spriteBatch, gameTime) {
        const state = this.CurrentState;
        if (!state || !state.IsActive) return;

        Safe.Run(state.constructor.name + '.Draw',
            () => state.Draw(spriteBatch || Terraria.Main.spriteBatch, gameTime));
    }

    Recalculate() {
        const state = this.CurrentState;
        if (state && state.IsActive) Safe.Run(state.constructor.name + '.Recalculate', () => state.Recalculate());
    }

    Clear() {
        this.SetState(null);
    }
}
