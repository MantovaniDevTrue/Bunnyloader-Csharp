// Historical geometry oracle, excluded from the runtime.
class BunnyCSharpVisualRender {
    static Textures = new Map();
    static Trails = new Map();
    static Pulse = 0;
    static DrawCtor = 'void .ctor(Texture2D texture, Vector2 position, Rectangle sourceRect, Color color, float rotation, Vector2 origin, float scale, SpriteEffects effect, int inactiveLayerDepth)';

    static Effects(player, mode) {
        const api = bl.csharp;
        if (!api || typeof api.visualSnapshot !== 'function') return [];
        const list = api.visualSnapshot(player.whoAmI);
        if (!list || !list.length) return [];
        return list.filter(effect => (effect.style || (effect.kind === 2 ? 2 : 1)) === mode);
    }

    static Visible(info) {
        const player = info.drawPlayer;
        if (info.shadow !== 0 || player.dead || player.invis) return false;
        const api = bl.csharp;
        return !!api && typeof api.visualActiveCount === 'function' &&
            api.visualActiveCount() > 0;
    }

    static Texture(name) {
        if (typeof name !== 'string' || !name) return null;
        if (this.Textures.has(name)) return this.Textures.get(name);
        if (this.Textures.size >= 24) this.Textures.clear();
        let value = null;
        try {
            const path = bl.csharp.visualTexturePath(name);
            if (path) value = bl.loadTexture(path);
        } catch (error) {
            Safe.Once('csharp.visual.texture:' + name, 'CSharp visual: textura indisponivel ' + name);
        }
        this.Textures.set(name, value);
        return value;
    }

    static Tint(effect, fraction = 1) {
        const raw = Number(effect.color) >>> 0;
        const opacity = Math.max(0, Math.min(1, Number(effect.opacity) * fraction));
        const alpha = Math.round((raw >>> 24) * opacity);
        return Color.new((raw >>> 16) & 255, (raw >>> 8) & 255, raw & 255, alpha);
    }

    static Sprite(info, texture, x, y, color, scale, frame = 0, rotation = 0) {
        if (!texture || color.A <= 0) return false;
        const width = texture.Width, height = texture.Height;
        if (!Number.isInteger(width) || !Number.isInteger(height) ||
            width < 1 || width > 256 || height < 1 || height > 2048) return false;
        const sheet = height === width * 28;
        const frameHeight = sheet ? width : height;
        const sourceY = sheet ? Math.max(0, Math.min(27, frame)) * frameHeight : 0;
        const data = Terraria.DataStructures.DrawData.new();
        data[this.DrawCtor](texture,
            Vector2.new(Math.round(x), Math.round(y)),
            Rectangle.new(0, sourceY, width, frameHeight),
            color, rotation,
            Vector2.new(width / 2, frameHeight / 2),
            scale, info.playerEffect, 0);
        data.shader = 0;
        return ModPlayer.AddDrawData(info, data);
    }

    static Position(info) {
        const player = info.drawPlayer, screen = Terraria.Main.screenPosition;
        const pos = info.Position;
        return {
            x: pos.X - screen.X + player.width / 2,
            y: pos.Y - screen.Y + player.height / 2,
            torso: pos.Y - screen.Y + player.height -
                (player.bodyFrame && player.bodyFrame.Height > 0
                    ? player.bodyFrame.Height / 2 : player.height / 2) + 7
        };
    }

    static Shield(info) {
        const player = info.drawPlayer;
        const effects = this.Effects(player, 1);
        if (!effects.length) return;
        const pos = this.Position(info);
        for (const effect of effects) {
            const texture = this.Texture(effect.texture);
            if (!texture) continue;
            const color = this.Tint(effect);
            const scale = Math.max(0.5, Math.min(1.5, Number(effect.scale) || 1));
            this.Sprite(info, texture, pos.x - player.direction * 4,
                pos.torso, color, scale, 6, player.bodyRotation || 0);
        }
    }

