from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from base64 import b64decode
from hashlib import sha256
import sys

root = Path(__file__).resolve().parent
output = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "LightsAndShadowsCSharp-0.2.0.bmod"
with ZipFile(output, "w", ZIP_DEFLATED, compresslevel=8) as package:
    for path in sorted((root / "Playable").rglob("*")):
        if path.is_file():
            package.write(path, path.relative_to(root / "Playable").as_posix())
    for name in ("ColorTex.png", "ColorTex2.png", "ColorTex_Alt.png"):
        data = b64decode((root / "SourceAssets" / (name + ".b64")).read_text().strip(), validate=True)
        if not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise ValueError(name + ": invalid original PNG")
        package.writestr("content/Assets/" + name, data)
print(output)
print("sha256=" + sha256(output.read_bytes()).hexdigest())
