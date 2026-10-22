// The native service owns IDs, six engine hooks, fades, geometry and textures.
// Both languages use the same registry; JS no longer allocates overlapping IDs
// or enters QuickJS just to discover that no mod background is visible.
class SurfaceBackgroundLoader {
    static List = [];
    static #bySlot = new Map();
    static #vanillaCount = 0;
    static get VanillaCount() {
        return SurfaceBackgroundLoader.#vanillaCount || (SurfaceBackgroundLoader.#vanillaCount = Terraria.ID.SurfaceBackgroundID.Count);
    }
    static Add(inst) {
        inst.Slot = bl.reserveSurfaceStyle(inst.Mod.uuid, inst.constructor.name);
        SurfaceBackgroundLoader.List.push(inst);
        SurfaceBackgroundLoader.#bySlot.set(inst.Slot, inst);
        Hooks.Once('bg.surface', () => Ready.Add(() => {
            bl.installSurfaceBackgrounds(() => {
                const scene = SceneEffectLoader.Of(Terraria.Main.LocalPlayer).surfaceBackground;
                if (!scene || !SurfaceBackgroundLoader.Get(scene.value)) return [-1, 0, 0];
                return [scene.value, scene.priority, scene.weight ?? 0.5];
            }, () => MenuLoader.BackgroundSlot(), (slot, operation, speed, fades) => {
                const style = SurfaceBackgroundLoader.Get(slot);
                if (!style) throw new Error('Surface style not owned by this JS provider: ' + slot);
                if (operation === 0) {
                    style.ModifyFarFades(fades, speed);
                    return { fades };
                }
                if (operation === 1) return { texture: style.ChooseFarTexture() };
                if (operation === 2) return { texture: style.ChooseMiddleTexture() };
                if (operation !== 3) throw new Error('Unsupported surface operation: ' + operation);
                const defaultClose = style.PreDrawCloseBackground(Terraria.Main.spriteBatch) !== false;
                const scale = new Ref(), parallax = new Ref(), a = new Ref(), b = new Ref();
                scale.value = 1.25; parallax.value = 0.37; a.value = 1800; b.value = 1750;
                const texture = defaultClose ? style.ChooseCloseTexture(scale, parallax, a, b) : -1;
                return { texture, defaultClose, scale: Number(scale.value), parallax: Number(parallax.value), a: Number(a.value), b: Number(b.value) };
            });
        }, 'setup'));
    }
    static Get(slot) { return SurfaceBackgroundLoader.#bySlot.get(slot); }
    static Wake() { if (SurfaceBackgroundLoader.List.length) bl.wakeSurfaceBackgrounds(); }
}
