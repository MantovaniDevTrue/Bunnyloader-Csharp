#!/usr/bin/env python3
"""Run production native engine/adapter, then Roslyn + real managed/native PInvoke. No APK."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--dotnet',default='dotnet')
parser.add_argument('--framework',default='net8.0')
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
tests=Path(__file__).resolve().parent
cpp=root/'app/src/main/cpp'
subprocess.run([sys.executable,str(tests/'check_upstream.py')],check=True)
with tempfile.TemporaryDirectory(prefix='bunny-biome-native-') as temp:
    out=Path(temp)
    flags=['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread','-I'+str(cpp)]
    def build(name,sources,more=()):
        subprocess.run(flags+list(more)+[str(p) for p in sources]+['-o',str(out/name)],check=True)
    build('engine',[tests/'engine_test.cpp'])
    subprocess.run([str(out/'engine')],check=True,timeout=15)
    build('adapter',[cpp/'experiments/csharp/NativeBiomeConversion.cpp',tests/'adapter_test.cpp'],['-I'+str(root/'tests/basic_items/include')])
    subprocess.run([str(out/'adapter')],check=True,timeout=15)
    subprocess.run([str(out/'adapter'),'bad-abi'],check=True,timeout=15)
    subprocess.run([str(out/'adapter'),'missing-framing'],check=True,timeout=15)
    build('parser',[cpp/'experiments/csharp/SourceParser.cpp',tests/'parser_test.cpp'])
    subprocess.run([str(out/'parser'),str(root/'samples/BiomeConversionCSharp/Playable/content/main.cs')],check=True,timeout=15)
    build('libbunny.so',[tests/'bridge_test.cpp'],['-shared','-fPIC'])
    subprocess.run([args.dotnet,'build',str(tests/'BiomeConversionTests.csproj'),'-c','Release','-p:BunnyTargetFramework='+args.framework],check=True)
    env=dict(os.environ,LD_LIBRARY_PATH=str(out)+(':'+os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else ''))
    subprocess.run([args.dotnet,str(tests/'bin/Release'/args.framework/'BiomeConversionTests.dll'),str(root/'samples/BiomeConversionCSharp/Playable')],env=env,check=True,timeout=60)
