#include "experiments/csharp/ManagedBannerWorldState.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace bl::csharp::banners;

int main() {
    WorldEntries original = {
        {"examplemod/Slime", {49, 2, true}},
        {"uninstalled/RareSlime", {100, 1, true}}
    };
    std::vector<uint8_t> buffer;
    assert(WorldLedger::encode(original, &buffer));
    assert(buffer.size() > 10 && buffer.size() < 512);
    WorldEntries parsed;
    assert(WorldLedger::decode(buffer, &parsed));
    assert(parsed.size() == original.size());
    assert(parsed.at("examplemod/Slime").kills == 49);
    assert(parsed.at("uninstalled/RareSlime").claimable == 1);

    // Rejeita corrupcao, truncamento, versionamento desconhecido e payload
    // extra. Nenhum caso pode sobrescrever estado validado anteriormente.
    auto corrupted = buffer;
    corrupted[9] ^= 0x80;
    assert(!WorldLedger::decode(corrupted, &parsed) && parsed.size() == 2);
    corrupted = buffer;
    corrupted.resize(buffer.size() - 1);
    assert(!WorldLedger::decode(corrupted, &parsed) && parsed.size() == 2);
    corrupted = buffer;
    corrupted[3] = '2';
    assert(!WorldLedger::decode(corrupted, &parsed) && parsed.size() == 2);
    corrupted = buffer;
    corrupted.insert(corrupted.end(), 0);
    assert(!WorldLedger::decode(corrupted, &parsed) && parsed.size() == 2);

    // Lotes de variantes: um unico contador, duas identidades persistentes.
    Registry registry(697);
    std::string error;
    Binding a = {"examplemod", "Slime", "Slime", "Banner", 697, 697, 2000};
    Binding b = {"examplemod", "EliteSlime", "Slime", "Banner", 698, 697, 2000};
    assert(registry.publish("examplemod", {a, b}, &error));

    std::vector<int32_t> kills(702), pending(702);
    std::vector<uint8_t> fresh(702);
    kills[5] = 37;
    kills[697] = 130;
    pending[697] = 3;
    fresh[697] = 1;
    assert(WorldLedger::capture(*registry.snapshot(), kills, pending, fresh, &parsed));
    assert(parsed.at("examplemod/Slime").kills == 130);
    assert(parsed.at("examplemod/EliteSlime").kills == 130);
    assert(parsed.contains("uninstalled/RareSlime"));
    assert(parsed.size() == 3);

    // Agora a ordem de registro muda (IDs 704/705), e o save continua
    // restaurando no novo ID sem jamais mexer nos arrays vanilla.
    Registry reloaded(697);
    a.npcType = 704; a.representativeType = 704; a.itemType = 2010;
    b.npcType = 705; b.representativeType = 704; b.itemType = 2010;
    assert(reloaded.publish("examplemod", {a, b}, &error));
    kills.resize(710);
    pending.resize(710);
    fresh.resize(710);
    kills[5] = 52;
    assert(WorldLedger::restore(*reloaded.snapshot(), parsed, &kills, &pending, &fresh));
    assert(kills[704] == 130 && pending[704] == 3 && fresh[704]);
    assert(kills[697] == 130 && kills[5] == 52);

    WorldEntries save = {
        {"examplemod/Slime", {10, 1, false}},
        {"examplemod/EliteSlime", {25, 2, true}}
    };
    assert(WorldLedger::restore(*reloaded.snapshot(), save, &kills, &pending, &fresh));
    assert(kills[704] == 25 && pending[704] == 2 && fresh[704]);
    assert(kills[5] == 52);

    auto oldKills = kills;
    auto oldPending = pending;
    auto oldFresh = fresh;
    pending.resize(700);
    assert(!WorldLedger::restore(*reloaded.snapshot(), save, &kills, &pending, &fresh));
    assert(kills == oldKills && fresh == oldFresh);
    pending = oldPending;

    // Capture falha sem afetar o sidecar anterior.
    WorldEntries unchanged = save;
    auto tooSmall = kills;
    tooSmall.resize(2);
    assert(!WorldLedger::capture(*reloaded.snapshot(), tooSmall, pending, fresh, &unchanged));
    assert(unchanged == save);

    // Informacoes maliciosas sao recusadas antes de alocar ou gravar.
    assert(!WorldLedger::keyValid("examplemod/../Slime"));
    assert(!WorldLedger::keyValid("a/a/a"));
    assert(!WorldLedger::keyValid("/Slime"));
    assert(!WorldLedger::keyValid("a/"));
    assert(!WorldLedger::keyValid("a/Slime\n"));
    assert(!WorldLedger::encode({{"examplemod/Slime", {1, 0, true}}}, &buffer));
    assert(!WorldLedger::encode({{"examplemod/Slime", {-1, 0, false}}}, &buffer));

    // Roundtrip vazio e preserve de identidade ausente.
    WorldEntries empty;
    assert(WorldLedger::encode(empty, &buffer));
    assert(WorldLedger::decode(buffer, &parsed));
    assert(parsed.empty());
    assert(WorldLedger::capture(*reloaded.snapshot(), kills, pending, fresh, &parsed));
    assert(parsed.size() == 2);

    std::cout << "Banner world state PASS (CRC, malformed, multi-mod, rebind, transactional arrays)\n";
}
