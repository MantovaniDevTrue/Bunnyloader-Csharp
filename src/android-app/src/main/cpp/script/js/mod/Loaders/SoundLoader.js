class SoundLoader {
    static ID = 1000;
    static Styles = new Map();
    static #ids = new Map();

    static Load(file, label) {
        let id = SoundLoader.#ids.get(file) || 0;
        if (!id) {
            id = Safe.Run('SoundStyle ' + label, () => bl.sounds.load(file)) || 0;
            SoundLoader.#ids.set(file, id);
        }
        return id;
    }

    static Register(sound) {
        return bl.sounds.registerStyle(sound.id, sound.volume, sound.pitch, sound.pitchVariance, sound.maxInstances, sound.limit);
    }

    static Of(style) {
        if (!style || typeof style !== 'object' || style.SoundId !== SoundLoader.ID) return -1;
        return style.Style | 0;
    }

    static Play(style, x, y, volumeScale, pitchOffset) {
        return bl.sounds.playStyle(style, x, y, volumeScale, pitchOffset);
    }

    static FindActive(style) {
        return bl.sounds.findActiveStyle(style);
    }
}
