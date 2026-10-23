#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
wire=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="bunny-native-equip-") as temporary:
    executable=Path(temporary)/"native-equip"
    subprocess.run(["g++","-std=c++20","-O2","-Wall","-Wextra","-Werror","-Wno-deprecated-declarations","-pthread",
                    "-ffunction-sections","-fdata-sections","-DBL_HAVE_QUICKJS=0",
                    "-I"+str(root/"tests/basic_items/include"),"-I"+str(root/"app/src/main/cpp"),
                    str(root/"tests/basic_items/native_equip_effects_test.cpp"),
                    str(root/"app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp"),
                    str(root/"app/src/main/cpp/experiments/csharp/NativeArmorPolicies.cpp"),
                    str(root/"app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp"),
                    str(root/"app/src/main/cpp/content/common/TypeTables.cpp"),
                    str(root/"tests/basic_items/native_policy_support.cpp"),
                    "-Wl,--gc-sections","-o",str(executable)],check=True)
    for mode in [[],["--type"],["--static"],["--bounds"],["--hook"]]:
        subprocess.run([str(executable),str(wire)]+mode,check=True,timeout=20)
    if len(sys.argv)>2:
        subprocess.run([str(executable),str(Path(sys.argv[2]).resolve()),"--sample"],check=True,timeout=20)
    subprocess.run([str(executable),str(wire),"--fallback"],check=True,timeout=20)
