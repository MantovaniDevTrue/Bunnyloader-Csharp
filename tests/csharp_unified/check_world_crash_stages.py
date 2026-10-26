from pathlib import Path
import re

source = Path("app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
body = source.split("void noteAutoGameUpdate() {", 1)[1].split("bool preWorldCSharpContentReady()", 1)[0]
stages = [int(value) for value in re.findall(r"setNativeGameStage\((\d+)\)", body)]
assert stages == list(range(100, 128)), stages
assert "struct StageReset { ~StageReset() { setNativeGameStage(0); } } stageReset;" in body
expected = {
    114: "touch::pumpNativeTouchActivationOnGameThread();",
    116: "pumpInteractiveConsoleOnGameThread();",
    117: "pumpBasicRecipesOnGameThread();",
    118: "angler::pumpOnGameThread();",
    119: "pumpNativeVisualRendererOnGameThread();",
    120: "flushDeferredWorldEntry();",
    123: "graphics::pumpCSharpScreenEffects();",
    124: "advanceManagedVisualState(tick, 32);",
}
for stage, call in expected.items():
    pos = body.index(f"setNativeGameStage({stage});")
    delta = body[pos:pos + 360]
    if stage == 114:
        assert body.index(call) < pos, (stage, call)
    else:
        assert call in delta, (stage, call)
print("WORLD_CRASH_STAGE_GATES=PASS", len(stages))