    static Barrier(info) {
        const effects = this.Effects(info.drawPlayer, 2);
        if (!effects.length) return;
        const pos = this.Position(info);
        const pulse = 1 + Math.sin(++this.Pulse * 0.09) * 0.035;
        for (const effect of effects) {
            const texture = this.Texture(effect.texture);
            if (!texture) continue;
            const scale = Math.max(0.6, Math.min(1.4, Number(effect.scale) || 1));
            this.Sprite(info, texture, pos.x, pos.y,
                this.Tint(effect, 0.2), scale * 1.08 * pulse);
            this.Sprite(info, texture, pos.x, pos.y,
                this.Tint(effect, 0.94), scale * pulse);
        }
    }

    static Trail(info) {
        const player = info.drawPlayer;
        const id = player.whoAmI;
        const effects = this.Effects(player, 3);
        if (!effects.length) {
            this.Trails.delete(id);
            return;
        }
        const effect = effects[0];
        const texture = this.Texture(effect.texture);
        if (!texture) return;
        let points = this.Trails.get(id);
        if (!points) {
            points = [];
            this.Trails.set(id, points);
        }
        const tick = ++this.Pulse;
        const vel = player.velocity;
        const moving = vel && Math.abs(vel.X) > 1.25 &&
            Math.abs(vel.Y) < 0.3 && !player.mount.Active;
        const x = player.position.X + player.width / 2;
        const y = player.position.Y + player.height - 3;
        const last = points[points.length - 1];
        if (moving && (!last || Math.abs(x - last.x) + Math.abs(y - last.y) > 6))
            points.push({ x, y, tick });
        while (points.length > 6 || (points.length && tick - points[0].tick > 16))
            points.shift();
        if (!points.length) {
            this.Trails.delete(id);
            return;
        }
        const screen = Terraria.Main.screenPosition;
        for (const point of points) {
            const life = Math.max(0, 1 - (tick - point.tick) / 17);
            if (life <= 0) continue;
            this.Sprite(info, texture,
                point.x - screen.X - player.direction * 3,
                point.y - screen.Y - 5, this.Tint(effect, life * 0.72),
                Math.max(0.3, Math.min(1.15, (0.35 + 0.55 * life) * effect.scale)));
        }
    }
}

class BunnyCSharpTrailLayer extends PlayerDrawLayer {
    static NativeEquipPoll = 0;
    constructor() { super('BunnyCSharpTrail'); }
    GetDefaultPosition() { return PlayerDrawLayer.BeforeParent(PlayerDrawLayers.BackAcc); }
    GetDefaultVisibility(info) {
        if (++BunnyCSharpTrailLayer.NativeEquipPoll % 120 === 1)
            EquipLoader.Install();
        const visible = BunnyCSharpVisualRender.Visible(info);
        if (!visible) BunnyCSharpVisualRender.Trails.delete(info.drawPlayer.whoAmI);
        return visible;
    }
    Draw(info) { BunnyCSharpVisualRender.Trail(info); }
}

class BunnyCSharpShieldLayer extends PlayerDrawLayer {
    constructor() { super('BunnyCSharpShield'); }
    GetDefaultPosition() { return PlayerDrawLayer.AfterParent(PlayerDrawLayers.Shield); }
    GetDefaultVisibility(info) { return BunnyCSharpVisualRender.Visible(info); }
    Draw(info) { BunnyCSharpVisualRender.Shield(info); }
}

class BunnyCSharpBarrierLayer extends PlayerDrawLayer {
    constructor() { super('BunnyCSharpBarrier'); }
    GetDefaultPosition() { return PlayerDrawLayer.AfterParent(PlayerDrawLayers.IceBarrier); }
    GetDefaultVisibility(info) { return BunnyCSharpVisualRender.Visible(info); }
    Draw(info) { BunnyCSharpVisualRender.Barrier(info); }
}

PlayerDrawHooks.AddLayer(new BunnyCSharpTrailLayer(), BunnyCSharpTrailLayer);
PlayerDrawHooks.AddLayer(new BunnyCSharpShieldLayer(), BunnyCSharpShieldLayer);
PlayerDrawHooks.AddLayer(new BunnyCSharpBarrierLayer(), BunnyCSharpBarrierLayer);
