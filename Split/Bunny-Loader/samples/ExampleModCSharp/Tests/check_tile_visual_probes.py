from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PLAYABLE = ROOT / "samples/ExampleModCSharp/Playable/content"
SOURCE = ROOT / "samples/ExampleMod/content"

TILES = {
    "ExampleCampfireVisualProbe": "ExampleCampfire",
    "ExampleLampVisualProbe": "ExampleLamp",
}

for probe, original in TILES.items():
    tile = PLAYABLE / f"Content/Tiles/VisualProbes/{probe}.cs"
    body = tile.read_text(encoding="utf-8")
    assert f"class {probe} : BunnyTile" in body
    assert "CacheDrawData=false;" in body
    assert ("Shape=BunnyTileShape.Style3x2;" if "Campfire" in probe
            else "Shape=BunnyTileShape.Style1x3;") in body
    assert "DrawFlameTexture(" in body
    assert "EmitDustStyled(" in body
    assert ("FrameToggle=BunnyTileFrameToggle.CampfireVertical;" if "Campfire" in probe
            else "FrameToggle=BunnyTileFrameToggle.LampHorizontal;") in body
    assert "ModifyLight(" in body
    for extension in (".png", "_Flame.png"):
        target = PLAYABLE / f"Content/Tiles/VisualProbes/{probe}{extension}"
        source = SOURCE / f"Content/Tiles/{original}{extension}"
        assert target.read_bytes() == source.read_bytes(), target
    item = PLAYABLE / f"Content/Items/Placeable/VisualProbes/{probe}Item.cs"
    item_text = item.read_text(encoding="utf-8")
    assert f'class {probe}Item : BunnyItem' in item_text
    assert f'DefaultToPlaceableTile("{probe}")' in item_text
    assert 'AddIngredient<ExampleModCSharp.ExampleItem>' in item_text
    assert 'AddTile(18)' in item_text
    icon = PLAYABLE / f"Content/Items/Placeable/VisualProbes/{probe}Item.png"
    assert icon.read_bytes() == (SOURCE / f"Content/Items/Placeable/{original}.png").read_bytes()

camp = (PLAYABLE / "Content/Tiles/VisualProbes/ExampleCampfireVisualProbe.cs").read_text()
lamp = (PLAYABLE / "Content/Tiles/VisualProbes/ExampleLampVisualProbe.cs").read_text()
assert "AnimationFrameHeight=36;" in camp
assert "BunnyTileFlags.Campfire" in camp
assert "AnimationFrames=8;" in camp
assert "AnimationFrameTicks=4;" in camp
assert "int frame=draw.AnimationFrame;" in camp
assert "Environment.TickCount64/68L" not in camp
assert "DrawFlameTexture(frameX,frameY+frame*36" in camp
assert "Random.Shared.Next(3)" in camp
assert "DrawEffects(" in lamp and "RequestSpecialDraw()" in lamp
assert "SpecialDraw(" in lamp and "for(int i=0;i<7;i++)" in lamp
assert "SetSpriteEffects(" in lamp
print("CSHARP_TILE_VISUAL_PROBES=PASS")
