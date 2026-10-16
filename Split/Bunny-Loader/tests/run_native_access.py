#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
cpp=root/'app/src/main/cpp'
flags=['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread','-I'+str(cpp)]
with tempfile.TemporaryDirectory(prefix='bunny-access-') as temp:
    target=str(Path(temp)/'native-access')
    subprocess.run(flags+['-DBUNNY_NATIVE_ACCESS_HOST_TEST','-Wno-missing-field-initializers',str(root/'tests/native_access/native.cpp'),str(cpp/'experiments/csharp/NativeAccess.cpp'),'-o',target],check=True)
    subprocess.run([target],check=True,timeout=30)
    subprocess.run(flags+['-fno-exceptions','-fno-rtti','-fsyntax-only',str(cpp/'experiments/csharp/NativeAccess.cpp')],check=True)
