#!/usr/bin/env python3
"""Verify differential-test sources against exact Git blob hashes; no network required."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parent / "upstream"
manifest = json.loads((root / "sources.json").read_text())
for source in manifest["files"]:
    data = (root / source["name"]).read_bytes()
    header, data = data.split(b"\n", 1)
    assert header.startswith(b"#nullable disable"), source["name"]
    blob = b"blob " + str(len(data)).encode() + b"\0" + data
    actual = hashlib.sha1(blob).hexdigest()
    assert actual == source["blobSha"], f'{source["name"]}: {actual} != {source["blobSha"]}'
print(f'TMODLOADER_PINNED_SOURCES_PASS files={len(manifest["files"])} commit={manifest["commit"]}')
