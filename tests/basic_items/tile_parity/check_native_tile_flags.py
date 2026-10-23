#!/usr/bin/env python3
"""Confere a identidade das flags C# ate o contrato de registro IL2CPP.

Inclui o mapeamento de versao ampla do TileLoader original, sem confundir
Lighted (4096) com ChecksForMerge (16384).
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyTile.cs").read_text()
loader = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
writer = (ROOT / "experiments/csharp/BunnySourceCompiler/TileWriter.cs").read_text()
native = (ROOT / "app/src/main/cpp/experiments/csharp/NativeTileFlags.h").read_text()
content = (ROOT / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()

required = {
    "Solid": (512, '{512,"Main","tileSolid"}'),
    "BlockLight": (1024, '{1024,"Main","tileBlockLight"}'),
    "MergeDirt": (2048, '{2048,"Main","tileMergeDirt"}'),
    "Lighted": (4096, '{4096,"Main","tileLighted"}'),
    "ChecksForMerge": (16384, '{16384,"Sets","ChecksForMerge"}'),
    "Cut": (32768, '{32768,"Main","tileCut"}'),
}
for name, (value, registration) in required.items():
    assert re.search(r"\b" + name + r"\s*=\s*" + str(value) + r"\b", sdk), name
    assert registration in native, registration
assert "supportedMask = 65535" in native
assert "tile.flags > 65535" in loader
assert "(flags & ~65535) != 0" in writer
assert "tile_flags::apply(type, entry.flags" in content
print("EXAMPLE_TILE_NATIVE_FLAG_WIRING=PASS")
