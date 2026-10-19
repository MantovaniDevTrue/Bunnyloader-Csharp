#include "experiments/csharp/NativeTileDrawWire.h"
#include <cassert>
#include <iostream>
#include <string_view>

int main() {
    using namespace bl::csharp::tile_draw;
    Data d{};
    assert(valid(d));
    assert(parse("DRAW|16|32|16|16|-4|0|36|1", &d));
    assert(d.frameX == 16 && d.frameY == 32 && d.top == -4 &&
           d.addFrameY == 36 && d.effects == 1 && d.drawDefault);
    assert(parse("DRAW|16|32|16|16|-4|0|36|1|0", &d));
    assert(!d.drawDefault && d.width == 16 && d.height == 16);
    assert(parse("DRAW|16|32|16|16|-4|0|36|1|1", &d) && d.drawDefault);
    for (std::string_view bad : {
        "", "READY", "DRAW|", "DRAW|0|0|16|16|0|0|0",
        "DRAW|0|0|16|16|0|0|0|0|2",
        "DRAW|0|0|16|16|0|0|0|0|",
        "DRAW|0|0|16|16|0|0|0|0|1|0",
        "DRAW|-1|0|16|16|0|0|0|0",
        "DRAW|0|0|0|16|0|0|0|0",
        "DRAW|0|0|16|0|0|0|0|0",
        "DRAW|0|0|16|16|0|0|0|4",
        "DRAW|0|0|16|16|0|0|8193|0",
        "DRAW|0|0|16|16|0|-1|0|0",
        "DRAW|0|0|16|16|0|0|-1|0",
        "DRAW|0|0|16|16|0|0|x|0",
        "DRAW|32768|0|16|16|0|0|0|0",
        "DRAW|0|0|16|16|129|0|0|0",
        "DRAW|0|0|16|16|0|0|2147483648|0",
        "DRAW|0|0|16|16|0|0|0|0 ",
    }) assert(!parse(bad, &d));
    assert(!parse("DRAW|0|0|16|16|0|0|0|0", nullptr));
    assert(parse("DRAW|8192|0|512|512|128|-8192|8192|3", &d));
    assert(d.width == 512 && d.height == 512 && d.effects == 3);
    std::cout << "CSHARP_TILE_DRAW_DATA_WIRE=PASS\\n";
}
