from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
cpp = (root / "app/src/main/cpp/boot/jni_entry.cpp").read_text()
kotlin = (root / "app/src/main/kotlin/dev/bunnyloader/nativebridge/NativeBridge.kt").read_text()
activity = (root / "app/src/main/kotlin/dev/bunnyloader/GameActivity.kt").read_text()

sig = {
    "String": "Ljava/lang/String;",
    "Int": "I",
    "Long": "J",
    "Boolean": "Z",
    "Array<String>": "[Ljava/lang/String;",
    "android.content.Context": "Landroid/content/Context;",
}
fields = dict(re.findall(r'@JvmField\s+val\s+(\w+)\s*:\s*([\w.<>]+)', kotlin))
required = dict(re.findall(r'\{\s*"([A-Za-z]+)"\s*,\s*"([^"]+)"\s*\}', cpp))
assert len(required) == 18, f"Expected 18 JNI fields, got {len(required)}"
assert len(fields) == len(required), (fields, required)
for name, descriptor in required.items():
    assert name in fields, f"Missing Kotlin @JvmField: {name}"
    assert sig.get(fields[name]) == descriptor, f"Wrong JNI signature for {name}: {fields[name]} != {descriptor}"
    assert name in cpp, f"JNI field unused: {name}"
for name, source in (
    ("joinAddress", "session?.address.orEmpty()"),
    ("joinPort", "session?.port ?: 0"),
    ("joinPassword", "session?.password.orEmpty()"),
    ("joinPlayer", "session?.player.orEmpty()"),
):
    assert re.search(r'\b' + name + r'\s*=\s*' + re.escape(source), activity), f"Server session JNI binding missing: {name}"
assert "env->ExceptionClear()" in cpp and "return JNI_FALSE" in cpp
print("BUNNY_NATIVE_JNI_CONFIG_CONTRACT=PASS")
