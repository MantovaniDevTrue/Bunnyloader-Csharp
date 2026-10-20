const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname,
    '../../app/src/main/cpp/script/js/mod/Loaders/TooltipLoader.js'), 'utf8');
const marker = '    static #NameColor(rare, diff, expert) {';
assert(source.includes(marker));

const colors = new Map([[15, { R: 90, G: 130, B: 210 }]]);
const calls = [];
const context = {
    Color: {
        new(R, G, B) { return { R, G, B }; },
        White: { R: 255, G: 255, B: 255 },
    },
    RarityLoader: {
        ByType: colors,
        ColorOf(id) { return colors.get(id); },
    },
    Terraria: { Main: {
        mcColor: { R: 3, G: 4, B: 5 },
        hcColor: { R: 6, G: 7, B: 8 },
        DiscoR: 40, DiscoG: 80, DiscoB: 120, masterColor: 1,
    } },
    bl: { csharp: { rarityColor(id) {
        calls.push(id);
        return id === 19 ? [80, 90, 170] : null;
    } } },
};
context.globalThis = context;
const instrumented = source.replace(marker,
    '    static TestNameColor(rare, diff, expert) { return TooltipLoader.#NameColor(rare, diff, expert); }\n' + marker);
vm.runInNewContext(instrumented + '\nglobalThis.realTooltip = TooltipLoader;', context);
const color = context.realTooltip.TestNameColor;

assert.deepEqual({ ...color(19, 0, false) }, { R: 80, G: 90, B: 170 });
assert.deepEqual({ ...color(15, 0, false) }, { R: 90, G: 130, B: 210 });
assert.deepEqual({ ...color(19, 1, false) }, { R: 3, G: 4, B: 5 });
assert.deepEqual({ ...color(19, 2, false) }, { R: 6, G: 7, B: 8 });
assert.deepEqual({ ...color(19, 0, true) }, { R: 40, G: 80, B: 120 });
assert.deepEqual({ ...color(21, 0, false) }, { R: 180, G: 40, B: 255 });
assert.deepEqual({ ...color(11, 0, false) }, { R: 180, G: 40, B: 255 });
assert.deepEqual({ ...color(0, 0, false) }, { R: 255, G: 255, B: 255 });
assert.deepEqual(calls, [19, 21]);

context.bl.csharp.rarityColor = () => null;
assert.deepEqual({ ...color(19, 0, false) }, { R: 180, G: 40, B: 255 });
delete context.bl.csharp.rarityColor;
assert.deepEqual({ ...color(19, 0, false) }, { R: 180, G: 40, B: 255 });

console.log('RARITY_JS_TOOLTIP_INTEROP=PASS');
