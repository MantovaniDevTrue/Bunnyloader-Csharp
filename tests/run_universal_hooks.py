#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
cpp = root / 'app/src/main/cpp'
flags = ['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread', '-I'+str(cpp)]
with tempfile.TemporaryDirectory(prefix='bunny-hooks-') as temp:
    for test in ['mailbox', 'native']:
        sources = [str(root/'tests/universal_hooks'/f'{test}.cpp')]
        if test == 'native':
            sources += [str(cpp/'experiments/csharp/UniversalHooks.cpp')]
        target = str(Path(temp)/test)
        subprocess.run(flags+['-DBUNNY_UNIVERSAL_HOOK_HOST_TEST']+sources+['-o', target], check=True)
        subprocess.run([target], check=True, timeout=20)
    subprocess.run(flags+['-fsyntax-only', str(cpp/'experiments/csharp/ManagedRuntime.cpp')], check=True)
