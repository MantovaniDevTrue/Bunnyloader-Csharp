#include "experiments/csharp/ManagedBannerLookupHooks.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace bl::csharp::banners;

static int vanillaNpcCalls = 0;
static int vanillaItemCalls = 0;
static int vanillaBannerNpcCalls = 0;

static int32_t VanillaNpc(int32_t value, const MethodInfo*) {
    ++vanillaNpcCalls;
    return value + 3;
}
static int32_t VanillaItem(int32_t value, const MethodInfo*) {
    ++vanillaItemCalls;
    return value + 100;
}
static int32_t VanillaBannerNpc(int32_t value, const MethodInfo*) {
    ++vanillaBannerNpcCalls;
    return value + 200;
}

int main() {
    constexpr int firstModNpc = bl::runtime::kVanillaNpcCount;
    auto& reg = registry(firstModNpc);
    std::string error;
    const Binding owner = {"example", "Slime", "Slime", "SlimeBanner",
                           firstModNpc, firstModNpc, 2100};
    assert(reg.publish("example", {owner}, &error));
    originalNpcToBanner = VanillaNpc;
    originalBannerToItem = VanillaItem;
    originalBannerToNpc = VanillaBannerNpc;

    // Quando o sistema de banners ainda nao tem os arrays expandidos, TODAS
    // as consultas passam ao jogo original mesmo com mappings ja presentes.
    setBannerLookupsActive(true);
    assert(!lookupsActive.load());
    assert(lookupNpcToBanner(firstModNpc, nullptr) == firstModNpc + 3);
    assert(lookupBannerToItem(firstModNpc, nullptr) == firstModNpc + 100);
    assert(lookupBannerToNpc(firstModNpc, nullptr) == firstModNpc + 200);

    // Este teste da politica simula o gate aberto, NAO ativa a aplicacao no
    // Android. A fonte de producao ainda nao chama setBannerLookupsActive(true).
    hooksInstalled = true;
    setBannerLookupsActive(true);
    assert(lookupNpcToBanner(firstModNpc, nullptr) == firstModNpc);
    assert(lookupBannerToItem(firstModNpc, nullptr) == 2100);
    assert(lookupBannerToNpc(firstModNpc, nullptr) == firstModNpc);
    assert(lookupNpcToBanner(30, nullptr) == 33);
    assert(lookupBannerToItem(5, nullptr) == 105);
    assert(lookupBannerToNpc(5, nullptr) == 205);
    assert(lookupNpcToBanner(firstModNpc + 3, nullptr) == firstModNpc + 6);
    assert(lookupBannerToItem(firstModNpc + 3, nullptr) == firstModNpc + 103);
    assert(lookupBannerToNpc(firstModNpc + 3, nullptr) == firstModNpc + 203);
    assert(vanillaNpcCalls == 3);
    assert(vanillaItemCalls == 3);
    assert(vanillaBannerNpcCalls == 3);

    reg.unload("example");
    assert(lookupNpcToBanner(firstModNpc, nullptr) == firstModNpc + 3);
    assert(lookupBannerToItem(firstModNpc, nullptr) == firstModNpc + 100);
    assert(lookupBannerToNpc(firstModNpc, nullptr) == firstModNpc + 200);
    setBannerLookupsActive(false);
    assert(!lookupsActive.load());
    reg.clear();
    originalNpcToBanner = nullptr;
    originalBannerToItem = nullptr;
    originalBannerToNpc = nullptr;
    std::cout << "Banner lookup dispatch PASS (inactive, mod, vanilla, unload)\n";
}
