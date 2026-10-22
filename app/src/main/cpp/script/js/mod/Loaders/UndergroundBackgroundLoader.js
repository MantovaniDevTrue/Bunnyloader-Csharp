// Both language bridges use the native renderer service. IDs are reserved by
// UID/class; a C# style may occupy a gap between two JavaScript styles.
class UndergroundBackgroundLoader {
    static List = [];
    static VanillaCount = 22;
    static #bySlot = new Map();

    static Add(inst) {
        inst.Slot = bl.reserveUndergroundStyle(inst.Mod.uuid, inst.constructor.name);
        UndergroundBackgroundLoader.List.push(inst);
        UndergroundBackgroundLoader.#bySlot.set(inst.Slot, inst);
        Hooks.Once('bg.underground', () => Ready.Add(() => {
            bl.installUndergroundBackgrounds(() => {
                const scene = SceneEffectLoader.Of(Terraria.Main.LocalPlayer).undergroundBackground;
                if (!scene || !UndergroundBackgroundLoader.Get(scene.value)) return [-1, 0, 0];
                return [scene.value, scene.priority, scene.weight ?? 0.5];
            }, slot => {
                const style = UndergroundBackgroundLoader.Get(slot);
                if (!style) throw new Error('Underground style not owned by this JS provider: ' + slot);
                const textureSlots = [-1, -1, -1, -1, 6];
                style.FillTextureArray(textureSlots);
                return textureSlots;
            });
        }, 'setup'));
    }

    static Get(slot) { return UndergroundBackgroundLoader.#bySlot.get(slot); }
}
