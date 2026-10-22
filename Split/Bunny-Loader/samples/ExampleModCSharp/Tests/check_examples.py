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

# Os espelhos da fixture devem permanecer byte a byte idênticos. ModHair é
# um exemplo adicional, intencionalmente fora do pacote Playable até que
# o ciclo nativo (UI/save/rede) esteja concluído.
experimental = {"Examples/Hairs/ExampleHair.cs"}
actual_examples = {
    str(path.relative_to(EXAMPLE)).replace("\\\\", "/")
    for path in (EXAMPLE / "Examples").rglob("*.cs")
}
assert actual_examples == set(MIRRORS) | experimental, (
    f"Exemplos ausentes/extras: {actual_examples ^ (set(MIRRORS) | experimental)}"
)
hair_example = (EXAMPLE / "Examples/Hairs/ExampleHair.cs").read_text(encoding="utf-8")
for key in ("class ExampleHair : ModHair", "GetUnlockConditions()",
            "HairManifestSession.Prepare", "AvailableDuringCharacterCreation",
            "RandomizedCharacterCreationGender"):
    assert key in hair_example, f"Hair example missing contract: {key}"
assert not list((EXAMPLE / "Playable").rglob("ExampleHair.cs")), (
    "HairLoader incompleto nunca pode entrar no pacote Playable"
)
for mirror, source in MIRRORS.items():
    example = EXAMPLE / mirror
    fixture = FIXTURE / source
    assert example.read_bytes() == fixture.read_bytes(), f"Example out of sync: {mirror}"
    assert "tModLoader" not in example.read_text(encoding="utf-8"), mirror

manifest = json.loads((FIXTURE / "manifest.json").read_text(encoding="utf-8"))
catalog = json.loads((EXAMPLE / "compatibility.json").read_text(encoding="utf-8"))
assert catalog["schemaVersion"] == 1
assert catalog["exampleModCSharpVersion"] == "0.1.40"
assert catalog["fixtureMod"] == manifest["id"] == "sourcecsharp"
assert catalog["fixtureModVersion"] == manifest["version"] == "0.1.5"
assert catalog["supportedModId"] == manifest["uid"]
assert catalog["verifiedLoaderVersion"] == "0.1.10.16.34"

supported_states = {"device_verified", "ci_verified", "experimental", "not_implemented"}
features = catalog["features"]
assert len(features) >= 12
ids = set()
example_classes = {"ExampleItem", "ExamplePickaxe", "ExampleHamaxe", "SourceWand", "SourceStaff", "SourceBolt", "SourceSpark",
                   "SourceBoltMotion", "SourceSparkMotion",
                   "ExampleDefenseBuff", "ExampleBuffPotion",
                   "ExampleFocusAccessory", "ExampleFocusPlayer", "ExampleSwiftAccessory", "ExampleSwiftPlayer",
                   "ExampleWelcomePlayer", "ExampleMomentumAccessory", "ExampleMomentumPlayer",
                   "ExampleShadowAccessory", "ExampleShadowPlayer",
                   "ExampleVitalAccessory", "ExampleVitalPlayer", "ExampleArcaneAccessory", "ExampleArcanePlayer", "ExampleSlimeNPC",
                   "BunnyLiveEntry", "BunnyLiveNpc", "ExampleWaveWand",
                   "ExampleWaveBolt", "ExampleWaveBoltMotion", "ExampleWaveBoltLiveEvents",
                   "ExampleMightyPrefix", "ExampleGuardedPrefix",
                   "ExampleGuardianShieldAccessory", "ExampleGuardianShieldPlayer",
                   "ExampleTrailblazerBootsAccessory", "ExampleTrailblazerBootsPlayer",
                   "ExampleMysticWardAccessory", "ExampleMysticWardPlayer",
                   "ExampleWorkbench", "ExampleHelmet", "ExampleBreastplate", "ExampleLeggings", "ExampleHair"}
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
assert manifest2["version"] == "0.1.40"
playable_files = list((playable / "content").rglob("*.cs"))
assert 0 < len(playable_files) <= 512
source_text = "\\n".join(p.read_text(encoding="utf-8") for p in playable_files)
for expected in ("ExampleWand", "ExampleBolt", "ExampleBoltMotion",
                 "ExampleWaveWand", "ExampleWaveBolt", "ExampleWaveBoltMotion",
                 "ExampleWaveBoltLiveEvents", "BunnyWeapon",
                 "BunnyProjectile", "SetDefaults()", "TurnAtTick(int tick)",
                 "BunnyBuff", "BunnyBuffPotion", "BunnyPlayer", "BunnyAccessory", "BunnyNPC",
                 "AnimationType = 1", "LifeMax = 25",
                 "static class BunnyLiveEntry", "static class BunnyLiveEvents", "static int Tick(int tick, int state)",
                 "static int OnHitNPC(int call, int damage)",
                 "static int OnKill(int call, int damage)",
                 "static class BunnyLiveNpc", "static int AI(int playerDeltaX, int npcLife)",
                 "DefenseBonus = 10", "DefenseBonus = 7", "MoveSpeedPercent = 8", "ResetEffects()",
                 "UpdateEquips(bool accessoryEquipped)", "OnEnterWorld()", "OnRespawn()", "UpdateMovement(bool accessoryEquipped)",
                 "FrameEffects(bool accessoryEquipped)", "PreUpdate()", "PostUpdate()", "UpdateDead()",
                 "UpdateLifeRegen(bool accessoryEquipped)", "UpdateManaRegen(bool accessoryEquipped)", "FreeDodge(bool accessoryEquipped, BunnyHurtModifiers modifiers)", "ModifyHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtModifiers modifiers)", "ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers)", "OnHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtInfo info)", "OnHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtInfo info)", "Tooltip =", "return accessoryEquipped ? 6 : 0;",
                 "Buff = nameof(ExampleDefenseBuff)", "BunnyPrefix",
                 "ExampleMightyPrefix", "ExampleGuardedPrefix", "ApplyAccessoryEffects",
                 "ExampleGuardianShieldAccessory", "ExampleGuardianShieldPlayer",
                 "ExampleTrailblazerBootsAccessory", "ExampleTrailblazerBootsPlayer",
                 "ExampleMysticWardAccessory", "ExampleMysticWardPlayer",
         "OnHitNPC(int damage)", "OnKill(int damage)", "OnKillDuration(int damage)"):
    assert expected in source_text, expected
