#include "experiments/csharp/NativeHairStartup.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace bl::csharp::hair;

void check(bool yes, const char* name) {
    if (!yes) {
        std::cerr << "FAIL " << name << '\n';
        std::exit(1);
    }
    std::cout << "OK " << name << '\n';
}

int main() {
    NativeHairSlotSnapshot live;
    int snapshots = 0;
    NativeHairStartup startup([&]() {
        ++snapshots;
        return live;
    });
    std::string error;
    NativeHairStartup::Block block;

    check(!startup.Begin("mod-a", 1, &block, &error) &&
          error == "HairStartupTextureAssetsUnavailable" &&
          block.first == -1, "sem TextureAssets rejeita preflight");
    live.valid = true;
    live.occupied.set(228);
    live.occupied.set(229);
    check(startup.Begin("mod-a", 1, &block, &error) &&
          block.first == 230 && snapshots == 2,
          "preflight reserva ID apos JS somente quando tables existem");
    check(!startup.ReadyDescriptors(),
          "registro ainda invisível antes de manifesto e Seal");
    const std::string manifest =
        "BUNNY_HAIR_MANIFEST_V1\n"
        "HAIR\t230\tmod-a\tHair\tContent/Hairs/Hair.png\tContent/Hairs/Hair.png\t0\t1\n";
    check(startup.CommitManifest("mod-a", manifest,
           [](std::string_view) { return true; }, &error),
           "MonoVM retorna manifesto para o bloco reservado");
    check(!startup.ReadyDescriptors(), "commit não publica antes de Seal");

    live.occupied.set(230);
    check(!startup.Seal(&error) &&
          error == "HairStagingOccupiedOrMissingType:230" &&
          !startup.ReadyDescriptors(), "mudança de slots durante Roslyn bloqueia instalação");
    live.occupied.reset(230);
    check(startup.Seal(&error) && snapshots == 4 &&
          startup.ReadyDescriptors() &&
          startup.ReadyDescriptors()->Resolve("mod-a/Hair") == 230,
          "revalidação libera descritor apenas após Seal");
    check(!startup.Begin("mod-b", 1, &block, &error),
          "não reserva após início do install");
    startup.Reset();
    check(!startup.ReadyDescriptors() &&
          startup.Reservation("mod-a").first == -1,
          "unload limpa descritores e reserva em memória");

    NativeHairStartup noCallback({});
    check(!noCallback.Begin("mod-x", 1, &block, &error) &&
          error == "HairStartupMissingTextureSlotReader",
          "sem bridge de game thread não tenta acessar slots");

    std::cout << "HairStartup simulation: snapshot->MonoVM manifest->Seal PASS. NO APK.\n";
}
