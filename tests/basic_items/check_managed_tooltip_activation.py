from pathlib import Path

root = Path(__file__).resolve().parents[2]
exports = (root / "app/src/main/cpp/script/js/mod/Exports.js").read_text()
tooltip = (root / "app/src/main/cpp/script/js/mod/Loaders/TooltipLoader.js").read_text()
native = (root / "app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp").read_text()
api = (root / "app/src/main/cpp/script/api/CSharpPrototypeApi.cpp").read_text()
writer = (root / "experiments/csharp/BunnySourceCompiler/BasicItemWriter.cs").read_text()
example = (root / "samples/ExampleModCSharp/Playable/content/Content/Items/ExampleTooltipItem.cs").read_text()
assert "typeof globalThis.bl?.csharp?.tooltipPlan === 'function'" in exports
assert "Hooks.Once('item.Tooltips', TooltipLoader.Install);" in exports
assert "static #ManagedPlan(type)" in tooltip
assert "if (TooltipLoader.#ManagedPlan(item.type)) return true;" in tooltip
assert "TooltipLoader.#ModifyManaged(item, lines);" in tooltip
assert "dl.__bunnyManagedDraw = line.__bunnyManagedDraw;" in tooltip
assert "TooltipLoader.#DrawManagedWave(line, def);" in tooltip
assert "g_frame" not in example
assert "Stage=PlanStored" in native
assert "return it==definitions.end()?std::string{}:it->second.tooltipPlan" in native
assert 'JS_SetPropertyStr(ctx, api, "tooltipPlan"' in api
assert "managed.ModifyTooltips(lines)" in writer
assert "managed.PreDrawTooltipLine(draw)" in writer
assert 'line.OverrideColor = "FFD75A"' in example
assert "line.RainbowWave = true;" in example
print("CSHARP_TOOLTIP_HOOK_AND_DRAW_PLAN=PASS")
