#include "experiments/csharp/NativeHairStaging.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace bl::csharp::hair;

static void check(bool yes, const char* label) {
    if (!yes) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK " << label << '\n';
}

std::string record(int type, std::string_view mod, std::string_view name) {
    const std::string path = "Content/Hairs/" + std::string(name) + ".png";
    return "HAIR\t" + std::to_string(type) + "\t" + std::string(mod) + "\t" +
           std::string(name) + "\t" + path + "\t" + path + "\t0\t1\n";
}

int main() {
    const std::string header = "BUNNY_HAIR_MANIFEST_V1\n";
    NativeHairStaging stage;
    std::set<int> occupied{228, 229, 236, 237};
    const auto used = [&](int id) { return occupied.contains(id); };
    auto assetExists = [](std::string_view) { return true; };
    std::string error;
    NativeHairStaging::Block first, second, third;

    check(stage.Begin("mod-a", 2, used, &first, &error) &&
          first.first == 230 && first.count == 2, "reserva apos Bunny JS");
    check(stage.Begin("mod-b", 3, used, &second, &error) &&
          second.first == 232 && second.count == 3, "bloco isolado por mod");
    check(stage.PendingCount() == 2, "dois manifestos pendentes");
    check(!stage.Begin("mod-a", 1, used, &third, &error) &&
          error == "HairStagingDuplicateOrInvalidMod",
          "registro repetido não muda IDs");

    const std::string foreign = header + record(230, "foreign", "A") +
                               record(231, "mod-a", "B");
    check(!stage.CommitManifest("mod-a", foreign, assetExists, &error) &&
          error == "HairStagingForeignModIdentity" &&
          stage.Descriptors().Size() == 0,
          "UID forjado nunca entra no registro global");
    check(!stage.CommitManifest("mod-a", header + record(230, "mod-a", "A"),
                                assetExists, &error) &&
          error == "HairStagingCountMismatch" &&
          stage.Descriptors().Size() == 0,
          "contagem incorreta não registra parcialmente");
    check(!stage.CommitManifest("mod-a", header + record(231, "mod-a", "A") +
                                record(232, "mod-a", "B"), assetExists, &error) &&
          error == "NativeHairManifestNumbersInvalid" &&
          stage.Descriptors().Size() == 0,
          "manifesto de ID errado não altera nada");
    check(!stage.Seal(used, &error) &&
          error.starts_with("HairStagingUncommittedMod"),
          "seal exige todos os mods validados");

    check(stage.CommitManifest("mod-a", header + record(230, "mod-a", "A") +
                                record(231, "mod-a", "B"), assetExists, &error),
          "primeiro mod commit atômico");
    check(stage.Descriptors().Size() == 2 &&
          stage.Descriptors().Resolve("mod-a/B") == 231,
          "bloco A preservado");
    check(!stage.CommitManifest("mod-a", header + record(230, "mod-a", "A") +
                                record(231, "mod-a", "B"), assetExists, &error),
          "replay de commit não registra duplicado");

    check(stage.CommitManifest("mod-b", header + record(232, "mod-b", "C") +
                                record(233, "mod-b", "D") +
                                record(234, "mod-b", "E"), assetExists, &error),
          "segundo mod adicionado ao mesmo registry");
    check(stage.Descriptors().Size() == 5 && stage.PendingCount() == 0,
          "cinco cabelos prontos, sem pendências");
    occupied.insert(233);
    check(!stage.Seal(used, &error) &&
          error == "HairStagingOccupiedOrMissingType:233",
          "Bunny JS registrou cabelo enquanto Mono compilava, seal bloqueia");
    occupied.erase(233);
    check(stage.Seal(used, &error) && stage.Sealed(),
          "seal só libera descritores após novo check de slots");
    check(!stage.Begin("mod-c", 1, used, &third, &error) &&
          error == "HairStagingAlreadySealed",
          "registry selado não aceita compilação tardia");
    stage.Reset();
    check(stage.Descriptors().Size() == 0 && !stage.Sealed(),
          "fim de sessão zera tudo sem mexer em IL2CPP");

    check(stage.Begin("mod-c", 1, used, &third, &error) &&
          third.first == 230, "nova sessão não herda reserva anterior");
    check(stage.AbortUncommitted("mod-c"), "falha Mono libera reserva pendente");
    check(stage.Begin("mod-c", 1, used, &third, &error) &&
          third.first == 230, "recompilação reutiliza slot não publicado");
    check(stage.CommitManifest("mod-c", header + record(230, "mod-c", "Hair"),
                                assetExists, &error), "commit C");
    check(!stage.AbortUncommitted("mod-c"), "commit não pode ser apagado isoladamente");

    std::cout << "NativeHairStaging atomic cross-mod manifest tests PASS; NO APK.\n";
}
