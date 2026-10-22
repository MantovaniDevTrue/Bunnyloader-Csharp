const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ready = [], hooks = new Map(), calls = [];
let accept = false, nativeThrows = false, originalDraws = 0;
class NativeArray extends Array {
    cloneResized(total) {
        const copy = new NativeArray(total).fill(0);
        this.forEach((value, index) => { copy[index] = value; });
        return copy;
    }
}
const seed = value => new NativeArray(15).fill(value);
const hookable = name => new Proxy({}, { get: (_target, signature) => ({
    hook: callback => hooks.set(`${name}:${signature}`, callback)
}) });
const Main = hookable('Main');
// Keep normal field access while recording actual loader hook installation.
const mainValues = { liquidAlpha: seed(0), activeLiquidAlpha: seed(0), waterStyle: 0,
    gameMenu: false, SceneMetrics: { ActiveFountainColor: -1 }, bgStyle: 0,
    bloodMoon: false, dayTime: true, LocalPlayer: {}, rain: [] };
const main = new Proxy(mainValues, { get: (target, name) => name in target ? target[name] : Main[name] });
const renderer = { _liquidTextures: seed({ name: 'vanilla-water' }) };
const textures = { Liquid: seed({ name: 'vanilla-block' }), LiquidSlope: seed({ name: 'vanilla-slope' }) };
const context = {
    Terraria: {
        Main: main, WaterfallManager: hookable('WaterfallManager'), Dust: hookable('Dust'),
        Gore: hookable('Gore'), Rain: hookable('Rain'), ID: { GoreID: { Count: 1400 } },
        GameContent: { TextureAssets: textures, Liquid: { LiquidRenderer: renderer }, Drawing: { TileDrawing: hookable('TileDrawing') } },
        Graphics: { Light: { LightingEngine: hookable('LightingEngine'), LegacyLighting: hookable('LegacyLighting') } }
    },
    Ready: { Add: callback => ready.push(callback) },
    Safe: { Run: (_name, callback) => { try { return callback(); } catch { return undefined; } } },
    Hooks: { Overrides: () => false }, ModWaterStyle: class ModWaterStyle {},
    Ref: class Ref { constructor(value) { this.value = value; } },
    Microsoft: { Xna: { Framework: { Vector3: {} } } },
    ModFiles: { Texture: name => name + '.png' }, SceneEffectPriority: { BiomeLow: 1 },
    SceneEffectLoader: { Of: () => ({}) },
    bl: { mod: { path: '/example/content' }, file: { exists: () => true },
        loadTextureAsset: path => ({ path }), log: () => {},
        tiles: { setWaterStyleCount: total => {
            // The bridge must see all five expanded arrays before either loop
            // can be enlarged. These calls execute the production JS loader.
            for (const table of [renderer._liquidTextures, textures.Liquid, textures.LiquidSlope, main.liquidAlpha, main.activeLiquidAlpha])
                assert.ok(table.length >= total);
            calls.push(total);
            if (nativeThrows) throw new Error('native contract unavailable');
            return accept;
        } }
    }
};
vm.runInNewContext(fs.readFileSync('app/src/main/cpp/script/js/mod/Loaders/WaterStyleLoader.js', 'utf8') +
    '\nthis.Loader = WaterStyleLoader;', context);
class ExampleWaterStyle {}
const example = new ExampleWaterStyle();
example.Texture = 'Content/Biomes/ExampleWaterStyle';
context.Loader.Add(example);
for (const callback of ready) callback();
const draw = hooks.get('Main:void DrawWaters(bool isBackground)');
assert.equal(typeof draw, 'function');
const original = (self, background) => { assert.equal(self, main); assert.equal(typeof background, 'boolean'); ++originalDraws; return 73; };
assert.equal(draw(original, main, true), undefined);
assert.equal(originalDraws, 0); assert.deepEqual(calls, [16]);
for (let i = 0; i < 60; ++i) draw(original, main, false);
assert.equal(calls.length, 1); assert.equal(originalDraws, 0);
accept = true;
assert.equal(draw(original, main, false), 73);
assert.deepEqual(calls, [16, 16]); assert.equal(originalDraws, 1);
for (let i = 0; i < 20; ++i) assert.equal(draw(original, main, i % 2 === 0), 73);
assert.equal(calls.length, 2); assert.equal(originalDraws, 21);
// A recreated local-user table is grown before the already-validated original
// runs, retaining the ordinary no-patch path on all following frames.
main.liquidAlpha = seed(0); main.activeLiquidAlpha = seed(0);
assert.equal(draw(original, main, true), 73);
assert.equal(main.liquidAlpha.length, 16); assert.equal(main.activeLiquidAlpha.length, 16);
assert.equal(calls.length, 2);
// A thrown bridge call must not mark this separate fresh loader installed.
nativeThrows = true; accept = false;
const secondContext = { ...context, Ready: { Add: callback => ready.push(callback) } };
vm.runInNewContext(fs.readFileSync('app/src/main/cpp/script/js/mod/Loaders/WaterStyleLoader.js', 'utf8') +
    '\nthis.Loader = WaterStyleLoader;', secondContext);
const secondExample = new ExampleWaterStyle(); secondExample.Texture = example.Texture;
secondContext.Loader.Add(secondExample); ready.at(-1)();
const retryDraw = hooks.get('Main:void DrawWaters(bool isBackground)');
const beforeDraws = originalDraws, beforeCalls = calls.length;
assert.equal(retryDraw(original, main, false), undefined);
for (let i = 0; i < 60; ++i) retryDraw(original, main, false);
assert.equal(originalDraws, beforeDraws); assert.equal(calls.length, beforeCalls + 1);
nativeThrows = false; accept = true;
assert.equal(retryDraw(original, main, false), 73);
assert.equal(originalDraws, beforeDraws + 1);
console.log('JS_WATER_REAL_LOADER_RETRY_AND_NATIVE_TABLE_ORDER_PASS');
