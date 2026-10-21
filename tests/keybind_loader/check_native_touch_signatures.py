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

print(f"Native touch signature parity PASS ({total} signatures)")
