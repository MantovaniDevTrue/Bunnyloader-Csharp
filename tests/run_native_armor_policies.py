from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="bunny-native-armor-") as temporary:
    executable=Path(temporary)/"armor"
    subprocess.run(["g++","-std=c++20","-O2","-Wall","-Wextra","-Werror","-Wno-deprecated-declarations","-pthread","-ffunction-sections","-fdata-sections","-DBL_HAVE_QUICKJS=0",
        "-I"+str(root/"tests/basic_items/include"),"-I"+str(root/"app/src/main/cpp"),str(root/"tests/basic_items/native_armor_policy_test.cpp"),
        str(root/"app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp"),str(root/"app/src/main/cpp/experiments/csharp/NativeArmorPolicies.cpp"),
                    str(root/"app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp"),
                    str(root/"app/src/main/cpp/content/common/TypeTables.cpp"),
                    str(root/"tests/basic_items/native_policy_support.cpp"),
        "-Wl,--gc-sections","-o",str(executable)],check=True)
    for mode in [[],["--struct"],["--boxed"],["--type"],["--bounds"],["--static"],["--set-retry"],["--add-retry"],["--result-retry"],["--add-side-effect"],["--lookup"],["--abi"],["--narrow-ids"]]:
        subprocess.run([str(executable),str(Path(sys.argv[1]).resolve())]+mode,check=True,timeout=30)

    if len(sys.argv)>2:
        for mode in ["--full-example","--full-example-boxed"]:
            subprocess.run([str(executable),str(Path(sys.argv[2]).resolve()),mode],check=True,timeout=30)
