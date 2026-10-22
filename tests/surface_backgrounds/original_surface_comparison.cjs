'use strict';
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const {createHash} = require('node:crypto');

const native = process.argv[2];
assert(native, 'Pass the compiled production native contract runner');
const root = path.join(__dirname, 'reference');
const expected = {
  'SurfaceBackgroundLoader.js': '4538e3d819e0d0c6a0b5be387fd0d2b10b306e8d44c09c9ef71b1a56c620c2a0',
  'ModSurfaceBackgroundStyle.js': 'f401aa4c5bd6a01fb19f5e0bad7f108b53610f19c6d20b1cf4c8ca8ac3af3e50',
  'ExampleSurfaceBackgroundStyle.js': '271dba01fd03f2938ee75017e5598328ef4584eaa01e0e4cdbdcdf83cde6a237',
};
const source = name => {
  const text = fs.readFileSync(path.join(root, name), 'utf8');
  if (expected[name]) assert.equal(createHash('sha256').update(text).digest('hex'), expected[name]);
  return text;
};
const hooks = new Map(), draws = [], depths = [], lookup = [], failures = [];
let menuSlot = -1;
const scene = {value: 16, priority: 3};
const resize = values => Object.assign(values, {
  cloneResized(length) {return resize(Array.from({length}, (_, i) => this[i] ?? 0));},
});
const properties = {
  gameMenu: false, bgStyle: 16, LocalPlayer: {}, instance: {bgLoops: 3, bgStartX: -100, bgTopY: 12, scAdj: 0},
  bgAlphaFrontLayer: resize(Array(17).fill(0)), bgAlphaFarBackLayer: resize(Array(17).fill(0)),
  backgroundWidth: [0, 128, 128, 128, 128, 128, 128], backgroundHeight: [0, 96, 96, 96, 96, 96, 96],
  bgWidthScaled: 256, bgScale: 2, spriteBatch: {}, screenPosition: {X: 160, Y: 200}, screenWidth: 2400, worldSurface: 100,
  ColorOfSurfaceBackgroundsBase: {r: 20, g: 40, b: 60, a: 80},
};
properties.bgAlphaFrontLayer[16] = .5;
properties.bgAlphaFarBackLayer[16] = .5;
const main = new Proxy(properties, {get(target, key) {
  if (key in target) return target[key];
  if (typeof key === 'string' && key.includes('(')) return {hook(fn) {hooks.set(key, fn);}};
  return undefined;
}});
const drawSignature = 'void Draw(Texture2D texture, Vector2 position, Nullable`1 sourceRectangle, Color color, float rotation, Vector2 origin, float scale, SpriteEffects effects, float layerDepth)';
const sandbox = vm.createContext({
  Terraria: {
    Main: main,
    ID: {SurfaceBackgroundID: {Count: 16, Sets: {IsForest: resize(Array(16).fill(false)), IsDesertVariant: resize(Array(16).fill(false))}}},
    GameContent: {TextureAssets: {Background: Array.from({length: 7}, (_, slot) => ({Value: {slot}}))}},
    Graphics: {Effects: {SkyManager: {Instance: {'void DrawToDepth(SpriteBatch spriteBatch, float minDepth)'(batch, depth) {
      assert.equal(batch, properties.spriteBatch);depths.push(depth);
    }}}}},
  },
  Microsoft: {Xna: {Framework: {Graphics: {SpriteBatch: {[drawSignature](batch, texture, pos, rect, color, rotation, origin, scale, effects, depth) {
    assert.equal(batch, properties.spriteBatch);assert.equal(rotation, 0);assert.equal(effects, 0);assert.equal(depth, 0);
    assert.equal(origin.X, 0);assert.equal(origin.Y, 0);
    draws.push({slot: texture.slot, x: pos.X, y: pos.Y, width: rect.Width, height: rect.Height, scale, color});
  }}}}}},
  Ref: class Ref {}, Vector2: {new: (X, Y) => ({X, Y})}, Rectangle: {new: (X, Y, Width, Height) => ({X, Y, Width, Height})},
  Color: {Multiply: (c, alpha) => ({r: c.r * alpha, g: c.g * alpha, b: c.b * alpha, a: c.a * alpha})},
  bl: {hookFlags: {set() {}}}, Hooks: {Once(key, install) {install();}},
  Safe: {Run(label, action) {try {return action();} catch(error) {failures.push({label, error});return null;}}},
  SceneEffectPriority: {BiomeLow: 1, BiomeMedium: 2, BiomeHigh: 3}, SceneEffectLoader: {Of: () => ({surfaceBackground: scene})},
  MenuLoader: {BackgroundSlot: () => menuSlot}, Templates: {Adopt(cls, instance) {instance.Mod = {Name: 'ExampleMod'};}},
  BackgroundTextureLoader: {GetBackgroundSlot(...args) {
    lookup.push(args);const name = args.at(-1);
    if (name.endsWith('Far')) return 1;
    if (name.endsWith('Close')) return 2;
    const match = /Mid([0-3])$/.exec(name);assert(match, name);return 3 + Number(match[1]);
  }},
});
vm.runInContext(source('SurfaceBackgroundLoader.js') + '\n' + source('ModSurfaceBackgroundStyle.js') + '\n' +
  source('ExampleSurfaceBackgroundStyle.js').replace('export class ', 'class '), sandbox);
