class ModMusic {
    static FADE = 0.005;
    static AUDIBLE = 0.25;
    static Tracks = [];
    static Slots = new Map();

    static #npcs = new Set();
    static #wanted = -1;
    static CHOOSE_EVERY = 10;
    static #chooseIn = 0;
    static #chosenTow = null;
    static #hooked = false;
    static #savedSilenceFade = null;
    static #base = 0;
    static #nativeTarget = -1;
    static #nativeVolume = -1;

    static Base() {
        if (!ModMusic.#base) ModMusic.#base = Safe.Run('MusicID.Count', () => Terraria.ID.MusicID.Count) || 105;
        return ModMusic.#base;
    }

    static Track(slot) {
        return ModMusic.Tracks[slot - ModMusic.Base()];
    }

    static TrackNpc(npc, m) {
        if ((m.Music | 0) >= 0 || ModMusic.Tracks.length > 0) ModMusic.Install();
        else if (!ModMusic.#hooked) return;
        ModMusic.#npcs.add(npc);
    }

    static Install() {
        Hooks.Once('music', () => {
            ModMusic.#hooked = true;
            Safe.Run('ModMusic: prewarm BossHigh', () => Terraria.GameContent.Events.CreditsRollEvent.IsEventOngoing);
            const Main = Terraria.Main;
            const decide = (tow) => (original, self) => {
                original(self);
                ModMusic.#Decide(tow);
            };
            Main['void UpdateAudio_DecideOnNewMusic()'].hook(decide(false), { ifBusy: 'skip' });
            Main['void UpdateAudio_DecideOnTOWMusic()'].hook(decide(true), { ifBusy: 'skip' });
        });
    }

    static #Choose(tow) {
        const Main = Terraria.Main;
        if (Main.gameMenu) return MenuLoader.MenuMusic();

        const box = ModMusic.#MusicBox();
        if (box > 0) return box;
        if (Main.SceneMetrics.ActiveMusicBox >= 0) return -1;

        let { music, priority } = ModMusic.#FromNpcs();
        const scene = SceneEffectLoader.List.length ? SceneEffectLoader.Of(Main.LocalPlayer).music : null;
        if (scene && scene.value > -1 && scene.priority > priority) {
            music = scene.value;
            priority = scene.priority;
        }
        if (music < 0 || priority < SceneEffectPriority.BiomeLow) return -1;

        const allowed = Safe.Run('ModMusic: degrau do jogo', () => VanillaMusic.Allows(priority, tow));
        return allowed === undefined || allowed ? music : -1;
    }

    static #FromNpcs() {
        const Main = Terraria.Main;
        const none = { music: -1, priority: SceneEffectPriority.None };
        if (!ModMusic.#npcs.size) return none;

        const margin = 5000;
        const sp = Main.screenPosition;
        const x0 = sp.X - margin;
        const x1 = sp.X + Main.screenWidth + margin;
        const y0 = sp.Y - margin;
        const y1 = sp.Y + Main.screenHeight + margin;

        let best = -1;
        let bestPriority = SceneEffectPriority.None;
        for (const npc of ModMusic.#npcs) {
            const live = npc.active && Main.npc[npc.whoAmI] === npc;
            const m = live ? NPCLoader.Of(npc) : undefined;
            if (!m) {
                ModMusic.#npcs.delete(npc);
                continue;
            }

            const music = m.Music | 0;
            if (music < 0) continue;

            const c = npc.Center;
            if (c.X < x0 || c.X > x1 || c.Y < y0 || c.Y > y1) continue;

            const priority = m.SceneEffectPriority | 0;
            if (best < 0 || priority > bestPriority) {
                best = music;
                bestPriority = priority;
            }
        }
        return { music: best, priority: bestPriority };
    }

    static #MusicBox() {
        if (!TileLoader.MusicBoxes.size) return 0;

        const Main = Terraria.Main;
        const sp = Main.screenPosition;
        const found = bl.tiles.find(
            'tile.musicbox',
            Math.floor(sp.X / 16),
            Math.floor(sp.Y / 16),
            Math.ceil((sp.X + Main.screenWidth) / 16),
            Math.ceil((sp.Y + Main.screenHeight) / 16)
        );

        for (let k = 0; k < found.length; k += 2) {
            const tile = TileLoader.Tile(found[k], found[k + 1]);
            if (tile.frameX < 36) continue;
            const boxes = TileLoader.MusicBoxes.get(tile.type);
            const box = boxes && boxes.get(tile.frameY - tile.frameY % 36);
            if (box) return box.slot;
        }

        const armor = Main.player[Main.myPlayer].armor;
        for (let k = 3; k < armor.length; k++) {
            if (k >= 10 && k < 13) continue;
            const slot = TileLoader.MusicBoxItems.get(armor[k].type);
            if (slot) return slot;
        }
        return 0;
    }

    static #SilenceGame(on) {
        const Main = Terraria.Main;
        const fade = Main.musicFade;
        if (on) {
            if (ModMusic.#savedSilenceFade === null) ModMusic.#savedSilenceFade = fade[0];
            fade[0] = 1;
            Main.newMusic = 0;
        } else if (ModMusic.#savedSilenceFade !== null) {
            fade[0] = ModMusic.#savedSilenceFade;
            ModMusic.#savedSilenceFade = null;
        }
    }

    static #Decide(tow) {
        const Main = Terraria.Main;
        if (--ModMusic.#chooseIn <= 0 || ModMusic.#chosenTow !== tow || Main.gameMenu) {
            ModMusic.#wanted = ModMusic.#Choose(tow);
            ModMusic.#chooseIn = ModMusic.CHOOSE_EVERY;
            ModMusic.#chosenTow = tow;
        }

        const wanted = ModMusic.#wanted;
        const track = wanted >= ModMusic.Base() ? ModMusic.Track(wanted) : null;
        const nativeTarget = track ? track.id : 0;

        const nativeVolume = Math.max(0, Math.min(1, Number(Main.musicVolume) || 0));
        if (nativeTarget !== ModMusic.#nativeTarget || Math.abs(nativeVolume - ModMusic.#nativeVolume) > 0.0001) {
            bl.music.target(nativeTarget, nativeVolume);
            ModMusic.#nativeTarget = nativeTarget;
            ModMusic.#nativeVolume = nativeVolume;
        }

        if (track && bl.music.fade(track.id) > ModMusic.AUDIBLE) {
            ModMusic.#SilenceGame(true);
            return;
        }

        ModMusic.#SilenceGame(false);
        if (track) Main.newMusic = Main.curMusic;
        else if (wanted >= 0) Main.newMusic = wanted;
    }
}