assert (playable / "content/Content/Tiles/Furniture/ExampleWorkbench.cs").exists()
assert (playable / "content/Content/Items/Placeable/Furniture/ExampleWorkbench.cs").exists()
for armor_name in ("ExampleHelmet", "ExampleBreastplate", "ExampleLeggings"):
    armor_text = (playable / "content/Content/Items/Armor" / (armor_name + ".cs")).read_text(encoding="utf-8")
    assert 'AddTile("ExampleWorkbench")' in armor_text
    assert 'TooltipTranslations["pt-BR"]' in armor_text
wave_weapon = (playable / "content/Content/Items/Weapons/ExampleWaveWand.cs").read_text(encoding="utf-8")
wave_projectile = (playable / "content/Content/Projectiles/ExampleWaveBolt.cs").read_text(encoding="utf-8")
wave_motion = (playable / "content/ExampleWaveBoltMotion.cs").read_text(encoding="utf-8")
assert "Shoot = nameof(ExampleWaveBolt);" in wave_weapon
assert "Texture = \"Content/Items/Weapons/SourceWand.png\";" in wave_weapon
assert "Texture = \"Content/Projectiles/SourceBolt.png\";" in wave_projectile
assert "AiStyle = 1;" in wave_projectile
assert "return MathF.Sin(tick * 0.20f) * 0.018f;" in wave_motion
assert "public override void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage)" in wave_projectile
assert "public override void OnKill(ref BunnyProjectileAiState state)" in wave_projectile
assert "public override int OnHitNPC(int damage)" not in wave_projectile
wave_events = (playable / "content/Common/Live/ExampleWaveBoltLiveEvents.cs").read_text(encoding="utf-8")
assert "static class ExampleWaveBoltLiveEvents" in wave_events
assert "static int OnHitNPC(int call, int damage)" in wave_events
assert "static int OnKill(int call, int damage)" in wave_events
assert "static int OnHitNPCV2(int call, int damage, int npcIndex)" in wave_events
assert "static int OnHitNPCV3(int call, int damage, int npcIndex, int npcType, int npcLife, float posX, float posY)" in wave_events
assert "static int OnHitNPCV4(int call, int damage, int npcIndex, int npcType," in wave_events
assert "playerHealthPercent" in wave_events
assert "return 1000000 + buffId * 1000 + durationTicks;" in wave_events
assert "return 2000000 + direction * 100 + strengthTenths;" in wave_events
assert "SourceCSharpMod" not in source_text
print("EXAMPLEMOD_CSHARP_INDEPENDENT_SOURCE=PASS")
print(f"EXAMPLEMOD_CSHARP_STATIC_CHECK=PASS examples={len(MIRRORS)} features={len(features)}")



