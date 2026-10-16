from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
cmake = (root / "app/src/main/cpp/CMakeLists.txt").read_text()
block = re.search(r"set\(BL_MOD_CLASSES_PARTS\s+(.*?)\s*\)", cmake, re.S)
assert block, "BL_MOD_CLASSES_PARTS not found"
parts = block.group(1).split()
assert "Loaders/ModMusic" in parts
moddir = root / "app/src/main/cpp/script/js/mod"
bundle = "(() => {\n'use strict';\n"
for name in parts:
    source = moddir / (name + ".js")
    assert source.is_file(), f"Missing bundled ModClasses source: {source}"
    bundle += f"\n// ---- mod/{name}.js ----\n" + source.read_text() + "\n"
bundle += "})();\n"
node = shutil.which("node")
assert node, "Node.js required to syntax-check the exact bundled ModClasses.js"
check = subprocess.run([node, "--check"], input=bundle, text=True, capture_output=True)
if check.returncode:
    sys.stderr.write(check.stderr)
    raise SystemExit("BUNNY_MOD_CLASSES_JS_SYNTAX=FAIL")
print(f"BUNNY_MOD_CLASSES_JS_SYNTAX=PASS ({len(parts)} files)")
