#!/usr/bin/env python3
"""Byte-identical Git blobs from tModLoader 1.4.5, fcafebc0b792. No network required."""
from pathlib import Path
import hashlib
root=Path(__file__).resolve().parent/'upstream'
for name,sha in {
    'BiomeConversionLoader.cs':'f0a0715e4dbd608c814ec80d6ec7e07dd5584c0b',
    'ModBiomeConversion.cs':'b0b7d317cd47126595b865da0e0b817b1f157632',
}.items():
    data=(root/name).read_bytes()
    actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    assert actual==sha,(name,actual,sha)
print('BIOME_CONVERSION_PINNED_UPSTREAM_PASS files=2')
