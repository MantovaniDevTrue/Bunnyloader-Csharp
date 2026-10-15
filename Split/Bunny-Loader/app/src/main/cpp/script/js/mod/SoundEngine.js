class SoundEngine {
    static PlaySound(style, position) {
        const nativeStyle = SoundLoader.Of(style);
        if (nativeStyle >= 0) {
            return position
                ? SoundLoader.Play(nativeStyle, position.X, position.Y, 1, 0)
                : SoundLoader.Play(nativeStyle, -1, -1, 1, 0);
        }

        const Engine = Terraria.Audio.SoundEngine;
        if (typeof style === 'number') {
            const x = position ? Math.floor(position.X) : -1, y = position ? Math.floor(position.Y) : -1;
            return Engine['SoundEffectInstance PlaySound(int type, int x, int y, int Style, float volumeScale, float pitchOffset)'](style, x, y, 1, 1, 0);
        }
        return position
            ? Engine['SoundEffectInstance PlaySound(LegacySoundStyle type, Vector2 position, float pitchOffset, float volumeScale)'](style, position, 0, 1)
            : Engine['SoundEffectInstance PlaySound(LegacySoundStyle type, int x, int y, float pitchOffset, float volumeScale)'](style, -1, -1, 0, 1);
    }

    static FindActiveSound(style) {
        const nativeStyle = SoundLoader.Of(style);
        return nativeStyle >= 0 ? SoundLoader.FindActive(nativeStyle) : 0;
    }

    // -1 falhou; 0 terminou/desconhecido; 1 pendente; 2 tocando; 3 pausado;
    // 4 cancelado; 5 descartado por atraso. Histórico limitado aos últimos pedidos.
    static GetSoundState(handle) {
        return handle > 0 ? bl.sounds.playbackState(handle) : 0;
    }

    static StopSound(handle) {
        if (handle > 0) bl.sounds.cancel(handle);
    }
}

Object.freeze(SoundEngine);
