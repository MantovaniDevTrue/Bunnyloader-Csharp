#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
cpp = root / 'app/src/main/cpp'
flags = ['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread', '-I'+str(cpp)]
with tempfile.TemporaryDirectory(prefix='bunny-lifecycle-') as tmp:
    out = Path(tmp)
    subprocess.run(flags+[str(cpp/'experiments/csharp/SourceParser.cpp'), str(root/'tests/native_source_identity.cpp'), '-o', str(out/'source-identity')], check=True)
    subprocess.run([str(out/'source-identity')], check=True, timeout=20)
    print('source_identity=PASS', flush=True)
    for name in ['managed_live_queue_smoke', 'managed_live_ingress_smoke', 'managed_multi_live_ingress_smoke', 'managed_live_lifecycle_smoke', 'managed_live_admission', 'managed_memory_snapshot_smoke']:
        subprocess.run(flags+[str(root/'tests'/f'{name}.cpp'), '-o', str(out/name)], check=True)
        subprocess.run([str(out/name)], check=True, timeout=20)
        print(name+'=PASS', flush=True)
    subprocess.run(flags+['-shared', '-fPIC', str(root/'tests/managed_fault_mock.cpp'), '-o', str(out/'libmonosgen-2.0.so')], check=True)
    runtime_object = out/'ManagedRuntime.o'
    subprocess.run(flags+['-c', str(cpp/'experiments/csharp/ManagedRuntime.cpp'), '-o', str(runtime_object)], check=True)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_fault_isolation.cpp'), '-ldl', '-o', str(out/'fault')], check=True)
    (out/'probe.dll').write_bytes(b'mock')
    subprocess.run([str(out/'fault'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
    subprocess.run(flags+['-shared', '-fPIC', str(root/'tests/managed_pinned_gc_mock.cpp'), '-o', str(out/'libmonosgen-2.0.so')], check=True)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_hit_v2_mock.cpp'), '-ldl', '-o', str(out/'hit-v2')], check=True)
    subprocess.run([str(out/'hit-v2'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_hit_v3_mock.cpp'), '-ldl', '-o', str(out/'hit-v3')], check=True)
    subprocess.run([str(out/'hit-v3'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_hit_v4_mock.cpp'), '-ldl', '-o', str(out/'hit-v4')], check=True)
    subprocess.run([str(out/'hit-v4'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_hit_effects_mock.cpp'), '-ldl', '-o', str(out/'hit-effects')], check=True)
    subprocess.run([str(out/'hit-effects'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
    subprocess.run(flags+[str(runtime_object), str(root/'tests/managed_hit_impulse_mock.cpp'), '-ldl', '-o', str(out/'hit-impulse')], check=True)
    subprocess.run([str(out/'hit-impulse'), str(out/'probe.dll')], env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
