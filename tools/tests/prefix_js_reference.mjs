import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const samples = path.resolve(here, '../../samples/ExampleMod/content/Content/Prefixes');
const filenames = ['ExamplePrefix.js', 'ExampleDerivedPrefix.js', 'ExampleAccessoryPrefix.js'];

const code = filenames.map(name => fs.readFileSync(path.join(samples, name), 'utf8')
    .replace(/^import\s+\{[^\n]+\n/gm, '')
    .replace(/\bexport\s+class\s+/g, 'class ')).join('\n');

const ModPrefix = class {};
const PrefixCategory = Object.freeze({
    Melee: 0, Ranged: 1, Magic: 2, Summon: 3,
    AnyWeapon: 4, Accessory: 5, Custom: 6
});
const create = new Function('ModPrefix', 'PrefixCategory',
    code + '\nreturn { ExamplePrefix, ExampleDerivedPrefix, ExampleAccessoryPrefix };');
const { ExamplePrefix, ExampleDerivedPrefix, ExampleAccessoryPrefix } = create(ModPrefix, PrefixCategory);

function assertWeapon(prefix, expectedDamage, expectedValue) {
    assert.equal(prefix.Category, PrefixCategory.AnyWeapon);
    assert.equal(prefix.RollChance({}), 5);
    assert.equal(prefix.CanRoll({}), true);
    const damage = { value: 1 };
    const knockback = { value: 1 };
    const useTime = { value: 1 };
    const scale = { value: 1 };
    const shootSpeed = { value: 1 };
    const mana = { value: 1 };
    const crit = { value: 0 };
    prefix.SetStats(damage, knockback, useTime, scale, shootSpeed, mana, crit);
    const value = { value: 1 };
    prefix.ModifyValue(value);
    assert.ok(Math.abs(damage.value - expectedDamage) < 1e-9);
    assert.ok(Math.abs(value.value - expectedValue) < 1e-9);
    assert.equal(shootSpeed.value, 1);
    assert.equal(mana.value, 1);
    assert.equal(useTime.value, 1);
}

assertWeapon(new ExamplePrefix(), 1.2, 1.05);
assertWeapon(new ExampleDerivedPrefix(), 1.4, 1.1);
const accessory = new ExampleAccessoryPrefix();
assert.equal(accessory.Category, PrefixCategory.Accessory);
const player = { statDefense: 10 };
accessory.ApplyAccessoryEffects(player);
assert.equal(player.statDefense, 14);
const value = { value: 1 };
accessory.ModifyValue(value);
assert.ok(Math.abs(value.value - 1.2) < 1e-9);
console.log('BUNNY_JS_PREFIX_REFERENCE=PASS cases=3');
