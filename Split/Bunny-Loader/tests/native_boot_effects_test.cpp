#include "experiments/csharp/NativeBootEffects.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    int32_t x=-1, y=-1;
    using bl::csharp::nativeBootFlameTile;
    assert(nativeBootFlameTile(100,200,20,42,2,0,false,false,&x,&y));
    assert(x==6 && y==15); // centre X, bottom Y, pixels -> tile
    assert(nativeBootFlameTile(100,200,20,42,-2,0,false,false,&x,&y));
    assert(!nativeBootFlameTile(100,200,20,42,1,0,false,false,&x,&y));
    assert(!nativeBootFlameTile(100,200,20,42,2,-1,false,false,&x,&y));
    assert(!nativeBootFlameTile(100,200,20,42,2,0,true,false,&x,&y));
    assert(!nativeBootFlameTile(100,200,20,42,2,0,false,true,&x,&y));
    assert(!nativeBootFlameTile(100,200,20,42,2,0,false,false,nullptr,&y));
    assert(!nativeBootFlameTile(std::numeric_limits<float>::quiet_NaN(),200,20,42,2,0,false,false,&x,&y));
    assert(!nativeBootFlameTile(std::numeric_limits<float>::max(),200,20,42,2,0,false,false,&x,&y));
    assert(!nativeBootFlameTile(-100,200,20,42,2,0,false,false,&x,&y));
    std::cout << "NATIVE_BOOT_EFFECTS_PASS=YES\n";
}
