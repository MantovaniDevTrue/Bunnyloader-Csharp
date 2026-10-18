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
    for attempt in range(40):
        result = subprocess.run([target], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=30)
        if result.returncode:
            print(f"NATIVE_ACCESS_FAILED_ITERATION={attempt + 1}\n{result.stdout}", flush=True)
            raise subprocess.CalledProcessError(result.returncode, [target])
    print("NATIVE_ACCESS_40_WAKE_RPC_AND_LIFETIME_EXECUTIONS_PASS")
    subprocess.run(flags+['-fno-exceptions','-fno-rtti','-fsyntax-only',str(cpp/'experiments/csharp/NativeAccess.cpp')],check=True)
