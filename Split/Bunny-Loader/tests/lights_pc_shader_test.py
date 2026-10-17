from base64 import b64decode
from hashlib import sha256
from json import loads
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "samples/LightsAndShadowsCSharp"
effects = root / "Playable/content/Effects"
source = (effects / "Lights.screenfx").read_text()
targets = {}
passes = []
for raw in source.splitlines():
    cells = raw.split("|")
    if cells[0] == "SCENE":
        assert cells == ["SCENE", "LightsAndShadowsCSharp", "LightingCalculator", "ComputeSceneWire"]
        continue
    if cells[0] == "TARGET":
        assert len(cells) == 3
        assert cells[1] not in targets
        assert 0.25 <= float(cells[2]) <= 1.0
        targets[cells[1]] = float(cells[2])
    else:
        assert cells[0] == "PASS" and len(cells) == 6
        assert (effects.parent / cells[1]).is_file()
        assert cells[2] == "screen" or cells[2] in targets
        assert cells[3] == "screen" or cells[3] in targets
        assert cells[2] != cells[3] or cells[2] == "screen"
        passes.append(cells)
assert set(targets) == {"shadowMask", "shadow", "bloomA", "bloomB"}
assert all(scale <= 1 / 3 for scale in targets.values())
assert len(passes) == 9
assert passes[0][1] == "Effects/ShadowMask.frag" and passes[0][3] == "shadowMask"
assert passes[1][1] == "Effects/ShadowRays.frag" and passes[1][2] == "shadowMask"
assert passes[1][3] == "shadow" and passes[1][5] == "-"
assert [p[4].split(",")[0] for p in passes[4:8]] == [f"uDistance={i}" for i in range(4)]
assert (effects / "Lights_Alternate.screenfx").is_file()
assert (effects.parent / "Common/Lighting/LightingCalculator.cs").is_file()
mask = (effects / "ShadowMask.frag").read_text()
rays = (effects / "ShadowRays.frag").read_text()
assert "uShadowThreshold - softness" in mask
assert "smoothstep(" in mask
assert "uDayBlend" in mask
assert "pow(max(1.0 - pixel.b, 0.0), 0.75)" in mask
assert "texture(uSource, clamp(uSun + offset / scale, 0.0, 1.0)).a" in rays
assert "i < 30" in rays and "i >= 20" in rays
assert "mix(nightWeight, dayWeight, dayFactor)" in rays
assert "uQuality" in rays and "(30.0 - j) / 120.0" in rays
assert "uBloom" in (effects / "BloomComposite.frag").read_text()
assert (root / "Playable/content/main.cs").read_text().count("BunnyScreenEffect") == 1
manifest = loads((root / "Playable/manifest.json").read_text())
assert manifest["version"] == "0.4.2"
compose = (effects / "LightCompose.frag").read_text()
assert "uGradientDay" in compose and "uGradientNight" in compose
assert "mix(moonlight, daylight" in compose
assert "uSceneShadowThreshold=@threshold" in source
assert "@param.DayShadowThreshold" in source
assert "@param.NightShadowThreshold" in source
assert "@param.RayQuality" in source
assert "@param.BloomStrength" in source
assert "mix(uNightShadowThreshold, uDayShadowThreshold, day)" in mask
assert "uSceneShadowThreshold" in mask
mod_source = (root / "Playable/content/main.cs").read_text()
for name in ("SunPower", "DayShadowThreshold", "NightShadowThreshold", "ShadowStrength",
             "RayQuality", "BloomThreshold", "BloomStrength", "BloomPower", "BlurGain"):
    assert "Uniform_" + name in mod_source
assert "uDayBlend=@dayblend" in source
assert "uGradientDay=Assets/ColorTex.png" in source
assert "uGradientNight=Assets/ColorTex2.png" in source
assets = {
    "ColorTex.png": "f354236337a4a002f2ba2897be214032ab9dc82719ec56ad847b3f829fe0c59c",
    "ColorTex2.png": "4861ad63036f3bcf51664c84bea7ea4836c237c86b5d0fe3943b3e33c79f04d2",
    "ColorTex_Alt.png": "e97f0beb9af9a891b13a6d5980c94e46dc8ebb32e050d7cf01f6bb029ab84a54",
}
for name, expected in assets.items():
    raw = b64decode((root / "SourceAssets" / (name + ".b64")).read_text().strip(), validate=True)
    assert raw[:8] == bytes((137, 80, 78, 71, 13, 10, 26, 10))
    assert sha256(raw).hexdigest() == expected, name
print("LIGHTS_PC_GRAPH_FIDELITY=PASS passes=9 lowres_targets=4 assets=3")
