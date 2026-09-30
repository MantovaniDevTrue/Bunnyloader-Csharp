class UIState {
    constructor() {
        this.IsInitialized = false;
        this.IsActive = false;
        this.CapturesMouse = false;
    }

    OnInitialize() {}
    OnActivate() {}
    OnDeactivate() {}
    Update(gameTime) {}
    Draw(spriteBatch, gameTime) {}
    Recalculate() {}
    ContainsPoint(x, y) { return false; }

    Activate() {
        if (!this.IsInitialized) {
            this.IsInitialized = true;
            this.OnInitialize();
            this.Recalculate();
        }
        if (this.IsActive) return;

        this.IsActive = true;
        this.OnActivate();
    }

    Deactivate() {
        if (!this.IsActive) return;

        this.IsActive = false;
        this.OnDeactivate();
    }
}
