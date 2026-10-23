#include "experiments/csharp/NativeAchievementManifest.h"

#include <cassert>
#include <string>
#include <vector>

using namespace bl::csharp::achievement;

int main() {
    std::vector<Descriptor> items;
    const std::string wire = "ACHMETA1\n"
        "A|6D2F4D616E79576F726D734B696C6C6564|4D616E7920576F726D73204B696C6C6564||"
        "436F6E74656E742F416368696576656D656E74732F4D616E79576F726D734B696C6C6564|0|1|1|0\n"
        "C|436F6E646974696F6E|1|5|";
    assert(parseManifest("m", wire, &items));
    assert(items.size() == 1);
    assert(items[0].name == "m/ManyWormsKilled");
    assert(items[0].category == 1);
    assert(items[0].hidden);
    assert(!items[0].completed);
    assert(items[0].conditions.size() == 1);
    assert(items[0].conditions[0].maximum == 5.f);
    assert(items[0].conditions[0].kind == 1);
    auto kept = items;
    assert(!parseManifest("other", wire, &items));
    assert(items.size() == kept.size());
    assert(!parseManifest("m", wire + "\nC|436F6E646974696F6E|1|5|", &items));
    assert(!parseManifest("m", wire + "\nC|4E6577|3|1|", &items));
    assert(!parseManifest("m", wire + "\nC|4E6577|3|1|999999", &items));
    assert(!parseManifest("m", "ACHMETA1\nC|536F6C6F|1|5|", &items));
    assert(!parseManifest("m", "ACHMETA1\nA|ZZ|4141||4141|0|0|0|0", &items));
    assert(parseManifest("m", "ACHMETA1", &items));
    assert(items.empty());
    return 0;
}
