from pathlib import Path

source = Path("app/src/main/cpp/experiments/csharp/NativeAnglerQuestGameplay.cpp").read_text()
runtime = Path("app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()

assert "setNativeGameStage(118);" in runtime
assert "angler::pumpOnGameThread();" in runtime
assert runtime.index("setNativeGameStage(118);") < runtime.index("angler::pumpOnGameThread();")
assert runtime.index("angler::pumpOnGameThread();") < runtime.index("setNativeGameStage(119);")

contract = source[source.index("bool findContracts() {"):source.index("struct PendingArrayRoots")]
assert "QuestTablesNotReady" in contract
assert contract.index("QuestTablesNotReady") < contract.index("hook::install(swap")
assert contract.index("QuestTablesNotReady") < contract.index("hook::install(chat")

pump = source[source.index("void pumpOnGameThread() {"):source.index("void invalidateWorldSession()")]
assert "~StageReset() { setNativeGameStage(118); }" in pump
for stage in (1181, 1182, 1183, 1190):
    assert f"setNativeGameStage({stage});" in pump
for stage in range(1184, 1190):
    assert f"setNativeGameStage({stage});" in source
assert "installFishingOnGameThread(&g_active)" in pump
assert "updateQuests(quests)" in pump
print("ANGLER_STAGE118_NATIVE_GUARDS=PASS")
