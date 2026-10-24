#!/usr/bin/env python3
import os
from pathlib import Path
import signal
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
cpp = root / 'app/src/main/cpp'
with tempfile.TemporaryDirectory(prefix='bunny-signal-test-') as tmp:
    d=Path(tmp)
    (d/'probe.dll').write_bytes(b'mock')
    flags=['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror']
    subprocess.run(flags+['-DEXPECT_MONO_NATIVE_SIGNAL_POLICY','-shared','-fPIC',str(root/'tests/managed_signal_mock.cpp'),'-o',str(d/'libmonosgen-2.0.so')],check=True)
    subprocess.run(flags+['-DBUNNY_TEST_ANDROID_SIGNALS','-I'+str(cpp),str(cpp/'experiments/csharp/ManagedRuntime.cpp'),str(root/'tests/managed_signal_ownership.cpp'),'-pthread','-ldl','-o',str(d/'test')],check=True)
    env=dict(os.environ,LD_LIBRARY_PATH=str(d))
    for sig in [0,signal.SIGPWR,signal.SIGXCPU,signal.SIGTTIN,-signal.SIGPWR,999]:
        for mode in ['DIRECT','PINNED_MONO','DOMAIN_ONLY','ATTACH_ONLY','DOMAIN_THREAD_AUDIT','LIBRARY_ONLY','PATH_ONLY','SYSTEM_NATIVE_ONLY']:
            subprocess.run([str(d/'test'),str(d/'probe.dll'),str(int(sig)),mode],env=env,check=True,timeout=5)
