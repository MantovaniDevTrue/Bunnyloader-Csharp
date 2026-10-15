#!/usr/bin/env python3
"""Validate the actual packaged sources, manifest and PNG assets for Android testing."""
import hashlib
import json
import struct
import sys
import zipfile

EXPECTED_HASHES = {'ProbePlatformB.png': '8958ff1414fc3577a454f744b4bdeb251a04294c7103b925bb65b437bc49f581', 'ProbePlatformA.png': '8958ff1414fc3577a454f744b4bdeb251a04294c7103b925bb65b437bc49f581', 'ProbeWorkbench.png': '02a96b61fc36d9a181c7e6b6c53fc891aca45160c47c2969bdf3511a617150fe'}
with zipfile.ZipFile(sys.argv[1]) as package:
    names = set(package.namelist())
    manifest = json.loads(package.read("manifest.json"))
    assert manifest["uid"] == "943c352f-5b07-4b3a-b565-620fc7ea63d3"
    assert manifest["id"] == "nativeplacementprobe" and manifest["version"] == "0.1.0"
    assert manifest["entry"] == "main.cs" and not manifest["dependencies"]
    assert "content/main.cs" in names and "content/COMO_TESTAR.md" in names
    assert len([n for n in names if n.endswith(".cs")]) == 4
    assert not any(n.endswith((".js", ".dll", ".so")) for n in names)
    for name, digest in EXPECTED_HASHES.items():
        png = package.read("content/Content/Items/" + name)
        assert png[:8] == b"\x89PNG\r\n\x1a\n"
        width, height = struct.unpack(">II", png[16:24])
        assert 0 < width <= 256 and 0 < height <= 256
        assert hashlib.sha256(png).hexdigest() == digest
        assert "content/Content/Items/" + name.replace(".png", ".cs") in names
print("NATIVE_PLACEMENT_ANDROID_PROBE_SOURCE_AND_ASSET_PACKAGE_PASS")
