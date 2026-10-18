# Bunny Loader C# .90.4 — restore mod prefix state on load and save

The Android .90.3 report confirmed the managed prefix gameplay hooks, but found prefix loss after quitting/reopening the player. The MonoVM probe returned 42, and the log reported three mod items restored; the earlier `0 prefixo(s) de mod em item do jogo` count only covers custom prefix IDs on vanilla items, **not** managed prefixes on ExampleModCSharp items.

The existing `ModItemSave::hkLoadPlayer` skipped all restoration when an item already had the expected type. Terraria may deserialize the item's type while leaving a mod prefix unapplied, so the correct item remained but the modifier did not. The new loader also invokes `Item.ResetPrefix` and `Item.Prefix` to reconstruct the applied managed modifier even when the raw prefix byte was present. It validates the resulting prefix and retains the prefix key as pending if it cannot apply.

The original game's player-save routine can alter item state while writing the `.plr`. The new save hook snapshots registered mod prefix IDs by item pointer/type before the original call, and uses the snapshot when the same item has prefix 0 afterward, before writing its stable identity into the `.plr.bl` sidecar. It does not alter the original Terraria `.plr` bytes or the layout of the `.bl` file.

The native reforge tooltip hook from .90.3 is retained without modification. The example C# packages and JS implementation are unchanged.

Validation: new prefix the Example C# Wand or Example Focus Accessory, take note of gameplay stats, exit/save, reopen the *same* character and world, check prefix names, damage/defense and reforge display, then repeat save/reload. Logs now report `prefixos de item C# repostos=N pendentes=M`. Test also vanilla items with managed prefixes and with JS prefixes. An already-lost modifier whose key was overwritten in the old sidecar cannot be reconstructed without reapplying it.

CI compile/static checks establish source integrity only; device testing is required before calling this Android fix confirmed.
