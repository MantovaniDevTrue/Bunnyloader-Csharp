from pathlib import Path

root = Path(__file__).resolve().parents[2]
reference = (root / "app/src/main/cpp/script/js/mod/Loaders/TouchButtonHooks.js").read_text()
checks = {
    "NativeTouchController.cpp": ("void Update(float elapsedTime)",),
    "NativeTouchPointer.cpp": (
        "void HandleTouchCursorDown(int slot, Vector2 cursorUIPosition, Cursor cursor)",
    ),
    "NativeTouchVisibility.cpp": (
        "bool DetermineButtonVisible(int slot, VirtualControlsHardwareConfigurationMapping_Layout.ContolType action, VirtualControlsHardwareConfigurationMapping_Layout slotConfig)",
    ),
    "NativeTouchEditor.cpp": (
        "string GetControlName(VirtualControlsHardwareConfigurationMapping_Layout.ContolType control)",
    ),
    "NativeTouchIconHooks.cpp": (
        "void Draw(QuickActionButton_Layout.QuickActionControl actionType, QuickActionButton_Layout layout, bool pressed, ref float scale, bool ignoreChecks, bool ignoreIcon)",
        "void Draw(QuickActionButton_Layout layout, Texture2D buttonTexture, GUIQuickActionButton.InteractionState state, ref float scale)",
        "void GetIconFrame(QuickActionButton_Layout.QuickActionControl actionType, out Texture2D iconTexture, out Rectangle frame)",
        "void GetControlTextureFrame(VirtualControlsHardwareConfigurationMapping_Layout.ContolType controlType, out Texture2D tex, out Rectangle frame)",
    ),
    "NativeTouchLayoutHooks.cpp": (
        "void SaveConfiguration(Preferences preferences)",
        "void LoadConfiguration(Preferences preferences)",
        "string GetShareCode(VirtualControlsProfile_Layout profile)",
    ),
}

total = 0
for filename, signatures in checks.items():
    path = root / "app/src/main/cpp/experiments/csharp" / filename
    code = path.read_text()
    for signature in signatures:
        if "'" + signature + "'" not in reference:
            raise SystemExit(f"ExampleModBunny reference missing: {signature}")
        if '"' + signature + '"' not in code:
            raise SystemExit(f"{filename} diverged from ExampleModBunny: {signature}")
        total += 1


controller = (root / "app/src/main/cpp/experiments/csharp/NativeTouchController.cpp").read_text()
bootstrap = controller.split("void pumpNativeTouchObserverOnGameThread()", 1)[1]
expected_order = [
    "static uint32_t probes = 0;",
    "probes % 120 != 0",
    "HostAbiCheck check = inspectNativeTouchAbi();",
    "refs.controller = controller;",
    "attempted = true;",
    "bl::hook::install(update, onControllerUpdate"
]
positions = [bootstrap.find(marker) for marker in expected_order]
if -1 in positions or positions != sorted(positions):
    raise SystemExit("Touch bootstrap must retry incomplete ABI before latching hook installation")

activation = (root / "app/src/main/cpp/experiments/csharp/NativeTouchActivation.cpp").read_text()
if "tick >= 600 && tick % 600 == 0" not in activation:
    raise SystemExit("Touch activation status must remain visible after late startup")

commands = (root / "app/src/main/cpp/experiments/csharp/NativeCommandChatHooks.cpp").read_text()
for marker in (
    'pending("Il2CppApiUnavailable")',
    'pending("ChatClassesUnavailable")',
    'pending("ChatSignatureOrLayoutMismatch")',
    'pending("ChatDetourInstallFailed")',
):
    if marker not in commands:
        raise SystemExit(f"Missing native command diagnostic: {marker}")

console = (root / "app/src/main/cpp/script/api/Console.cpp").read_text()
on_log = console.split("void onLogLine(int level, const char* line)", 1)[1].split("void queueChat", 1)[0]
if 'std::strncmp(line, "[CSharp]", 8) == 0' not in on_log:
    raise SystemExit("Native C# diagnostics must not be forwarded to the JS console")

profiles = (root / "app/src/main/cpp/experiments/csharp/NativeTouchProfiles.cpp").read_text()
for marker in (
    "if (Il2CppObject* active = read<Il2CppObject*>(styles, position))",
    "if (!styleOf(profile, slot, &temp.style)) temp.style = -1;",
    "if (temp.style == 2 || temp.style < 0)",
    "nativeTouchLastLayoutStatus()",
    'bunny_csharp_touch_layout_status()',
):
    if marker not in profiles:
        raise SystemExit(f"Active touch layout cannot recover direct bindings: {marker}")
qa = (root / "samples/FiveLoadersCSharp/Playable/content/Common/Commands/QaCommand.cs").read_text()
if '"TouchLayout: " + BunnyTouchButtonLoader.LayoutDiagnostics()' not in qa:
    raise SystemExit("QA5 is missing native layout status in /lote")

print(f"Native touch signature parity and bootstrap gate PASS ({total} signatures)")
