from pathlib import Path

root = Path(__file__).resolve().parents[2]
native = (root / "app/src/main/cpp/experiments/csharp/NativeCommandChatHooks.cpp").read_text()
helpers = (root / "app/src/main/cpp/script/js/ModHelpers.js").read_text()
commands = (root / "app/src/main/cpp/script/js/mod/Loaders/CommandLoader.js").read_text()

assert "Microsoft.Xna.Framework.Graphics.Color" in helpers
assert 'findClassQuiet("Microsoft.Xna.Framework.Graphics", "Color")' in native
assert '"Microsoft.Xna.Framework.Graphics.Color", "System.Int32"' in native
assert 'findClassQuiet("Microsoft.Xna.Framework", "Color")' not in native
assert '"Microsoft.Xna.Framework.Color", "System.Int32"' not in native
assert "SendChatMessageToClient(NetworkText text, Color color, int playerId)" in commands
for gate in ("SendClient", "ColorConstructor", "ColorConstructorReturn", "MobileColorLayout"):
    assert f'{{"{gate}",' in native
assert 'if (!validChatAbi) return pending("ChatSignatureOrLayoutMismatch");' in native
print("NATIVE_CHAT_GRAPHICS_COLOR_ABI_PASS")
