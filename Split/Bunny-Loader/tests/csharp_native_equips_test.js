const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const source = fs.readFileSync(process.argv[2], 'utf8');
const jobs = [];
const assigned = new Map();
const pending = [
    { type: 6197, kind: 1, texture: 'uuid:Content/Items/Accessories/Shield_Shield.png' },
    { type: 6198, kind: 3, texture: 'uuid:Content/Items/Accessories/Boots_Shoes.png' }
];
const requests = [];
const growCalls = [];
const assets = () => {
    const value = [];
    value.cloneResized = n => {
        const next = assets();
        for (let i = 0; i < Math.min(n, value.length); ++i) next[i] = value[i];
        next.length = n;
        return next;
    };
    return value;
};
const shieldTextures = assets();
const shoeTextures = assets();
shieldTextures.length = 15;
shoeTextures.length = 25;

const global = {
    EquipTexture: class {},
    EquipType: { Body: 'Body', Legs: 'Legs', Wings: 'Wings' },
    Terraria: {
        ID: { ArmorIDs: { Shield: { Count: 15 }, Shoe: { Count: 25 } } },
        GameContent: { TextureAssets: { AccShield: shieldTextures, AccShoes: shoeTextures } }
    },
    ContentLookup: { RequestFile(path) { requests.push(path); return { path }; } },
    bl: {
        csharp: {
            nativeEquipTextures() {
                return pending.filter(e => !assigned.has(e.type));
            },
            visualTexturePath(value) { return '/test/' + value.split(':')[1]; },
            setNativeEquipSlot(type, kind, slot) {
                assigned.set(type, { kind, slot });
                return true;
            }
        },
        file: { exists() { return true; } },
        items: { growEquipSets(kind, first, last) { growCalls.push({ kind, first, last }); } },
        log() {}
    },
    Ready: { Add(fn, phase) { jobs.push({ fn, phase }); } },
    Safe: { Run(label, fn) { fn(); } }
};
vm.runInNewContext(source + '\nglobalThis.Equip = EquipLoader;', global);
const Equip = global.Equip;
assert.equal(jobs.length, 1, 'automatic content-ready equip registration');
assert.equal(jobs[0].phase, 'content');
jobs[0].fn();
assert.equal(requests.length, 0, 'registration stays pending until equip install');
Equip.Install();
assert.equal(assigned.get(6197).kind, 1);
assert.equal(assigned.get(6197).slot, 15);
assert.equal(assigned.get(6198).kind, 3);
assert.equal(assigned.get(6198).slot, 25);
assert.equal(global.Terraria.ID.ArmorIDs.Shield.Count, 16);
assert.equal(global.Terraria.ID.ArmorIDs.Shoe.Count, 26);
assert.equal(global.Terraria.GameContent.TextureAssets.AccShield[15].path,
    '/test/Content/Items/Accessories/Shield_Shield.png');
assert.equal(global.Terraria.GameContent.TextureAssets.AccShoes[25].path,
    '/test/Content/Items/Accessories/Boots_Shoes.png');
assert.equal(Equip.VanillaCount('Shield'), 15);
Equip.SyncCSharp();
assert.equal(requests.length, 2, 'no repeated texture registrations');
pending.push({ type: 6203, kind: 1, texture: 'uuid:Content/Items/Accessories/Later_Shield.png' });
Equip.SyncCSharp();
assert.equal(assigned.get(6203).slot, 16);
assert.equal(Equip.VanillaCount('Shield'), 15, 'real vanilla count preserved on late installs');
assert.equal(global.Terraria.ID.ArmorIDs.Shield.Count, 17);
assert.equal(requests.length, 3);
assert.equal(Equip.InstalledKinds().length, 2);
console.log('CSHARP_NATIVE_EQUIP_LOADER_PASS=YES');
