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

                let captureInput = false;
                for (const layer of layers) {
                    if (!layer || layer.IsVanilla || layer.Active === false || typeof layer.ReserveInput !== 'function') continue;
                    if (Safe.Run((layer.Name || 'InterfaceLayer') + '.ReserveInput', () => layer.ReserveInput()) === true) captureInput = true;
                }

                if (captureInput) {
                    const x = Main.mouseX;
                    const y = Main.mouseY;
                    const left = Main.mouseLeft;
                    const leftRelease = Main.mouseLeftRelease;
                    const right = Main.mouseRight;
                    const rightRelease = Main.mouseRightRelease;
                    Main.mouseX = -100000;
                    Main.mouseY = -100000;
                    Main.mouseLeft = false;
                    Main.mouseRight = false;
                    try {
                        original(self, gameTime);
                    } finally {
                        Main.mouseX = x;
                        Main.mouseY = y;
                        Main.mouseLeft = left;
                        Main.mouseLeftRelease = leftRelease;
                        Main.mouseRight = right;
                        Main.mouseRightRelease = rightRelease;
                    }
                } else {
                    original(self, gameTime);
                }

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
