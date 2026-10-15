from pathlib import Path
import importlib.util
import json
import tempfile
import uuid
import zipfile
import io

root = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


generator = load("create_csharp_mod", root / "tools/create_csharp_mod.py")
pack = load("bunny_pack", root / "tools/mods/pack.py")
with tempfile.TemporaryDirectory(prefix="bunny-starter-test-") as temporary:
    destination = Path(temporary) / "My Mod"
    generator.create("my_mod", destination, "Meu Mod")
    playable = destination / "Playable"
    manifest = json.loads((playable / "manifest.json").read_text())
    assert manifest["id"] == "my_mod" and manifest["name"] == "Meu Mod"
    uuid.UUID(manifest["uid"])
    assert "namespace MyMod;" in (playable / "content/main.cs").read_text()
    project = (destination / "Mod.csproj").read_text()
    assert "BunnySourceSdk.csproj" in project and "Playable/content/**/*.cs" in project
    first = pack.pack_bytes(playable)
    assert first == pack.pack_bytes(playable)
    with zipfile.ZipFile(io.BytesIO(first)) as archive:
        names = archive.namelist()
        assert "manifest.json" in names and "content/main.cs" in names
        assert "content/Content/Items/StarterMaterial.png" in names
        assert len([name for name in names if name.endswith(".cs")]) == 2
        assert not any(name.endswith((".dll", ".so", ".js", ".csproj")) for name in names)
    original = (playable / "manifest.json").read_bytes()
    try:
        generator.create("my_mod", destination, "Overwrite")
        raise AssertionError("Existing destination overwritten")
    except FileExistsError:
        pass
    assert (playable / "manifest.json").read_bytes() == original
    for invalid in ("../bad", "Invalid", "a", "with-dash", "with space"):
        try:
            generator.create(invalid, Path(temporary) / "Invalid", "Test")
            raise AssertionError("Invalid ID accepted: " + invalid)
        except ValueError:
            pass
    second = Path(temporary) / "Second"
    generator.create("second_mod", second, "Second")
    assert json.loads((second / "Playable/manifest.json").read_text())["uid"] != manifest["uid"]
print("BUNNY_CSHARP_STARTER_GENERATION_AND_PACKAGE=PASS")
