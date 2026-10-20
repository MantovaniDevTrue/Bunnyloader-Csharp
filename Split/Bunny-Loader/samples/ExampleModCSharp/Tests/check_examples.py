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
assert catalog["exampleModCSharpVersion"] == "0.1.58"
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
                   "ExampleDefenseBuff", "ExampleBuffPotion", "ExampleDefenseDebuff", "ExampleDebuffPotion",
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
                   "ExampleWorkbench", "ExampleHelmet", "ExampleBreastplate", "ExampleLeggings",
                   "ExampleMinion", "ExampleMinionBuff", "ExampleMinionItem"}
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
assert manifest2["version"] == "0.1.58"
playable_files = list((playable / "content").rglob("*.cs"))
assert 0 < len(playable_files) <= 512
hook_dir = playable / "content/Common/Hooks"
native_probe = (hook_dir / "ExampleNativeAccess.cs").read_text(encoding="utf-8")
pair_probe = (hook_dir / "ExampleIntPairHook.cs").read_text(encoding="utf-8")
assert "public const bool EnableAtLoad = false;" in native_probe
assert "if (!EnableAtLoad) return;" in native_probe
assert 'hooks.HookVoid("Terraria.Player", "ResetEffects"' in native_probe
assert 'ReadAndInvoke(' in native_probe
assert 'scope.HookInt32Pair("Terraria.Utilities.UnifiedRandom", "Next"' in pair_probe
assert "original.Invoke()" in pair_probe
assert "public static string Start()" in pair_probe and "public static string Stop()" in pair_probe
assert "public static bool OverrideSmallRange" in pair_probe
hook_examples = {
    "ExampleHookInt32Zero": ("HookInt32Zero", "Terraria.Utilities.UnifiedRandom", "Next", False),
    "ExampleHookVoidInt32": ("HookVoidInt32", "Terraria.WorldGen", "QuickFindHome", True),
    "ExampleHookVoidInt32Pair": ("HookVoidInt32Pair", "Terraria.WorldGen", "PlantCactus", True),
    "ExampleHookInt32Triple": ("HookInt32Triple", "Terraria.WorldGen", "countWires", True),
    "ExampleHookVoidInt32Triple": ("HookVoidInt32Triple", "Terraria.WorldGen", "ConvertTile", True),
}
for name, (signature, target, method, is_static) in hook_examples.items():
    example = (hook_dir / (name + ".cs")).read_text(encoding="utf-8")
    assert "public const bool EnableAtLoad = true;" in example, name
    assert "if (!EnableAtLoad) return;" in example, name
    assert "activeHook?.Dispose()" in example, name
    assert "SampleLimit = 2" in example, name
    assert "TrimEnd(" not in example, name
    assert "LastError.Substring(index + 7).Replace(" in example, name
    assert "original.Invoke()" in example, name
    if name == "ExampleHookVoidInt32Pair":
        assert '"HitSwitch"' in example and '"HitWireSingle"' in example, name
    if name == "ExampleHookInt32Triple":
        assert '"CalculateFrameFromMiscCounter"' in example and '"countWires"' in example, name
    if name == "ExampleHookVoidInt32Triple":
        assert '"Check1x1"' in example and '"CheckLogicTiles"' in example, name
    if name in ("ExampleHookVoidInt32", "ExampleHookVoidInt32Pair", "ExampleHookInt32Triple", "ExampleHookVoidInt32Triple"):
        assert f"hooks.{signature}(" in example, name
        assert "LastAttempt = Types[i]" in example, name
        assert "activeHook?.Dispose();" in example, name
    else:
        assert f'hooks.{signature}("{target}", "{method}"' in example, name
    if name in ("ExampleHookVoidInt32", "ExampleHookVoidInt32Pair", "ExampleHookInt32Triple", "ExampleHookVoidInt32Triple"):
        assert "isStatic: StaticFlags[i]" in example, name
    else:
        assert f'isStatic: {str(is_static).lower()}' in example, name

