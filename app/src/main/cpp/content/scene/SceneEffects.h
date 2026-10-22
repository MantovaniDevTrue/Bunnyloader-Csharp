#pragma once
#include <array>
#include <string>
#include <vector>

namespace bl::runtime::scene {

// Same numeric scale as SceneEffectPriority in the original JS loader.
enum Priority : int {
    None = 0, BiomeLow = 1, BiomeMedium = 2, BiomeHigh = 3,
    Environment = 4, Event = 5, BossLow = 6, BossMedium = 7, BossHigh = 8
};

enum Channel : int { Music = 0, SurfaceBackground, UndergroundBackground, WaterStyle, kValueChannels };

// One scene effect (or biome) as seen by the ranking for one player and frame.
// `values` holds the slot each channel wants; -1 means the effect does not fill
// that channel. For Music, -1 also means "leave the game's own music".
struct Candidate {
    std::string key;                       // "<mod uid>/<Class>": tie-break and log identity
    bool active = false;
    int priority = None;
    double weight = 0.5;                   // raw; clamped to [0,1] by the ranking
    std::array<int, kValueChannels> values{-1, -1, -1, -1};
    std::string map;                       // map background texture path, empty = none
};

struct ChannelPick {
    int value = -1;
    int priority = None;
    int from = -1;                         // index into the candidate list, -1 = nobody
    double weight = 0.0;                   // rank - priority, used by the surface/underground blend
};

struct MapPick {
    std::string value;
    int from = -1;
};

struct Result {
    bool anyActive = false;
    std::array<ChannelPick, kValueChannels> channels{};
    MapPick map;
    std::vector<int> active;               // candidate indexes, best rank first
};

// Pure, deterministic ranking: the result depends only on the candidates'
// contents and keys, never on their order in the list. Best rank is
// clamp(weight, 0, 1) + priority; equal ranks are ordered by key, ascending
// by bytes. Each channel comes from the best-ranked active candidate that
// fills it. A NaN weight counts as 0; infinities clamp to 0 or 1.
Result rank(const std::vector<Candidate>& candidates);

}  // namespace bl::runtime::scene
