# ModInput / Vanilla Touch Editor integration

This patch adds a shared mod-input layer to Bunny Loader and integrates registered mobile buttons with Terraria's touch-control editing screen without changing Terraria's internal control enum or save format.

## What it adds

- `bl.input.registerButton(id, label)`
- `bl.input.state(id)`
- `ModInput.RegisterButton(id, options)`
- `ModInput.SecondaryUse`
- `Pressed`, `JustPressed`, and `JustReleased`
- `ModInput.AimDirection(player)`
- `ModInput.SyncAim(player)`
- Android touch buttons created only for controls registered by loaded mods
- Position and size persistence
- Terraria touch-editor detection through `GUIVirtualInputControllerEdit.Draw`
- Vanilla-style selection, sizing, multi-touch and binding panel while the touch editor is open
- Gameplay mode locks button position so normal use cannot accidentally move it

## Why it does not patch Terraria's control enum

Custom controls are intentionally kept outside Terraria's serialized vanilla control table. Mods can add arbitrary control IDs, while Terraria's internal control IDs and saved layouts remain untouched. The editor integration is visual and behavioral; Bunny stores only the custom control layout.

## Files

Core API:

- `app/src/main/cpp/script/api/Input.h`
- `app/src/main/cpp/script/api/Input.cpp`
- `app/src/main/cpp/script/bridge/Bindings.cpp`
- `app/src/main/cpp/script/js/ModHelpers.js`

Touch/editor bridge:

- `app/src/main/cpp/menu/CheatButton.cpp`
- `tools/cheatbridge/bunny/CheatBridge.java`

Example:

- `samples/ExampleMod/content/Common/Players/ExampleSecondaryUsePlayer.js`

## Example

```js
export class ExampleSecondaryUsePlayer extends ModPlayer {
    PostUpdateMiscEffects(player) {
        if (player.whoAmI !== Terraria.Main.myPlayer || Terraria.Main.gameMenu) return;

        const secondary = ModInput.SecondaryUse;
        if (!secondary.JustPressed) return;

        const aim = ModInput.SyncAim(player);
        bl.log('SecondaryUse: aim ' + aim.X.toFixed(3) + ', ' + aim.Y.toFixed(3));
    }
}
```

A mod only needs to read `ModInput.SecondaryUse`. Registration is shared, so multiple mods using the same logical input do not create duplicate buttons.

## Touch editor behavior

Outside Terraria's touch editor, the custom control behaves only as an input button. Position and size are locked.

Inside the touch editor:

- tap the custom control to select it
- drag with one finger to move it
- pinch with two fingers to resize it
- use the Size row for precise sizing
- Multi Touch is stored per custom control
- Binding shows the logical Bunny input ID
- Reset Control restores the default position and size

The custom layout is stored in Bunny Loader preferences and does not modify Terraria's native control profile.
