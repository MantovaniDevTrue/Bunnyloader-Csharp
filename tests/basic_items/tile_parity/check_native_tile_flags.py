#!/usr/bin/env python3
"""Garante que cada flag adicionada no SDK chega ao registro nativo C++."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyTile.cs").read_text()
loader = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
native = (ROOT / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()

required = {
    "Solid": (512, 'apply(main, "tileSolid", 512);'),
    "BlockLight": (1024, 'apply(main, "tileBlockLight", 1024);'),
    "MergeDirt": (2048, 'apply(main, "tileMergeDirt", 2048);'),
    "ChecksForMerge": (4096, 'apply(sets, "ChecksForMerge", 4096);'),
}
for key, (flag, call) in required.items():
    assert re.search(r"\b" + key + r"\s*=\s*" + str(flag) + r"\b", sdk), key
    assert call in native, call
assert "tile.flags > 8191" in loader, "O parser nao aceita todas as flags"
assert 'if (!enabled) return true;' in native, "Mudou a politica existente de flags"
print("EXAMPLE_TILE_NATIVE_FLAG_WIRING=PASS")
