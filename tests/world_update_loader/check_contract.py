from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
original = (ROOT / "app/src/main/cpp/script/js/mod/Loaders/SystemUpdateHooks.js").read_text()
sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyWorldUpdateLoader.cs").read_text()
docs = (ROOT / "samples/ExampleModCSharp/Playable/content/Documentation/SYSTEM_UPDATE_LOADER.md").read_text()
lifecycle = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyUniversalHooks.cs").read_text()
native = (ROOT / "app/src/main/cpp/experiments/csharp/UniversalSystemStages.cpp").read_text()
original_hooks = (ROOT / "app/src/main/cpp/script/api/SystemHooks.cpp").read_text()

complete = {
    "PreUpdatePlayers", "PostUpdatePlayers", "PreUpdateNPCs",
    "PostUpdateNPCs", "PreUpdateGores", "PostUpdateGores",
    "PostUpdateProjectiles", "PreUpdateItems", "PostUpdateItems",
    "PreUpdateEntities", "PostUpdateEverything", "PreUpdateProjectiles",
    "PreUpdateDusts", "PostUpdateDusts", "PreUpdateInvasions",
    "PostUpdateInvasions", "UpdateUI", "PostUpdateInput",
    "ModifyTimeRate",
}

implemented = set(re.findall(r"\bprotected virtual void (\w+)\s*\(", sdk)) - {"OnUnloaded"}
expected = complete
assert implemented == expected, f"Escopo C# alterado sem auditoria: {implemented ^ expected}"
assert len(complete) == 19
for callback in complete:
    assert re.search(r"\b" + callback + r"\b", original), f"Ausente no Bunny JS: {callback}"
for callback in complete - implemented:
    assert callback in docs, f"Callback faltante nao documentado: {callback}"

for stage in (
    "PlayersBegin", "PlayersEnd", "NPCsBegin", "NPCsEnd",
    "GoresEnd", "ItemsBegin", "ItemsEnd", "ClockAdvance"
):
    assert stage in original, f"Referencia de estagio ausente: {stage}"
# Cada âncora nativa deve preservar os 4 opcodes originais, não só o nome.
for stage, offset in (
    ("PlayersBegin", "0x1b8"), ("PlayersEnd", "0x57c"),
    ("NPCsBegin", "0x884"), ("NPCsEnd", "0xf40"),
    ("GoresEnd", "0x114c"), ("ItemsBegin", "0x1740"),
    ("ItemsEnd", "0x1a14")
):
    assert f'{{"{stage}", {offset},' in native, f"Sem âncora {stage}"
    assert f'{{"{stage}", "void DoUpdateInWorld()", {offset},' in original_hooks, stage
assert 'std::memcmp(code + stage.offset, stage.expected' in native
assert 'originalWorldUpdate(self, info);' in native
assert 'installUniversalSystemStageOnGameThread' in native
assert "UpdateUIStates(GameTime gameTime)" in original
assert "Main['void DoUpdateInWorld()']" in original
assert 'new("Bunny.SystemStage", "EntitiesBegin", false,' in sdk
assert 'new("Bunny.SystemStage", "EntitiesEnd", false,' in sdk
assert 'new("Bunny.SystemStage", "UIEnd", false,' in sdk
assert '{"UIEnd", 0,' in native
assert '{"TimeRate", 0,' in native
assert '{"ClockAdvance", 0xb8c,' in native
assert 'new("Bunny.SystemStage", "TimeRate", false,' in sdk
assert 'bunny_system_time_rate_read' in native
assert 'bunny_system_time_rate_write' in native
assert 'tileRemainder = tilesWithFraction - std::floor(tilesWithFraction)' in native
assert 'UpdateUIStates(GameTime gameTime)' in native
assert 'readMobileMainMenu(gameMenuGetter, &gameMenu)' in native
assert 'resolveMobileMainMenu()' in native
assert 'findField(main, "gameMenu")' not in native
for stage in ("PlayersBegin", "PlayersEnd", "NPCsBegin", "NPCsEnd", "GoresEnd", "ItemsBegin", "ItemsEnd"):
    assert f'new("Bunny.SystemStage", "{stage}", false,' in sdk
assert 'new("Terraria.Main", "PreUpdateAllProjectiles", false,' in sdk
assert 'new("Terraria.Dust", "UpdateDust", true,' in sdk
assert 'new("Terraria.Main", "UpdateInvasion", true,' in sdk
assert 'new("Terraria.Main", "DoUpdate_HandleInput", false,' in sdk
assert lifecycle.index("BunnyWorldUpdateRegistry.DetachScope(context.Scope)") < lifecycle.index(
    "context.Scope.Dispose();", lifecycle.index("private static void Close(Context context)")
)
print(f"Auditoria de fontes OK: {len(implemented)}/19 callbacks implementados; "
      f"{len(complete - implemented)} pendentes; nenhum APK.")
