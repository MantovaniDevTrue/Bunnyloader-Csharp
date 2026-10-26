from pathlib import Path

source = Path("app/src/main/cpp/content/tiles/ModTiles.cpp").read_text()
read = source[source.index("DrawShape readDrawShape("):source.index("void hookDrawData()")]
# Android 301720: o Int32[] de get_CoordinateHeights falhou em todas as checagens
# de array (ponteiro como length, classe de elemento ausente, classe desconhecida).
# A forma nao pode depender desse objeto.
for forbidden in ("get_CoordinateHeights", "runtime_invoke(get", "arrayData(heights)",
                  "heights->length", "heights->bounds"):
    assert forbidden not in read, forbidden
assert "tile_draw_shape::valid(s.rows, 1)" in read
assert read.index("tile_draw_shape::valid(s.rows, 1)") < read.index("reconstructHeights(")
assert "CoordinateHeightsUnreconstructable" in read
assert "Source=CoordinateFullHeight" in read
assert read.index("CoordinateHeightsUnreconstructable") < read.index("s.ok = true;")
keep = source[source.index("void keepObjectData("):source.index("void runStaticDefaults(")]
for stage in (1201, 1202, 1203, 1204):
    assert f"setNativeGameStage({stage});" in keep, stage
assert keep.index("setNativeGameStage(1204);") < keep.index("readDrawShape(obj)")
hook = source[source.index("void hookDrawData()"):source.index("void keepObjectData(")]
for stage in (1205, 1206, 1207, 1208):
    assert f"setNativeGameStage({stage});" in hook, stage
assert hook.index("setNativeGameStage(1206);") < hook.index("hkGetSpecialTileDrawData")
assert hook.index("setNativeGameStage(1207);") < hook.index("hook::install(target, hkGetTileDrawData")
assert source.count("setNativeGameStage(1209);") == 2
assert "~TileStageScope() { bl::csharp::setNativeGameStage(previous); }" in source
print("TILE_DRAW_NATIVE_ARRAY_CONTRACT=PASS")
