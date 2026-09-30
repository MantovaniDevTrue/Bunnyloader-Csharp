class TileEntityLoader {
    static ByType = new Map();
    static ByID = new Map();
    static ByPosition = new Map();

    static #byKey = new Map();
    static #keyByType = new Map();
    static #nextType = 1;
    static #nextID = 0;
    static #saveKey = '__bunnyTileEntitiesV1';

    static Add(template) {
        const key = (template.Mod ? template.Mod.uuid : 'sem-mod') + '/' + template.constructor.name;
        if (TileEntityLoader.#byKey.has(key)) {
            throw new TypeError('ModTileEntity.register: ja existe ' + key);
        }

        const type = TileEntityLoader.#nextType++;
        template.Type = type;

        TileEntityLoader.ByType.set(type, template);
        TileEntityLoader.#byKey.set(key, template);
        TileEntityLoader.#keyByType.set(type, key);
        TileEntityLoader.Install();
        return type;
    }

    static At(i, j) {
        return TileEntityLoader.ByPosition.get(TileEntityLoader.#PositionKey(i, j));
    }

    static Place(which, i, j) {
        const template = TileEntityLoader.#Template(which);
        if (!template) return -1;

        i = Number(i) | 0;
        j = Number(j) | 0;

        const existing = TileEntityLoader.At(i, j);
        if (existing) return existing.ID;

        const entity = TileEntityLoader.#Create(template, TileEntityLoader.#nextID++, i, j);
        const valid = Safe.Run(entity.constructor.name + '.IsTileValidForEntity',
            () => entity.IsTileValidForEntity(i, j));
        if (valid === false) return -1;

        TileEntityLoader.#AddInstance(entity);
        return entity.ID;
    }

    static Kill(i, j) {
        const entity = TileEntityLoader.At(i, j);
        return entity ? TileEntityLoader.KillByID(entity.ID) : false;
    }

    static KillByID(id) {
        const entity = TileEntityLoader.ByID.get(Number(id) | 0);
        if (!entity) return false;

        TileEntityLoader.ByID.delete(entity.ID);
        TileEntityLoader.ByPosition.delete(TileEntityLoader.#PositionKey(entity.Position.X, entity.Position.Y));
        Safe.Run(entity.constructor.name + '.OnKill', () => entity.OnKill());
        return true;
    }

    static Clear() {
        TileEntityLoader.ByID.clear();
        TileEntityLoader.ByPosition.clear();
        TileEntityLoader.#nextID = 0;
    }

    static UpdateAll() {
        const entities = [...TileEntityLoader.ByID.values()];
        for (const entity of entities) {
            const i = entity.Position.X;
            const j = entity.Position.Y;
            const valid = Safe.Run(entity.constructor.name + '.IsTileValidForEntity',
                () => entity.IsTileValidForEntity(i, j));
            if (valid === false) {
                TileEntityLoader.KillByID(entity.ID);
                continue;
            }

            Safe.Run(entity.constructor.name + '.Update', () => entity.Update());
        }
    }

    static Install() {
        Hooks.Once('tileEntity.lifecycle', () => {
            const Main = Terraria.Main;
            const WorldGen = Terraria.WorldGen;
            const WorldFile = Terraria.IO.WorldFile;

            WorldGen['void clearWorld()'].hook((original) => {
                original();
                TileEntityLoader.Clear();
            });

            WorldFile['void LoadWorld(bool canCheckFileState)'].hook((original, check) => {
                original(check);
                TileEntityLoader.#Load();
            });

            WorldFile['void InternalSaveWorld(bool useCloudSaving, bool resetTime)'].hook((original, cloud, reset) => {
                original(cloud, reset);
                if (!cloud) TileEntityLoader.#Save();
            });

            Main['void DoUpdateInWorld()'].hook((original, self) => {
                original(self);
                TileEntityLoader.UpdateAll();
            });

            WorldGen['void SaveAndQuitCallBack(object threadContext)'].hook((original, context) => {
                original(context);
                TileEntityLoader.Clear();
            });
        });
    }

    static #Template(which) {
        if (typeof which === 'number') return TileEntityLoader.ByType.get(which);
        if (typeof which === 'function') return Templates.Get(which);

        if (which && typeof which === 'object') {
            if (which instanceof ModTileEntity) return TileEntityLoader.ByType.get(which.Type);
            if (typeof which.Type === 'number') return TileEntityLoader.ByType.get(which.Type);
        }
        return undefined;
    }

    static #Create(template, id, i, j) {
        const entity = new template.constructor();
        entity.Mod = template.Mod;
        entity.Type = template.Type;
        entity.ID = id;
        entity.Position = Point16.new(i, j);
        return entity;
    }

    static #AddInstance(entity) {
        const pos = TileEntityLoader.#PositionKey(entity.Position.X, entity.Position.Y);
        const old = TileEntityLoader.ByPosition.get(pos);
        if (old) TileEntityLoader.ByID.delete(old.ID);

        TileEntityLoader.ByID.set(entity.ID, entity);
        TileEntityLoader.ByPosition.set(pos, entity);
        TileEntityLoader.#nextID = Math.max(TileEntityLoader.#nextID, entity.ID + 1);
    }

    static #PositionKey(i, j) {
        return (Number(i) | 0) + ':' + (Number(j) | 0);
    }

    static #WorldFile() {
        const path = Terraria.Main.worldPathName;
        return path ? path + '.bl.json' : null;
    }

    static #Read(file) {
        const text = bl.file.read(file);
        if (!text) return {};

        try {
            return JSON.parse(text) || {};
        } catch (e) {
            bl.log('ModTileEntity: ' + file + ' esta quebrado (' + e + '); os dados foram ignorados');
            return {};
        }
    }

    static #Load() {
        TileEntityLoader.Clear();

        const file = TileEntityLoader.#WorldFile();
        if (!file) return;

        const all = TileEntityLoader.#Read(file);
        const saved = all[TileEntityLoader.#saveKey];
        const entries = saved && Array.isArray(saved.entries) ? saved.entries : [];

        for (const row of entries) {
            const template = row && TileEntityLoader.#byKey.get(row.key);
            if (!template) continue;

            const id = Number(row.id) | 0;
            const i = Number(row.x) | 0;
            const j = Number(row.y) | 0;
            const entity = TileEntityLoader.#Create(template, id, i, j);
            Safe.Run(entity.constructor.name + '.LoadData',
                () => entity.LoadData(TagCompound.from(row.data)));
            TileEntityLoader.#AddInstance(entity);
        }

        if (saved && Number.isFinite(saved.nextID)) {
            TileEntityLoader.#nextID = Math.max(TileEntityLoader.#nextID, Number(saved.nextID) | 0);
        }
    }

    static #Save() {
        const file = TileEntityLoader.#WorldFile();
        if (!file) return;

        const all = TileEntityLoader.#Read(file);
        const old = all[TileEntityLoader.#saveKey];
        const oldEntries = old && Array.isArray(old.entries) ? old.entries : [];
        const entries = oldEntries.filter((row) => row && !TileEntityLoader.#byKey.has(row.key));

        for (const entity of TileEntityLoader.ByID.values()) {
            const key = TileEntityLoader.#keyByType.get(entity.Type);
            if (!key) continue;

            const data = new TagCompound();
            Safe.Run(entity.constructor.name + '.SaveData', () => entity.SaveData(data));
            entries.push({
                key,
                id: entity.ID,
                x: entity.Position.X,
                y: entity.Position.Y,
                data,
            });
        }

        if (entries.length) {
            all[TileEntityLoader.#saveKey] = {
                version: 1,
                nextID: TileEntityLoader.#nextID,
                entries,
            };
        } else {
            delete all[TileEntityLoader.#saveKey];
        }

        Safe.Run('ModTileEntity: gravar ' + file, () => {
            if (Object.keys(all).length) bl.file.write(file, JSON.stringify(all));
            else bl.file.delete(file);
        });
    }
}
