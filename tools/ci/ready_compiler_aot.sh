#!/usr/bin/env bash
set -euo pipefail
PACKAGE=microsoft.netcore.app.runtime.aot.linux-x64.cross.android-arm64
mkdir -p "$RUNNER_TEMP/ready-aot"
curl -fLsS --retry 3 "https://api.nuget.org/v3-flatcontainer/$PACKAGE/8.0.23/$PACKAGE.8.0.23.nupkg" -o "$RUNNER_TEMP/ready-aot/cross.nupkg"
curl -fLsS --compressed --retry 3 "https://api.nuget.org/v3/registration5-gz-semver2/$PACKAGE/8.0.23.json" -o "$RUNNER_TEMP/ready-aot/registration.json"
CATALOG=$(python3 -c 'import json,sys; u=json.load(open(sys.argv[1]))["catalogEntry"]; assert u.startswith("https://api.nuget.org/v3/catalog0/data/"); print(u)' "$RUNNER_TEMP/ready-aot/registration.json")
curl -fLsS --compressed --retry 3 "$CATALOG" -o "$RUNNER_TEMP/ready-aot/catalog.json"
python3 - "$RUNNER_TEMP/ready-aot" <<'PY'
from pathlib import Path
import sys,json,hashlib,base64,zipfile
p=Path(sys.argv[1]); m=json.loads((p/'catalog.json').read_text())
assert m['packageHashAlgorithm']=='SHA512'
assert hashlib.sha512((p/'cross.nupkg').read_bytes()).digest()==base64.b64decode(m['packageHash'],validate=True)
with zipfile.ZipFile(p/'cross.nupkg') as z:
    assert all('..' not in Path(n).parts and not n.startswith('/') for n in z.namelist())
    z.extractall(p/'cross')
for tool in (p/'cross').rglob('mono-aot-cross'): tool.chmod(0o755)
PY
CROSS=$(find "$RUNNER_TEMP/ready-aot/cross" -type f -name mono-aot-cross -print -quit)
python3 tools/build_compiler_aot.py --framework app/src/main/assets/managed --cross "$CROSS" \
 --clang "$ANDROID_HOME/ndk/30.0.16248370/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
 --mono app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so \
 --output app/src/main/jniLibs/arm64-v8a/libbunnycompileraot.so --work build/compiler-aot
