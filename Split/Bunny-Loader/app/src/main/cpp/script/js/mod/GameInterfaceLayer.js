class GameInterfaceLayer {
    constructor(name, drawMethod, scaleType = InterfaceScaleType.UI, reserveInput = null) {
        this.Name = String(name || '');
        this.ScaleType = scaleType;
        this.Active = true;
        this.IsVanilla = false;
        this._drawMethod = typeof drawMethod === 'function' ? drawMethod : null;
        this._reserveInput = typeof reserveInput === 'function' ? reserveInput : null;
    }

    ReserveInput() {
        if (!this.Active || !this._reserveInput) return false;

        return Safe.Run(this.Name || 'InterfaceLayer', () => this._reserveInput()) === true;
    }

    Draw() {
        if (!this.Active || !this._drawMethod) return true;

        const result = Safe.Run(this.Name || 'InterfaceLayer', () => this._drawMethod());
        return result !== false;
    }

    static Marker(name) {
        const layer = new GameInterfaceLayer(name, null, InterfaceScaleType.UI);
        layer.IsVanilla = true;
        return layer;
    }
}

class LegacyGameInterfaceLayer extends GameInterfaceLayer {}

class InterfaceLayerList extends Array {
    Add(layer) {
        this.push(layer);
    }

    Insert(index, layer) {
        const at = Math.max(0, Math.min(this.length, Number(index) | 0));
        this.splice(at, 0, layer);
    }

    Remove(layer) {
        const index = this.indexOf(layer);
        if (index < 0) return false;

        this.splice(index, 1);
        return true;
    }

    RemoveAt(index) {
        if (index < 0 || index >= this.length) return;

        this.splice(index, 1);
    }

    FindIndex(predicate) {
        return this.findIndex(predicate);
    }
}
