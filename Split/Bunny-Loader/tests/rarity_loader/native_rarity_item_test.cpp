#include "experiments/csharp/BasicItemDefaults.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    bl::csharp::BasicItemEntry item;
    item.name = "ExampleRarityItem";
    std::string error;
    assert(bl::csharp::validateBasicItem(item, &error));
    item.values[bl::csharp::BasicRare] = 4107;
    assert(bl::csharp::validateBasicItem(item, &error));
    item.values[bl::csharp::BasicRare] = 4108;
    assert(!bl::csharp::validateBasicItem(item, &error));
    item.values[bl::csharp::BasicRare] = -12;
    assert(!bl::csharp::validateBasicItem(item, &error));

    std::vector<std::string> fields = {
        "ITEM", "ExampleRarityItem", "24", "24", "1", "0",
        "12", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0", "-", "-", "-"
    };
    bl::csharp::BasicItemEntry parsed;
    assert(!bl::csharp::parseBasicItemWire(fields, &parsed, &error));
    assert(error.find("SetRarity") != std::string::npos);
    fields[6] = "11";
    error.clear();
    assert(bl::csharp::parseBasicItemWire(fields, &parsed, &error));
    assert(parsed.values[bl::csharp::BasicRare] == 11);
}
