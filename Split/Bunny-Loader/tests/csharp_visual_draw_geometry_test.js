const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const file = process.argv[2];
const raw = fs.readFileSync(file, 'utf8');
const index = raw.indexOf('class BunnyCSharpVisualLayer extends PlayerDrawLayer {');
assert.ok(index >= 0);
const code = raw.slice(index) + '\nglobalThis.Visual = BunnyCSharpVisualLayer;';
const draws = [];
const loaded = [];
let effects = [];
const player = {
    whoAmI: 0, dead: false, invis: false,
    width: 20, height: 42, direction: 1,
    position: { X: 1010, Y: 1020 },
    bodyFrame: { Y: 112, Height: 56 },
    legFrame: { Y: 56, Height: 56 },
    velocity: { X: 3, Y: 0 }
};
const screenPosition = { X: 1000, Y: 1000 };
const ctor = 'void .ctor(Texture2D texture, Vector2 position, Rectangle sourceRect, Color color, float rotation, Vector2 origin, float scale, SpriteEffects effect, int inactiveLayerDepth)';
const sandbox = {
    PlayerDrawLayer: class {
        constructor(name) { this.Name = name; }
        static AfterParent(value) { return { After: value }; }
    },
    PlayerDrawLayers: { IceBarrier: {} },
    PlayerDrawHooks: { AddLayer() {} },
    ModPlayer: { AddDrawData(info, data) { draws.push(data.draw); } },
    Terraria: {
        Main: { screenPosition },
        GameContent: { TextureAssets: { MagicPixel: { Value: { Width: 1, Height: 1, pixel: true } } } },
        DataStructures: { DrawData: { new() {
            return {
                [ctor](texture, position, source, color, rotation, origin, scale) {
                    this.draw = { texture, position, source, color, origin, scale };
                }
            };
        } } }
    },
    bl: {
        csharp: {
            visualSnapshot() { return effects; },
            visualActiveCount() { return effects.length; },
            visualTexturePath(name) { return name; }
        },
        loadTexture(name) { loaded.push(name); return { Width: 40, Height: 1120 }; }
    },
    Color: { new(R, G, B, A) { return { R, G, B, A }; } },
    Vector2: { new(X, Y) { return { X, Y }; } },
    Rectangle: { new(X, Y, Width, Height) { return { X, Y, Width, Height }; } },
    Safe: { Once() {} },
    Math
};
vm.runInNewContext(code, sandbox, { filename: file });
const Visual = sandbox.Visual;
const layer = new Visual();
const info = { drawPlayer: player, Position: player.position, shadow: 0, playerEffect: 0 };
const rgba = 0xffffffff;
effects = [{ kind: 4, style: 1, color: rgba, opacity: 1, scale: 1.15, texture: 'shield.png' }];
assert.equal(layer.GetDefaultVisibility(info), true);
layer.Draw(info);
assert.equal(draws.length, 1, 'one equipped shield frame');
assert.equal(draws[0].source.Width, 40);
assert.equal(draws[0].source.Height, 40, 'no full tall sprite sheet');
assert.equal(draws[0].source.Y, 80, 'body frame matches 40-pixel sheet');
assert.ok(Math.abs(draws[0].position.X - 24) <= 4);
assert.ok(Math.abs(draws[0].position.Y - 39) <= 4);
draws.length = 0;
const loadCount = loaded.length;
effects = [{ kind: 2, style: 2, color: 0xff66ccff, opacity: 0.8, scale: 1.1, texture: 'shield.png' }];
layer.Draw(info);
assert.equal(draws.length, 14, 'only procedural shield ring');
assert.equal(loaded.length, loadCount, 'barrier must not load shield sprite sheet');
for (const draw of draws) {
    assert.equal(draw.source.Width, 1);
    assert.equal(draw.source.Height, 1);
    assert.ok(draw.position.Y > -30 && draw.position.Y < 100, 'ring near player');
}
draws.length = 0;
effects = [{ kind: 4, style: 3, color: 0xffff9632, opacity: 0.85, scale: 1, texture: 'boots.png' }];
for (let i = 0; i < 8; i++) {
    player.position = { X: 1010 + i * 7, Y: 1020 };
    info.Position = player.position;
    layer.Draw(info);
}
assert.equal(loaded.length, loadCount, 'running trail must not load boot sprite sheet');
assert.ok(draws.length > 0 && draws.length <= 80, 'bounded pixel-only trail');
for (const draw of draws) {
    assert.equal(draw.source.Width, 1);
    assert.equal(draw.source.Height, 1);
    assert.ok(draw.position.Y > 15 && draw.position.Y < 75, 'streaks stay at feet');
    assert.ok(draw.scale <= 6, 'small particles only');
}
player.velocity.X = 0;
layer.Draw(info);
assert.equal(Visual.Trails.size, 0, 'clear when motion stops');
effects = [];
assert.equal(layer.GetDefaultVisibility(info), false);
console.log('CSHARP_VISUAL_GEOMETRY_PASS=YES');
