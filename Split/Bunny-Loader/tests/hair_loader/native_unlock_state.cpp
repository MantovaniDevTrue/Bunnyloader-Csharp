#include "experiments/csharp/NativeHairUnlockState.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace bl::csharp::hair;

void check(bool passed, const char* name) {
    if (!passed) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
    std::cout << "OK: " << name << '\n';
}

struct World {
    bool bossDefeated = false;
    int queries = 0;
    bool available = true;
};

bool unlock(void* context, const Entry& entry, bool creation,
            bool stylist, bool* result) {
    auto& world = *static_cast<World*>(context);
    world.queries++;
    if (!world.available || creation || !stylist) return false;
    *result = entry.fullName == "mod-a/A" || world.bossDefeated;
    return true;
}

int main() {
    Registry registry;
    std::set<std::string> existing{"Content/Hairs/A.png", "Content/Hairs/B.png"};
    auto exists = [&](std::string_view path) { return existing.contains(std::string(path)); };
    std::string error;
    check(registry.RegisterBatch({
        {"mod-a", "A", "Content/Hairs/A", "Content/Hairs/A", RandomGender::Unspecified, true},
        {"mod-b", "B", "Content/Hairs/B", "Content/Hairs/B", RandomGender::Female, false}
    }, exists, &error), "registrar dois penteados");

    World world;
    UnlockState state;
    check(state.Refresh(registry, true, false, unlock, &world), "primeira lista da criacao");
    check(state.IsAvailable(228) && !state.IsAvailable(229), "criacao respeita AvailableDuringCharacterCreation");
    check(world.queries == 0, "criacao nao chama condições do Cabeleireiro");
    check(!state.Refresh(registry, true, false, unlock, &world), "estado de criacao estavel");
    check(state.Refresh(registry, false, true, unlock, &world), "troca de contexto forca refresh");
    check(state.IsAvailable(228) && !state.IsAvailable(229), "Stylist inicialmente fechado para segundo mod");
    world.bossDefeated = true;
    check(state.Refresh(registry, false, true, unlock, &world), "progressao de mundo ativa o segundo");
    check(state.AvailableTypes().size() == 2, "Style list atualiza dinamicamente");
    check(!state.Refresh(registry, false, true, unlock, &world), "sem mudanca nao recria lista");
    world.available = false;
    check(state.Refresh(registry, false, true, unlock, &world) &&
          state.AvailableTypes().empty(), "callback indisponivel falha fechado");
    check(!state.Refresh(registry, false, true, nullptr, nullptr) &&
          state.AvailableTypes().empty(), "ausencia Mono nao libera estilos");
    check(state.Refresh(registry, false, false, unlock, &world) &&
          state.AvailableTypes().empty(), "fora da UI permanece vazio");
    state.Reset();
    check(state.Refresh(registry, true, false, unlock, &world),
          "reset permite outra sessao");
    registry.Reset();
    check(state.Refresh(registry, true, false, unlock, &world) &&
          state.AvailableTypes().empty(), "remocao de mods elimina IDs antigos");
    std::cout << "HairUnlockState contracts OK. NO APK.\n";
}
