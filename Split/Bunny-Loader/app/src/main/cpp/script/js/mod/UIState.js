class UIState {
    constructor() {
        this.IsInitialized = false;
        this.IsActive = false;
        this.CapturesMouse = false;
        this.InputRegion = null;
    }

    OnInitialize() {}
    OnActivate() {}
    OnDeactivate() {}
    Update(gameTime) {}
    Draw(spriteBatch, gameTime) {}
    Recalculate() {}
    ContainsPoint(x, y) { return false; }

    GetInputRegion() {
        return typeof this.InputRegion === 'function' ? this.InputRegion() : this.InputRegion;
    }

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
