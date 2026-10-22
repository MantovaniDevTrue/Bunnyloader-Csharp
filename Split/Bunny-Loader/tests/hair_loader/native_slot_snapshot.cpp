#include "experiments/csharp/NativeHairSlotProbe.h"
#include "experiments/csharp/NativeHairStaging.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace bl::csharp::hair;

static void check(bool yes, const char* why) {
    if (!yes) {
        std::cerr << "FAIL " << why << '\n';
        std::exit(1);
    }
    std::cout << "OK " << why << '\n';
}

int main() {
    NativeHairSlotSnapshot unavailable;
    check(unavailable.IsOccupied(228) && unavailable.IsOccupied(4095),
          "sem TextureAssets: todos slots são tratados como ocupados");
    unavailable.valid = true;
    unavailable.occupied.set(228);
    unavailable.occupied.set(229);
    unavailable.occupied.set(240);
    check(unavailable.IsOccupied(228) && unavailable.IsOccupied(229) &&
          !unavailable.IsOccupied(230) && unavailable.IsOccupied(240) &&
          unavailable.IsOccupied(-1) && unavailable.IsOccupied(4096),
          "snapshot vanilla + mods JS possui limites seguros");

    NativeHairStaging stage;
    NativeHairStaging::Block block;
    std::string error;
    auto occupied = [&](int id) { return unavailable.IsOccupied(id); };
    check(stage.Begin("mod-a", 2, occupied, &block, &error) &&
          block.first == 230, "staging usa snapshot nativo em vez de ID fixo 228");

    const std::string manifest = "BUNNY_HAIR_MANIFEST_V1\n"
        "HAIR\t230\tmod-a\tFirst\tContent/Hairs/First.png\tContent/Hairs/First.png\t0\t1\n"
        "HAIR\t231\tmod-a\tSecond\tContent/Hairs/Second.png\tContent/Hairs/Second.png\t0\t1\n";
    check(stage.CommitManifest("mod-a", manifest,
                               [](std::string_view) { return true; }, &error),
          "manifesto MonoVM importado após reserva");
    unavailable.occupied.set(231);
    check(!stage.Seal(occupied, &error) &&
          error == "HairStagingOccupiedOrMissingType:231",
          "um novo cabelo JS instalado entre compilação e seal bloqueia uso");
    unavailable.occupied.reset(231);
    check(stage.Seal(occupied, &error), "seal com snapshot atualizado aceita bloco");
    stage.Reset();

    unavailable.valid = false;
    check(!stage.Begin("mod-b", 1, occupied, &block, &error) &&
          error == "HairReservationNoContiguousRange",
          "sem tables do Terraria, nenhum slot pode ser publicado");
    std::cout << "NativeHairSlotSnapshot and staging bridge PASS. NO APK.\n";
}
