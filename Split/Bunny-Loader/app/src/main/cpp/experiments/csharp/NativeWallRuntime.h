#pragma once
#include "experiments/csharp/NativeWallPolicy.h"
namespace bl::csharp::walls {
struct LoadStatus {int pending=0,failed=0;};
LoadStatus loadStatus();
bool publish(std::vector<Definition> definitions,std::string* error);
}
