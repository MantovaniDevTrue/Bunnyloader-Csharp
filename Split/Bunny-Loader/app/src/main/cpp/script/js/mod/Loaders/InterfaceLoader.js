class InterfaceLoader {
    static Install() {
        Hooks.Once('system.InterfaceLayers', () => {
            const Main = Terraria.Main;
            Main['void DrawInterface(GameTime gameTime)'].hook((original, self, gameTime) => {
                if (Main.gameMenu) return original(self, gameTime);

                const layers = new InterfaceLayerList();
                layers.Add(GameInterfaceLayer.Marker('Vanilla: Resource Bars'));
                layers.Add(GameInterfaceLayer.Marker('Vanilla: Inventory'));
                layers.Add(GameInterfaceLayer.Marker('Vanilla: Mouse Text'));

                SystemLoader.Each('ModifyInterfaceLayers', (system) => system.ModifyInterfaceLayers(layers));

                for (const layer of layers) {
                    if (!layer || layer.IsVanilla || layer.Active === false || typeof layer.ReserveInput !== 'function') continue;
                    Safe.Run((layer.Name || 'InterfaceLayer') + '.ReserveInput', () => layer.ReserveInput());
                }

                original(self, gameTime);

                for (const layer of layers) {
                    if (!layer || layer.IsVanilla || layer.Active === false) continue;

                    if (typeof layer.Draw === 'function') {
                        Safe.Run((layer.Name || 'InterfaceLayer') + '.Draw', () => layer.Draw());
                    } else if (typeof layer === 'function') {
                        Safe.Run('InterfaceLayer', () => layer());
                    }
                }
            });
        });
    }
}
