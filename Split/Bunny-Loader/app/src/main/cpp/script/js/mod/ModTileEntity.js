class ModTileEntity {
    ID = -1;
    Position = null;
    Type = 0;
    Mod = undefined;

    IsTileValidForEntity(i, j) { return true; }
    Update() {}
    SaveData(tag) {}
    LoadData(tag) {}
    NetSend(writer) {}
    NetReceive(reader) {}
    OnNetPlace() {}
    OnKill() {}

    Place(i, j) {
        return TileEntityLoader.Place(this.Type, i, j);
    }

    Kill(i, j) {
        return TileEntityLoader.Kill(i, j);
    }

    static Find(i, j) {
        return TileEntityLoader.At(i, j);
    }

    static get ByID() {
        return TileEntityLoader.ByID;
    }

    static get ByPosition() {
        return TileEntityLoader.ByPosition;
    }

    static getTypeByName(name) {
        for (const entity of TileEntityLoader.ByType.values()) {
            if (entity.constructor.name === name && (!bl.mod || entity.Mod === bl.mod)) return entity.Type;
        }
        return 0;
    }

    static getModTileEntity(type) {
        return TileEntityLoader.ByType.get(type);
    }

    static register(cls) {
        if (typeof cls !== 'function' || !(cls.prototype instanceof ModTileEntity)) {
            throw new TypeError('ModTileEntity.register(Classe): passe a classe, que estende ModTileEntity');
        }

        const inst = new cls();
        Templates.Adopt(cls, inst);
        return TileEntityLoader.Add(inst);
    }
}
