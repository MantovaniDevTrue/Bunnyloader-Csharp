"""Stage official .NET 8 compile-only metadata; runtime assemblies stay untouched."""
import argparse
import base64
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

NAMES = ("System.Runtime", "netstandard", "System.Collections", "System.Collections.Concurrent",
         "System.Linq", "System.Console", "System.Runtime.Extensions", "System.ObjectModel",
         "System.Threading", "System.Threading.Tasks", "System.Numerics.Vectors",
         "System.ComponentModel.Primitives")

def stage(package, sdk_reference, destination, checksum):
    actual = hashlib.sha512(package.read_bytes()).digest()
    metadata = json.loads(checksum.read_text())
    if metadata.get("packageHashAlgorithm") != "SHA512":
        raise ValueError("UnsupportedReferenceHashAlgorithm")
    if actual != base64.b64decode(metadata["packageHash"], validate=True):
        raise ValueError("ReferencePackSHA512Mismatch")
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(package) as archive:
        for name in NAMES:
            data = archive.read("ref/net8.0/" + name + ".dll")
            if not data.startswith(b"MZ"):
                raise ValueError("InvalidReferencePE: " + name)
            (destination / (name + ".dll")).write_bytes(data)
    data = sdk_reference.read_bytes()
    if not data.startswith(b"MZ"):
        raise ValueError("InvalidSdkReferencePE")
    shutil.copyfile(sdk_reference, destination / "BunnySourceSdk.dll")
    print("COMPILER_COMPACT_REFERENCES_STAGED files=13 bytes=" +
          str(sum(p.stat().st_size for p in destination.glob("*.dll"))))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("package", "sdk_reference", "destination", "checksum"):
        parser.add_argument(name, type=Path)
    stage(**vars(parser.parse_args()))
