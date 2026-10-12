# Bunny Loader C# .90.3 — accessory prefix tooltip in reforge

Android confirmed the .90.2 gameplay implementation works: the `Poderoso` weapon modifier displays `+22% de danos`, and the `Protegido` accessory modifier gives its `+4` defense bonus, but the reforge popup incorrectly says `Sem modificadores`.

The original Bunny Loader JavaScript already handles `GUIReforgePopup.UpdatePrefixes(Item reforgeItem)` for a `ModPrefix.GetTooltipLines` override. For managed C# modifiers, .90.3 adds an **optional native hook** on the same method that reports the defense bonus through the game's existing line arrays.

The hook calls the original popup method first, checks a registered managed accessory prefix, reads `Lang.tip[25].Value` for the player's active Terraria language, writes the signed defense amount into the first tooltip line with the IL2CPP GC write barrier, and sets its bad-modifier color flag based on the sign. It limits modification to 0- or 1-line popups to avoid overwriting independent lines. If the signature, fields, or localization API cannot be resolved, it leaves the popup unchanged and preserves reforging and accessory stats.

This does not add a public `GetTooltipLines` C# callback. The .90.3 change is specifically the automatic **accessory defense modifier line in the mobile reforge UI**. No change to the managed prefix damage, selection, pricing, save format or behavior hooks.

**Android validation:** open the reforge UI for `Protegido Example Focus Accessory`; expect `+4 de defesa` (localized), not `Sem modificadores`. Confirm the same +4 defense is still applied while equipped. Test with a non-prefix accessory and with a JS prefix; these must remain unchanged. Check for `reforgeTooltip=1` in Bunny logs. The ordinary inventory tooltip is not yet claimed as fixed; verify it separately.
