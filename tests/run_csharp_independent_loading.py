from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="bunny-independent-loading-") as temporary:
    out=Path(temporary);exe=out/'loading'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-DBL_HAVE_QUICKJS=0',
        '-I'+str(root/'tests/mod_loader/include'),'-I'+str(root/'tests/basic_items/include'),'-I'+str(root/'app/src/main/cpp'),
        str(root/'tests/mod_loader/independent_loading.cpp'),str(root/'app/src/main/cpp/mods/ModLoader.cpp'),
        str(root/'app/src/main/cpp/mods/LoadOrder.cpp'),str(root/'app/src/main/cpp/mods/ManifestText.cpp'),'-o',str(exe)],check=True)
    for mode in ['--only-csharp','--mixed','--js-fails','--builtin']:
        subprocess.run([str(exe),str(out/'mods'),mode],check=True,timeout=20)
