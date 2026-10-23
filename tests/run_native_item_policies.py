from pathlib import Path
import subprocess,sys,tempfile
# The project still uses shared_ptr atomic free functions, valid in C++20 but
# deprecated by GCC 14. Keep other warnings as errors, as in the other runners.
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="bunny-native-items-") as temporary:
    executable=Path(temporary)/"items"
    subprocess.run(["g++","-std=c++20","-O2","-Wall","-Wextra","-Werror","-Wno-deprecated-declarations","-pthread","-ffunction-sections","-fdata-sections","-DBL_HAVE_QUICKJS=0",
        "-I"+str(root/"tests/basic_items/include"),"-I"+str(root/"app/src/main/cpp"),str(root/"tests/basic_items/native_item_policies_test.cpp"),
        str(root/"app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp"),str(root/"app/src/main/cpp/experiments/csharp/NativeArmorPolicies.cpp"),
        str(root/"app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp"),str(root/"app/src/main/cpp/experiments/csharp/NativeVisualRenderer.cpp"),str(root/"app/src/main/cpp/content/common/TypeTables.cpp"),
        str(root/"tests/basic_items/native_policy_support.cpp"),"-Wl,--gc-sections","-o",str(executable)],check=True)
    for mode in [[],["--boxed"],["--type"],["--bounds"],["--static"],["--screen-property"],["--android-fields"],["--android-properties"],["--android-properties-boxed"],["--getter-exception"],["--count-overflow"],["--android-145"],["--android-145-boxed"],["--animation-ctor-exception"],["--framework-color"],["--color-argument-mismatch"],["--color-return-mismatch"],["--world-active-field"],["--world-active-static"],["--world-active-type"],["--world-active-pointer"]]:
        subprocess.run([str(executable),str(Path(sys.argv[1]).resolve()),str(Path(sys.argv[2]).resolve())]+mode,check=True,timeout=30)
