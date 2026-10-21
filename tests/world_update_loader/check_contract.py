from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
original = (ROOT / "app/src/main/cpp/script/js/mod/Loaders/SystemUpdateHooks.js").read_text()
sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyWorldUpdateLoader.cs").read_text()
docs = (ROOT / "samples/ExampleModCSharp/Playable/content/Documentation/SYSTEM_UPDATE_LOADER.md").read_text()
lifecycle = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyUniversalHooks.cs").read_text()

complete = {
    "PreUpdatePlayers", "PostUpdatePlayers", "PreUpdateNPCs",
    "PostUpdateNPCs", "PreUpdateGores", "PostUpdateGores",
    "PostUpdateProjectiles", "PreUpdateItems", "PostUpdateItems",
    "PreUpdateEntities", "PostUpdateEverything", "PreUpdateProjectiles",
    "PreUpdateDusts", "PostUpdateDusts", "PreUpdateInvasions",
    "PostUpdateInvasions", "UpdateUI", "PostUpdateInput",
    "ModifyTimeRate",
}

implemented = set(re.findall(r"\bprotected virtual void (\w+)\s*\(", sdk))
expected = {
    "PreUpdateEntities", "PostUpdateEverything", "PreUpdateProjectiles",
    "PreUpdateDusts", "PostUpdateDusts", "PreUpdateInvasions",
    "PostUpdateInvasions", "PostUpdateInput",
}
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
assert "UpdateUIStates(GameTime gameTime)" in original
assert "Main['void DoUpdateInWorld()']" in original
assert 'new("Terraria.Main", "DoUpdateInWorld", false,' in sdk
assert 'new("Terraria.Main", "PreUpdateAllProjectiles", false,' in sdk
assert 'new("Terraria.Dust", "UpdateDust", true,' in sdk
assert 'new("Terraria.Main", "UpdateInvasion", true,' in sdk
assert 'new("Terraria.Main", "DoUpdate_HandleInput", false,' in sdk
assert lifecycle.index("BunnyWorldUpdateRegistry.DetachScope(context.Scope)") < lifecycle.index(
    "context.Scope.Dispose();", lifecycle.index("private static void Close(Context context)")
)
print(f"Auditoria de fontes OK: {len(implemented)}/19 callbacks implementados; "
      f"{len(complete - implemented)} pendentes; nenhum APK.")