const style = vm.runInContext('ModSurfaceBackgroundStyle.register(ExampleSurfaceBackgroundStyle)', sandbox);
assert.equal(style.Slot, 16);
const hook = name => {
  const entries = [...hooks].filter(([signature]) => signature.includes(name + '('));
  assert.equal(entries.length, 1, name);return entries[0][1];
};
const choose = hook('GetPreferredBGStyleForPlayer'), step1 = hook('DrawSurfaceBG_BackMountainsStep1');
const step2 = hook('DrawSurfaceBG_BackMountainsStep2'), fog = hook('DrawSurfaceBG_GetFogPower');
const run = (...args) => execFileSync(native, args.map(String), {encoding: 'utf8'}).trim().split(/\s+/).map(Number);
const near = (actual, expected) => assert(Math.abs(actual - expected) <= Math.max(1e-4, Math.abs(expected) * 1e-6), `${actual} != ${expected}`);
let projections = 0;
for (const x of [-50000, -1280, -256, -1, 0, 160, 50000]) {
  for (const scale of [.5, 1.25, 2]) {
    for (const below of [false, true]) {
      const parallax = .5, a = 1851, b = 1749, top = .1234, adj = -11.375, push = 7;
      properties.screenPosition = {X: x, Y: below ? 1616 : 200};properties.instance.scAdj = adj;
      const originalChoose = style.ChooseCloseTexture;
      style.ChooseCloseTexture = function(s, p, ar, br) {
        s.value = scale;p.value = parallax;ar.value = a;br.value = b;
        return originalChoose.call(this, s, p, ar, br);
      };
      step1(() => {}, properties.instance, top, 1, push);draws.length = 0;depths.length = 0;
      let originals = 0;assert.equal(fog(() => {++originals;return .75;}), .75);assert.equal(originals, 1);
      const plan = run('--close', x, properties.screenPosition.Y, properties.worldSurface, properties.screenWidth, top, adj, push, 128, 96, scale, parallax, a, b);
      assert.equal(plan[0], draws.length);assert.equal(plan[6], 1);assert.equal(depths.length, 1);near(plan[5], depths[0]);
      draws.forEach((draw, index) => {
        near(plan[1] + plan[3] * index, draw.x);near(plan[2], draw.y);near(plan[4], draw.scale);
        assert.equal(draw.slot, 2);assert.equal(draw.width, 128);assert.equal(draw.height, 96);
      });
      // GetFogPower consumes Step1 once; a second call cannot redraw Close.
      const count = draws.length;fog(() => .75);assert.equal(draws.length, count);
      style.ChooseCloseTexture = originalChoose;++projections;
    }
  }
}
for (const loops of [0, 1, 9]) {
  properties.instance.bgLoops = loops;properties.instance.bgStartX = -311;properties.instance.bgTopY = 17;
  properties.bgWidthScaled = 300;properties.bgScale = 1.75;draws.length = 0;
  let originals = 0;step1(() => ++originals, properties.instance, .2, 1, 0);assert.equal(originals, 1);
  const plan = run('--back', loops, -311, 17, 300, 1.75, 128, 96);assert.equal(draws.length, plan[0]);
  draws.forEach((draw, index) => {near(draw.x, plan[1] + index * plan[3]);near(draw.y, plan[2]);near(draw.scale, plan[4]);assert.equal(draw.slot, 1);});++projections;
}
for (let priority = 0; priority <= 8; ++priority) for (let vanilla = 0; vanilla < 16; ++vanilla) {
  scene.priority = priority;const expectedSlot = choose(() => vanilla);
  assert.equal(run('--choose', priority, vanilla)[0], expectedSlot === style.Slot ? 1 : 0);
}
properties.gameMenu = true;scene.priority = 8;assert.equal(choose(() => 7), 7);properties.gameMenu = false;
for (const speed of [0, .03, .25, 1, -.1]) {
  const alphas = [1, .2, 0, .9];style.Slot = 2;style.ModifyFarFades(alphas, speed);
  const converted = run('--fades', 2, speed, 1, .2, 0, .9);alphas.forEach((alpha, i) => near(converted[i], alpha));
}
style.Slot = 16;
// A Double speed just below a Single midpoint must remain Double until the
// final array write. Converting speed to float first rounds the wrong way.
const initial = Math.fround(.5 + 2 ** -24), exactSpeed = 2 ** -25 - 2 ** -52;
style.Slot = 0;const precise = [initial];style.ModifyFarFades(precise, exactSpeed);
assert.equal(Math.fround(run('--fades', 0, exactSpeed, initial)[0]), Math.fround(precise[0]));
style.Slot = 16;
// Original example's animation is call-driven: 13 ChooseMiddle calls, not a
// timer in Update(). Keep this oracle for the forthcoming managed callback.
properties.instance.bgLoops = 1;lookup.length = 0;
for (let call = 1; call <= 53; ++call) {
  draws.length = 0;step2(() => {}, properties.instance, 0);
  assert.equal(draws.length, 1);assert.equal(draws[0].slot, 3 + Math.floor(call / 13) % 4);
}
assert(lookup.some(args => args.length === 1 && args[0] === 'ExampleMod/Assets/Textures/Backgrounds/ExampleBiomeSurfaceMid3'));
assert.equal(failures.length, 0, JSON.stringify(failures));
console.log(`SURFACE_ORIGINAL_JS_NATIVE_GEOMETRY_PASS projections=${projections} priorities=144 fades=5 doublePrecision=1 animationCalls=53`);
