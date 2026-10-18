#!/usr/bin/env python3
from pathlib import Path
import importlib.util
import shutil
import sys
import tempfile
import zipfile
import io

ROOT=Path(__file__).resolve().parents[1]

def package_bytes():
    spec=importlib.util.spec_from_file_location('bunny_pack',ROOT/'tools/mods/pack.py')
    pack=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pack)
    with tempfile.TemporaryDirectory(prefix='native-access-probe-') as temp:
        folder=Path(temp)/'mod'
        shutil.copytree(ROOT/'samples/NativeAccessProbe/Playable',folder)
        source=ROOT/'samples/ExampleModCSharp/Playable/content/Common/Hooks/ExampleNativeAccess.cs'
        shutil.copyfile(source,folder/'content/ExampleNativeAccess.cs')
        data=pack.pack_bytes(folder)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        assert archive.read('content/ExampleNativeAccess.cs')==source.read_bytes()
        assert 'content/ProbePlayer.cs' in archive.namelist()
        assert not any(name.endswith(('.js','.dll','.so')) for name in archive.namelist())
    return data

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('package_native_access_probe.py OUTPUT.bmod')
    output=Path(sys.argv[1])
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(package_bytes())
    print('NATIVE_ACCESS_CSHARP_PROBE_PACKAGE=PASS')
