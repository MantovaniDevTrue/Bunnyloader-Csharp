#include "content/scene/SceneEffects.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>

using namespace bl::runtime::scene;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

static Candidate effect(const char* key, bool active, int priority, double weight) {
    Candidate c;
    c.key = key;
    c.active = active;
    c.priority = priority;
    c.weight = weight;
    return c;
}

static void emptyAndInactive() {
    Result r = rank({});
    CHECK(!r.anyActive && r.active.empty());
    for (const auto& pick : r.channels) CHECK(pick.value == -1 && pick.from == -1 && pick.priority == None);
    CHECK(r.map.from == -1 && r.map.value.empty());

    Candidate off = effect("a/Off", false, BossHigh, 1.0);
    off.values = {5, 5, 5, 5};
    off.map = "x";
    r = rank({off});
    CHECK(!r.anyActive && r.channels[Music].from == -1 && r.map.from == -1);
}

static void priorityBeatsWeight() {
    Candidate low = effect("m/Low", true, BiomeLow, 1.0);       // rank 2.0
    Candidate high = effect("m/High", true, BiomeMedium, 0.0);  // rank 2.0, key sorts after
    low.values[WaterStyle] = 15;
    high.values[WaterStyle] = 16;
    Result r = rank({low, high});
    // Equal rank: the key decides ("m/High" < "m/Low").
    CHECK(r.channels[WaterStyle].value == 16 && r.channels[WaterStyle].from == 1);

    high.weight = 0.0;
    low.priority = BiomeMedium;  // rank 3.0 beats 2.0
    r = rank({low, high});
    CHECK(r.channels[WaterStyle].value == 15 && r.channels[WaterStyle].priority == BiomeMedium);
    CHECK(r.active.size() == 2 && r.active[0] == 0 && r.active[1] == 1);
}

static void weightClampAndNonFinite() {
    const double inf = std::numeric_limits<double>::infinity();
    Candidate a = effect("a/A", true, BiomeLow, 7.0);                                        // clamps to 1 -> 2.0
    Candidate b = effect("a/B", true, BiomeLow, -3.0);                                       // clamps to 0 -> 1.0
    Candidate c = effect("a/C", true, BiomeLow, std::numeric_limits<double>::quiet_NaN());   // NaN -> 0 -> 1.0
    Candidate d = effect("a/D", true, BiomeLow, inf);                                        // +inf -> 1 -> 2.0
    Candidate e = effect("a/E", true, BiomeLow, -inf);                                       // -inf -> 0 -> 1.0
    a.values[SurfaceBackground] = 20;
    b.values[SurfaceBackground] = 21;
    c.values[SurfaceBackground] = 22;
    d.values[SurfaceBackground] = 23;
    e.values[SurfaceBackground] = 24;
    Result r = rank({e, d, c, b, a});
    CHECK(r.channels[SurfaceBackground].value == 20);
    CHECK(r.channels[SurfaceBackground].weight == 1.0);
    // a and d tie at 2.0 (key order A, D); b, c, e tie at 1.0 (B, C, E).
    CHECK(r.active.size() == 5);
    CHECK(r.active[0] == 4 && r.active[1] == 1 && r.active[2] == 3 && r.active[3] == 2 && r.active[4] == 0);
}

static void channelsComeFromDifferentEffects() {
    Candidate boss = effect("m/Boss", true, BossLow, 0.5);
    boss.values[Music] = 3;
    Candidate biome = effect("m/Biome", true, BiomeLow, 0.25);
    biome.values[Music] = 9;                     // loses the music channel to the boss
    biome.values[UndergroundBackground] = 30;
    biome.values[WaterStyle] = 17;
    biome.map = "Maps/Biome";
    Result r = rank({biome, boss});
    CHECK(r.channels[Music].value == 3 && r.channels[Music].from == 1 && r.channels[Music].priority == BossLow);
    CHECK(r.channels[UndergroundBackground].value == 30 && r.channels[UndergroundBackground].from == 0);
    CHECK(r.channels[UndergroundBackground].weight == 0.25);
    CHECK(r.channels[WaterStyle].value == 17 && r.channels[WaterStyle].priority == BiomeLow);
    CHECK(r.channels[SurfaceBackground].from == -1 && r.channels[SurfaceBackground].value == -1);
    CHECK(r.map.value == "Maps/Biome" && r.map.from == 0);
}

static void musicSentinel() {
    Candidate silence = effect("m/Silence", true, Event, 0.5);
    silence.values[Music] = 0;                   // 0 is silence and a real value
    Candidate game = effect("m/Game", true, BossHigh, 0.5);   // -1: keep the game's music
    Result r = rank({silence, game});
    CHECK(r.channels[Music].value == 0 && r.channels[Music].from == 0);

    Candidate odd = effect("m/Odd", true, BossHigh, 0.5);
    odd.values[Music] = -2;                      // only -1 is skipped, like the original
    r = rank({silence, odd});
    CHECK(r.channels[Music].value == -2 && r.channels[Music].from == 1);
}

static void independentOfInputOrder() {
    std::vector<Candidate> list;
    const char* keys[] = {"u1/Aa", "u1/Ab", "u2/Aa", "u2/B", "u3/Z"};
    int priorities[] = {BiomeLow, BiomeLow, BiomeMedium, BiomeLow, Environment};
    double weights[] = {0.5, 0.5, 0.0, 1.0, 0.0};
    for (int i = 0; i < 5; ++i) {
        Candidate c = effect(keys[i], i != 3, priorities[i], weights[i]);
        c.values = {i, 10 + i, 20 + i, 30 + i};
        c.map = i % 2 ? "map" + std::to_string(i) : "";
        list.push_back(c);
    }
    auto pickKeys = [&](const std::vector<Candidate>& in) {
        Result r = rank(in);
        std::vector<std::string> out;
        for (const auto& pick : r.channels) out.push_back(pick.from < 0 ? "-" : in[pick.from].key);
        out.push_back(r.map.from < 0 ? "-" : in[r.map.from].key);
        for (int index : r.active) out.push_back(in[index].key);
        return out;
    };
    const auto expected = pickKeys(list);
    std::vector<int> order(list.size());
    std::iota(order.begin(), order.end(), 0);
    int permutations = 0;
    do {
        std::vector<Candidate> shuffled;
        for (int index : order) shuffled.push_back(list[index]);
        CHECK(pickKeys(shuffled) == expected);
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    CHECK(permutations == 120);
}

int main() {
    emptyAndInactive();
    priorityBeatsWeight();
    weightClampAndNonFinite();
    channelsComeFromDifferentEffects();
    musicSentinel();
    independentOfInputOrder();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("scene effect ranking: ok");
    return 0;
}
