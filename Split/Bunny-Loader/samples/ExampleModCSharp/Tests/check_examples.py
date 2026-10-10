from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EXAMPLE = ROOT / "samples" / "ExampleModCSharp"
FIXTURE = ROOT / "samples" / "SourceCSharpMod"

MIRRORS = {
    "Examples/Items/SourceWand.cs": "content/Content/Items/Weapons/SourceWand.cs",
    "Examples/Items/SourceStaff.cs": "content/Content/Items/Weapons/SourceStaff.cs",
    "Examples/Projectiles/SourceBolt.cs": "content/main.cs",
    "Examples/Projectiles/SourceSpark.cs": "content/Content/Projectiles/SourceSpark.cs",
    "Examples/Projectiles/AI/SourceBoltMotion.cs": "content/SourceBoltMotion.cs",
    "Examples/Projectiles/AI/SourceSparkMotion.cs": "content/SourceSparkMotion.cs",
}

sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text(encoding="utf-8")
assert "public abstract class BunnyWeapon" in sdk
assert "public abstract class BunnyProjectile" in sdk
assert "public abstract class BunnyBuff" in sdk
assert "public abstract class BunnyBuffPotion" in sdk
assert "public abstract class BunnyPlayer" in sdk
assert "public abstract class BunnyAccessory" in sdk
assert "public abstract class BunnyNPC" in sdk
for signature in (
    "public abstract void SetDefaults()",
    "public virtual int OnHitNPC(int damage)",
    "public virtual int OnKill(int damage)",
    "public virtual int OnKillDuration(int damage)",
):
    assert signature in sdk, f"SDK method missing: {signature}"

assert len(list((EXAMPLE / "Examples").rglob("*.cs"))) == len(MIRRORS)
for mirror, source in MIRRORS.items():
    example = EXAMPLE / mirror
    fixture = FIXTURE / source
    assert example.read_bytes() == fixture.read_bytes(), f"Example out of sync: {mirror}"
    assert "tModLoader" not in example.read_text(encoding="utf-8"), mirror

manifest = json.loads((FIXTURE / "manifest.json").read_text(encoding="utf-8"))
catalog = json.loads((EXAMPLE / "compatibility.json").read_text(encoding="utf-8"))
assert catalog["schemaVersion"] == 1
assert catalog["exampleModCSharpVersion"] == "0.1.6"
assert catalog["fixtureMod"] == manifest["id"] == "sourcecsharp"
assert catalog["fixtureModVersion"] == manifest["version"] == "0.1.5"
assert catalog["supportedModId"] == manifest["uid"]
assert catalog["verifiedLoaderVersion"] == "0.1.10.16.34"

supported_states = {"device_verified", "ci_verified", "experimental", "not_implemented"}
features = catalog["features"]
assert len(features) >= 12
ids = set()
example_classes = {"SourceWand", "SourceStaff", "SourceBolt", "SourceSpark",
                   "SourceBoltMotion", "SourceSparkMotion",
                   "ExampleDefenseBuff", "ExampleBuffPotion",
                   "ExampleFocusAccessory", "ExampleFocusPlayer", "ExampleSlimeNPC",
                   "BunnyLiveEntry", "BunnyLiveNpc"}
for feature in features:
    assert feature["id"] not in ids, "Duplicate feature ID"
    ids.add(feature["id"])
    assert feature["status"] in supported_states
    assert all(name in example_classes for name in feature["examples"])
    if feature["status"] == "not_implemented":
        assert not feature["examples"], "Unimplemented feature incorrectly has an example"

get_started = (EXAMPLE / "Docs" / "GettingStarted.md").read_text(encoding="utf-8")
for keyword in ("sourceCompile()", "sourceApplyCompiled", "sourceEnableProjectileAI",
                "sourceHitStatus", "sourceKillStatus"):
    assert keyword in get_started, f"Missing tutorial step: {keyword}"

doc = (EXAMPLE / "Docs" / "SupportedAPI.md").read_text(encoding="utf-8")
for name in ("BunnyWeapon", "BunnyProjectile", "OnHitNPC", "OnKill", "TurnAtTick"):
    assert name in doc, f"Missing supported API documentation: {name}"

playable = EXAMPLE / "Playable"
manifest2 = json.loads((playable / "manifest.json").read_text(encoding="utf-8"))
assert manifest2["uid"] != manifest["uid"]
assert manifest2["id"] == "examplemodcsharp"
assert manifest2["entry"] == "main.cs"
assert manifest2["version"] == "0.1.7"
playable_files = list((playable / "content").rglob("*.cs"))
assert len(playable_files) == 11
source_text = "\\n".join(p.read_text(encoding="utf-8") for p in playable_files)
for expected in ("ExampleWand", "ExampleBolt", "ExampleBoltMotion", "BunnyWeapon",
                 "BunnyProjectile", "SetDefaults()", "TurnAtTick(int tick)",
                 "BunnyBuff", "BunnyBuffPotion", "BunnyPlayer", "BunnyAccessory", "BunnyNPC",
                 "AnimationType = 1", "LifeMax = 25",
                 "static class BunnyLiveEntry", "static class BunnyLiveEvents", "static int Tick(int tick, int state)",
                 "static int OnHitNPC(int call, int damage)",
                 "static int OnKill(int call, int damage)",
                 "static class BunnyLiveNpc", "static int AI(int playerDeltaX, int npcLife)",
                 "DefenseBonus = 10", "DefenseBonus = 7", "ResetEffects()",
                 "UpdateEquips(bool accessoryEquipped)",
                 "Buff = nameof(ExampleDefenseBuff)",
         "OnHitNPC(int damage)", "OnKill(int damage)", "OnKillDuration(int damage)"):
    assert expected in source_text, expected
assert "SourceCSharpMod" not in source_text
print("EXAMPLEMOD_CSHARP_INDEPENDENT_SOURCE=PASS")
print(f"EXAMPLEMOD_CSHARP_STATIC_CHECK=PASS examples={len(MIRRORS)} features={len(features)}")
