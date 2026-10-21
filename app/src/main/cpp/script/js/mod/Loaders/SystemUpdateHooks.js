class SystemUpdateHooks {
    static Install(cls) {
        const has = name => Hooks.Overrides(cls, ModSystem, name);
        const Main = Terraria.Main;
        const call = SystemLoader.Call;
        const pairs = [
            ['PlayersBegin', ['PreUpdatePlayers']],
            ['PlayersEnd', ['PostUpdatePlayers']],
            ['NPCsBegin', ['PreUpdateNPCs']],
            ['NPCsEnd', ['PostUpdateNPCs', 'PreUpdateGores']],
            ['GoresEnd', ['PostUpdateGores']],
            ['ItemsBegin', ['PostUpdateProjectiles', 'PreUpdateItems']],
            ['ItemsEnd', ['PostUpdateItems']],
        ];
        for (const [stage, methods] of pairs) {
            if (methods.some(has)) Hooks.Once('system.stage.' + stage, () => {
                bl.installSystemStage(stage, () => {
                    for (const method of methods) call(method);
                });
            });
        }
        if (has('PreUpdateEntities') || has('PostUpdateEverything')) Hooks.Once('system.UpdateEverything', () => {
            Main['void DoUpdateInWorld()'].hook((original, self) => {
                call('PreUpdateEntities');
                original(self);
                call('PostUpdateEverything');
            });
        });
        if (has('PreUpdateProjectiles')) Hooks.Once('system.ProjectilesBegin', () => {
            Main['void PreUpdateAllProjectiles()'].hook((original, self) => {
                call('PreUpdateProjectiles');
                original(self);
            });
        });
        if (has('PreUpdateDusts') || has('PostUpdateDusts')) Hooks.Once('system.Dusts', () => {
            Terraria.Dust['void UpdateDust()'].hook(original => {
                call('PreUpdateDusts');
                original();
                call('PostUpdateDusts');
            });
        });
        if (has('PreUpdateInvasions') || has('PostUpdateInvasions')) Hooks.Once('system.Invasions', () => {
            Main['void UpdateInvasion()'].hook(original => {
                call('PreUpdateInvasions');
                original();
                call('PostUpdateInvasions');
            });
        });
        if (has('UpdateUI')) Hooks.Once('system.UI', () => {
            Main['void UpdateUIStates(GameTime gameTime)'].hook((original, time) => {
                original(time);
                if (!Main.gameMenu) call('UpdateUI', time);
            });
        });
        if (has('PostUpdateInput')) Hooks.Once('system.Input', () => {
            Main['void DoUpdate_HandleInput()'].hook((original, self) => {
                original(self);
                call('PostUpdateInput');
            });
        });
        if (has('ModifyTimeRate')) Hooks.Once('system.TimeRate', () => {
            bl.systemTimeRatePrepare();
            bl.installSystemStage('ClockAdvance', () => bl.systemTimeRateClock());
            Main['void UpdateTimeRate()'].hook(original => {
                const entered = bl.systemTimeRateBegin();
                try {
                    original();
                    if (!entered) return;
                    const initial = bl.systemTimeRateRead();
                    const time = new Ref(initial[0]), tiles = new Ref(initial[1]), events = new Ref(initial[2]);
                    call('ModifyTimeRate', time, tiles, events);
                    bl.systemTimeRatePublish(
                        SystemUpdateHooks.#Rate(time.value, initial[0]),
                        SystemUpdateHooks.#Rate(tiles.value, initial[1]),
                        SystemUpdateHooks.#Rate(events.value, initial[2]));
                } finally {
                    if (entered) bl.systemTimeRateEnd();
                }
            });
        });
    }

    static #Rate(value, fallback) {
        return Number.isFinite(value) && value >= 0 && value <= 2147483647 ? value : fallback;
    }
}
