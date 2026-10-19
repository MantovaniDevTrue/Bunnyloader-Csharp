const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname,
    '../../app/src/main/cpp/script/js/mod/Loaders/RarityLoader.js'), 'utf8');

let nextId = 12;
const reserved = [];
const context = {
    Ready: { Add() {} },
    Hooks: { Once() {} },
    TooltipLoader: { Install() {} },
    bl: { csharp: { rarityCount() { return nextId; }, reserveRarityId() {
        const id = nextId++;
        reserved.push(id);
        return id;
    } } },
};
context.globalThis = context;
vm.runInNewContext(source + '\nglobalThis.sharedRarityLoader = RarityLoader;', context);
const loader = context.sharedRarityLoader;
const first = { SetStaticDefaults() {} };
const idFirst = loader.Add(first);
assert.equal(idFirst, 12);
assert.equal(first.Type, 12);
assert.equal(loader.GetRarity(12), first);

const reservedByCSharp = context.bl.csharp.reserveRarityId();
assert.equal(reservedByCSharp, 13);
assert.equal(loader.RarityCount, 14);
const second = { SetStaticDefaults() {} };
const idSecond = loader.Add(second);
assert.equal(idSecond, 14);
assert.equal(second.Type, 14);
assert.equal(loader.RarityCount, 15);
assert.equal(loader.GetRarity(13), null);

const duplicate = { SetStaticDefaults() {} };
context.bl.csharp.reserveRarityId = () => 14;
assert.throws(() => loader.Add(duplicate), /invalid shared rarity ID/);
assert.equal(loader.GetRarity(14), second);

context.bl.csharp.reserveRarityId = () => 4108;
assert.throws(() => loader.Add({}), /invalid shared rarity ID/);

const old = { Ready: { Add() {} }, Hooks: { Once() {} }, TooltipLoader: { Install() {} } };
old.globalThis = old;
vm.runInNewContext(source + '\nglobalThis.sharedRarityLoader = RarityLoader;', old);
const legacy = old.sharedRarityLoader;
assert.equal(legacy.Add({}), 12);
assert.equal(legacy.Add({}), 13);
assert.equal(legacy.RarityCount, 14);

console.log('RARITY_SHARED_ID_JS=PASS');
