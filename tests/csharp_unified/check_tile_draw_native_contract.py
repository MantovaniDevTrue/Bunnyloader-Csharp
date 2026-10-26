from pathlib import Path

source = Path("app/src/main/cpp/content/tiles/ModTiles.cpp").read_text()
read = source[source.index("DrawShape readDrawShape("):source.index("void hookDrawData()")]
assert '#include "il2cpp/FieldContract.h"' in source
assert read.index("nativeManagedPointerPlausible(heights)") < read.index("heights->length")
assert read.index("tile_draw_shape::valid(s.rows, length)") < read.index("arrayData(heights)")
assert read.index('nativeArrayContractIssue(heights, "Int32"') < read.index("arrayData(heights)")
assert read.index("CoordinateHeightsRuntimeClassMismatch") < read.index("heights->length")
assert "CoordinateHeightsOutOfBounds" in read
print("TILE_DRAW_NATIVE_ARRAY_CONTRACT=PASS")