assert "ExampleNativeAccess.cs" not in str(EXAMPLE / "Experimental" / "NativeAccess" / "ExampleNativeAccess.cs") or not (EXAMPLE / "Experimental" / "NativeAccess" / "ExampleNativeAccess.cs").exists()
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
prefix_example = (playable / "content/Content/Prefixes/ExampleMightyPrefix.cs").read_text(encoding="utf-8")
assert 'rules.Allow("ExampleWand", 3);' in prefix_example
assert 'effects.DamageBonus = 3;' in prefix_example
assert 'effects.ValueBonus = 40;' in prefix_example
assert 'ExtraTooltip = "Bonus adicional do prefixo";' in prefix_example
assert 'AddTooltipLine("Dano extra do prefixo");' in prefix_example
assert 'AddTooltipLine("Valor adicional");' in prefix_example
assert 'AddTooltipItemLine("Dano atual: {damage}, mana: {mana}, pilha: {stack}");' in prefix_example
assert 'conditions.RequireDamageBetween(10, 25);' in prefix_example
assert 'conditions.RequireManaBetween(0, 8);' in prefix_example
assert 'conditions.RequireStackBetween(1, 10);' in prefix_example
assert 'conditions.OverrideRollWeightWhenStackBetween(3, 5, 6);' in prefix_example
guarded_prefix_example = (playable / "content/Content/Prefixes/ExampleGuardedPrefix.cs").read_text(encoding="utf-8")
assert 'conditions.OverrideRollWeightWhenManaBetween(0, 8, 4);' in guarded_prefix_example
assert 'public override bool CanRoll(BunnyPrefixItemSnapshot item)' in guarded_prefix_example
assert 'public override int RollChance(BunnyPrefixItemSnapshot item)' in guarded_prefix_example
assert 'public override bool AllStatChangesHaveEffectOn(BunnyPrefixItemSnapshot item)' in guarded_prefix_example
assert 'public override void Apply(BunnyPrefixItemSnapshot item, ref BunnyPrefixApplyStats effects)' in guarded_prefix_example
item_deny_example = (playable / "content/Content/Items/Weapons/ExampleWand.cs").read_text(encoding="utf-8")
assert 'rules.Deny(71);' in item_deny_example
assert 'rules.Deny(nameof(ExampleGuardedPrefix));' in item_deny_example
assert 'rules.DenyWhenStackBetween(72, 3, 5);' in item_deny_example
assert 'rules.DenyWhenDamageBetween(73, 10, 25);' in item_deny_example
assert 'rules.DenyWhenManaBetween(nameof(ExampleGuardedPrefix), 2, 8);' in item_deny_example
assert 'rules.SetPrefixChanceWhenStackBetween(3, 5, true);' in item_deny_example
assert 'rules.ChooseWhenStackBetween(nameof(ExampleMightyPrefix), 1, 2);' in item_deny_example
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
welcome = (playable / "content/Common/Players/ExampleWelcomePlayer.cs").read_text(encoding="utf-8")
assert "C# Hook QA 0.1.58:" in welcome
assert "consulte os estados H1-H5 no relatorio" in welcome
for name in hook_examples:
    example = (hook_dir / (name + ".cs")).read_text(encoding="utf-8")
    assert "public override string DiagnosticStatus() => Result();" in example, name
qa_doc = (playable / "content/Documentation/HOOK_QA_ANDROID.md").read_text(encoding="utf-8")
assert "H1" in qa_doc and "H5" in qa_doc
assert "SourceCSharpMod" not in source_text
print("EXAMPLEMOD_CSHARP_INDEPENDENT_SOURCE=PASS")
print(f"EXAMPLEMOD_CSHARP_STATIC_CHECK=PASS examples={len(MIRRORS)} features={len(features)}")




summon_source = (playable / "content/Content/Projectiles/ExampleMinion.cs").read_text(encoding="utf-8")
staff_source = (playable / "content/Content/Items/Weapons/Summon/ExampleMinionItem.cs").read_text(encoding="utf-8")
buff_source = (playable / "content/Content/Buffs/ExampleMinionBuff.cs").read_text(encoding="utf-8")
assert "MinionContactDamage = true;" in summon_source
assert "MinionMovement = movement;" in summon_source
assert "Summon = true;" in staff_source
assert "MinionBuff = nameof(ExampleMinionBuff);" in staff_source
assert "MinionProjectile = nameof(ExampleMinion);" in buff_source
assert (playable / "content/Documentation/SUMMON_ANDROID_V94.md").is_file()
print("EXAMPLEMOD_CSHARP_SUMMON_V94=PASS")
