from pathlib import Path

root = Path(__file__).resolve().parents[2]
file = root / "app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.cpp"
content = file.read_text(encoding="utf-8")

required = (
    "int NewNPC(IEntitySource source",
    "worldIsAuthoritative(mode)",
    "runtime::modNpcTypeByName(request.modId, request.npcClass)",
    "runtime::modNpcBelongsTo(type, request.modId)",
    "r.playerActive",
    "r.playerLife",
    "r.npcActive",
    "r.npcType",
    "InvasionSpawnError::NoNpcSlot",
    "InvasionSpawnError::VerificationFailed",
    "runtime::sendData(23, slot)",
    "findNativeInvasionSpawnSite(",
    "InvasionSpawnError::NoSafeSpawnSite",
    "int x = site.x, y = site.y;",
)

for marker in required:
    assert marker in content, marker

policy = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.h").read_text(encoding="utf-8")
assert "request.worldReady" in policy
assert "NoSafeSpawnSite" in policy
assert "worldIsAuthoritative(mode)" in policy
assert "script/js/" not in content
assert "QuickJS" not in content
print("Native spawn contract: OK")
