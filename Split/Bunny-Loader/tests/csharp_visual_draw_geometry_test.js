const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const raw = fs.readFileSync(process.argv[2], 'utf8');
const offset = raw.indexOf('class BunnyCSharpVisualRender {');
assert.ok(offset > 0);
const registered = [];
const draws = [];
const loaded = [];
let effects = [];
const player = {
    whoAmI: 0, width: 20, height: 42, direction: 1,
    dead: false, invis: false, bodyRotation: 0,
    position: { X: 1010, Y: 1020 },
    bodyFrame: { Y: 112, Height: 56 },
    velocity: { X: 3.2, Y: 0 },
    mount: { Active: false }
};
const ctor = 'void .ctor(Texture2D texture, Vector2 position, Rectangle sourceRect, Color color, float rotation, Vector2 origin, float scale, SpriteEffects effect, int inactiveLayerDepth)';
const sandbox = {
    PlayerDrawLayer: class {
        constructor(name) { this.Name = name; }
        static AfterParent(value) { return { After: value }; }
        static BeforeParent(value) { return { Before: value }; }
    },
    PlayerDrawLayers: { IceBarrier: { name: 'IceBarrier' }, Shield: { name: 'Shield' }, BackAcc: { name: 'BackAcc' } },
    PlayerDrawHooks: { AddLayer(layer) { registered.push(layer); } },
    EquipLoader: { SyncCSharp() {} },
    ModPlayer: { AddDrawData(info, data) { draws.push(data.draw); return true; } },
    Terraria: {
        Main: { screenPosition: { X: 1000, Y: 1000 } },
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
        loadTexture(name) {
            loaded.push(name);
            const dims = name === 'shield.png' ? [40, 1120] :
                name === 'ward.png' ? [52, 72] : [12, 14];
            return { Width: dims[0], Height: dims[1], name };
        }
    },
    Color: { new(R, G, B, A) { return { R, G, B, A }; } },
    Vector2: { new(X, Y) { return { X, Y }; } },
    Rectangle: { new(X, Y, Width, Height) { return { X, Y, Width, Height }; } },
    Safe: { Once() {} },
    Math
};
vm.runInNewContext(raw.slice(offset) + '\nglobalThis.Visual = BunnyCSharpVisualRender;', sandbox);
const { Visual } = sandbox;
assert.equal(registered.length, 3);
const trailLayer = registered[0], shieldLayer = registered[1], barrierLayer = registered[2];
assert.equal(trailLayer.GetDefaultPosition().Before, sandbox.PlayerDrawLayers.BackAcc);
assert.equal(shieldLayer.GetDefaultPosition().After, sandbox.PlayerDrawLayers.Shield);
assert.equal(barrierLayer.GetDefaultPosition().After, sandbox.PlayerDrawLayers.IceBarrier);
const info = { drawPlayer: player, Position: player.position, shadow: 0, playerEffect: 0 };
effects = [{ kind: 4, style: 1, color: 0xffffffff, opacity: 1, scale: 1.15, texture: 'shield.png' }];
assert.equal(shieldLayer.GetDefaultVisibility(info), true);
shieldLayer.Draw(info);
assert.equal(draws.length, 1);
assert.equal(draws[0].source.Width, 40);
assert.equal(draws[0].source.Height, 40);
assert.equal(draws[0].source.Y, 240, 'stable full shield frame 6');
const position1 = draws[0].position;
draws.length = 0;
player.bodyFrame.Y = 784;
shieldLayer.Draw(info);
assert.equal(draws[0].source.Y, 240, 'walking cannot select empty frames');
assert.deepEqual(draws[0].position, position1);
assert.ok(draws[0].position.X >= 12 && draws[0].position.X < 30);
assert.ok(draws[0].position.Y >= 27 && draws[0].position.Y < 50);
draws.length = 0;
effects = [{ kind: 2, style: 2, color: 0xff66ccff, opacity: 0.8, scale: 1.1, texture: 'ward.png' }];
barrierLayer.Draw(info);
assert.equal(draws.length, 2, 'two compact barrier glow sprites');
assert.ok(draws.every(d => d.texture.name === 'ward.png'));
assert.ok(draws.every(d => d.source.Width === 52 && d.source.Height === 72));
assert.ok(draws.every(d => d.position.X === 20 && d.position.Y === 41));
draws.length = 0;
effects = [{ kind: 4, style: 3, color: 0xffff9632, opacity: 0.85, scale: 1, texture: 'spark.png' }];
for (let i = 0; i < 10; i++) {
    player.position = { X: 1010 + i * 7, Y: 1020 };
    info.Position = player.position;
    trailLayer.Draw(info);
}
assert.ok(draws.length > 0 && draws.length < 60, 'bounded subtle trail');
assert.ok(draws.every(d => d.texture.name === 'spark.png'));
assert.ok(draws.every(d => d.source.Width === 12 && d.source.Height === 14));
assert.ok(draws.every(d => d.scale < 1.3));
const last = draws[draws.length - 1].position;
assert.ok(last.Y > 30 && last.Y < 75, 'trail at feet');
player.velocity.X = 0;
for (let i = 0; i < 19; i++) trailLayer.Draw(info);
assert.equal(Visual.Trails.size, 0, 'short-lived trail cleanup');
effects = [];
assert.equal(trailLayer.GetDefaultVisibility(info), false);
assert.equal(Visual.Trails.size, 0);
console.log('CSHARP_VISUAL_GEOMETRY_PASS=YES');
