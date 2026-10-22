#include "experiments/csharp/ManagedBannerRegistry.h"

#include <cassert>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using bl::csharp::banners::Binding;
using bl::csharp::banners::Registry;

static Binding Slime(const std::string& mod = "examplemod", int type = 700, int item = 2100) {
    return {mod, "ExampleSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner",
            type, type, item};
}

int main() {
    Registry registry(700);
    std::string error;
    const Binding representative = Slime();
    const Binding variant = {"examplemod", "ExampleEliteSlimeNPC", "ExampleSlimeNPC",
                             "ExampleSlimeBanner", 701, 700, 2100};

    assert(registry.validate("examplemod", {variant, representative}, &error));
    assert(registry.snapshot()->npcToBanner.empty());
    assert(registry.publish("examplemod", {variant, representative}, &error));
    assert(registry.npcToBanner(700) == 700);
    assert(registry.npcToBanner(701) == 700);
    assert(registry.bannerToItem(700) == 2100);
    assert(registry.bannerToNpc(700) == 700);
    assert(registry.npcToBanner(1) == -1);
    assert(registry.bannerToItem(2) == -1);

    auto snapshot = registry.snapshot();
    assert(!registry.publish("examplemod", {
        representative, {"examplemod", "Bad", "ExampleSlimeNPC", "Wrong",
                         702, 700, 2101}}, &error));
    assert(registry.snapshot()->npcToBanner == snapshot->npcToBanner);
    assert(registry.snapshot()->bannerToItem == snapshot->bannerToItem);

    assert(!registry.publish("examplemod", {
        {"examplemod", "ExampleSlimeNPC", "ExampleEliteSlimeNPC", "ExampleSlimeBanner",
         700, 701, 2100},
        variant}, &error));
    assert(!registry.publish("examplemod", {
        {"examplemod", "ExampleSlimeNPC", "Ghost", "ExampleSlimeBanner",
         700, 700, 2100}}, &error));
    assert(!registry.publish("examplemod", {
        {"anothermod", "ExampleSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner",
         700, 700, 2100}}, &error));
    assert(!registry.publish("examplemod", {
        {"examplemod", "ExampleSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner",
         699, 699, 2100}}, &error));
    assert(!registry.publish("examplemod", {
        representative, representative}, &error));

    assert(registry.publish("anothermod", {Slime("anothermod", 710, 2150)}, &error));
    assert(registry.npcToBanner(710) == 710);
    assert(!registry.publish("anothermod", {Slime("anothermod", 700, 2150)}, &error));
    assert(!registry.publish("anothermod", {Slime("anothermod", 710, 2100)}, &error));
    assert(registry.npcToBanner(710) == 710 && registry.bannerToItem(700) == 2100);
    registry.unload("examplemod");
    assert(registry.npcToBanner(700) == -1);
    assert(registry.npcToBanner(710) == 710);
    assert(snapshot->npcToBanner.size() == 2);

    // O snapshot antigo continua valido mesmo com unload/reregistro no writer.
    std::atomic<bool> invalid{false};
    std::thread reader([&] {
        for (int i = 0; i < 3000; ++i) {
            auto read = registry.snapshot();
            if (read->npcToBanner.contains(710) &&
                read->npcToBanner.at(710) != 710) invalid.store(true);
            if (read->npcToBanner.contains(700) &&
                read->bannerToItem.find(700) == read->bannerToItem.end())
                invalid.store(true);
        }
    });
    for (int i = 0; i < 150; ++i) {
        assert(registry.publish("examplemod", {representative}, &error));
        registry.unload("examplemod");
    }
    reader.join();
    assert(!invalid.load());

    registry.clear();
    assert(registry.snapshot()->byMod.empty());
    assert(registry.npcToBanner(710) == -1);
    std::cout << "Banner native registry PASS (validation, multi-mod, rollback, unload, readers)\n";
}
