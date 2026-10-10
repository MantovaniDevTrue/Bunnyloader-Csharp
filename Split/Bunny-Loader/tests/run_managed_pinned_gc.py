#!/usr/bin/env python3
"""Native state-machine regression test; does not simulate a real Mono GC."""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
cpp = root / 'app/src/main/cpp'
with tempfile.TemporaryDirectory(prefix='bunny-pinned-gc-') as temp:
    out = Path(temp)
    probe = out / 'probe.dll'
    probe.write_bytes(b'mock probe')
    common = ['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror']
    subprocess.run(common + ['-I'+str(cpp), str(cpp/'experiments/csharp/ManagedRuntime.cpp'),
        str(root/'tests/managed_pinned_gc_smoke.cpp'), '-ldl', '-pthread', '-o', str(out/'test')], check=True)
    env = dict(os.environ, LD_LIBRARY_PATH=str(out))
    for omit in ['', 'OMIT_ENTER_GC', 'OMIT_EXIT_GC']:
        subprocess.run(common + (['-D'+omit] if omit else []) + ['-shared', '-fPIC',
            str(root/'tests/managed_pinned_gc_mock.cpp'), '-o', str(out/'libmonosgen-2.0.so')], check=True)
        stages = ['LIBRARY_ONLY', 'PATH_ONLY', 'SYSTEM_NATIVE_ONLY',
            'DOMAIN_ONLY', 'ATTACH_ONLY', 'DOMAIN_THREAD_AUDIT', 'PINNED_MONO']
        for stage in stages:
            subprocess.run([str(out/'test'), str(probe), stage], env=env, check=True, timeout=10)
