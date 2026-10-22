#include "experiments/csharp/NativeHairWorkerPipeline.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace bl::csharp::hair;

namespace bl::csharp {
ManagedHairManifestResult inspectManagedHairClassCount(
    const std::string&, const std::string&, const std::string&) {
    return {0, "mock-available", {}};
}
ManagedHairManifestResult exportManagedHairManifest(
    const std::string&, const std::string&, const std::string&,
    const std::string&, const std::string&, const std::string&, int) {
    return {0, "mock-available", {}};
}
}

static void check(bool result, const char* label) {
    if (!result) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK " << label << '\n';
}

int main(int argc, char** argv) {
    check(argc == 2, "recebe diretório privado com manifesto C# real");
    NativeHairSlotSnapshot slots;
    slots.valid = true;
    for (int id = 228; id < 300; ++id) slots.occupied.set(static_cast<size_t>(id));
    auto assetExists = [](std::string_view s) {
        return s.starts_with("Content/Hairs/") && s.ends_with(".png");
    };
    const auto makeStartup = [&] {
        return NativeHairStartup([&] { return slots; });
    };
    auto startup = makeStartup();
    const auto exported = [&](int first) {
        check(first == 300, "UID recebe primeiro ID calculado pelo C++");
        return 2;
    };
    const auto result = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1], exported, assetExists);
    if (!result.ready)
        std::cerr << "HAIR_WORKER_DEBUG phase=" << result.phase
                  << " error=" << result.error
                  << " reservedFirst=" << result.firstType
                  << " count=" << result.count << '\\n';
    check(result.ready && result.count == 2 && result.firstType == 300 &&
          result.phase == "hair-worker-descriptors-ready",
          "worker consome arquivo exportado por C# e publica apenas descritores");
    check(startup.ReadyDescriptors() &&
          startup.ReadyDescriptors()->Resolve("examplemodcsharp/AlphaHair") == 300 &&
          startup.ReadyDescriptors()->Resolve("examplemodcsharp/BetaHair") == 301,
          "descritores corretos sem instalar Terraria assets");

    startup.Reset();
    auto badCount = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1],
        [](int) { return 1; }, assetExists);
    check(!badCount.ready && !startup.ReadyDescriptors() &&
          startup.Reservation("examplemodcsharp").first == -1 &&
          badCount.error == "HairWorkerManagedExportCountMismatch",
          "erro de quantidade cancela todas as reservas");

    auto exception = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1],
        [](int) -> int { throw std::runtime_error("Mono callback exception"); },
        assetExists);
    check(!exception.ready && !startup.ReadyDescriptors() &&
          exception.error == "HairWorkerManagedExportException",
          "exceção no callback não deixa registro parcial");

    auto unknown = PrepareOneCompiledHairMod(
        &startup, "othermod", 2, argv[1], exported, assetExists);
    check(!unknown.ready && !startup.ReadyDescriptors() &&
          unknown.error == "HairHandoffManifestMissing",
          "manifesto de outro UID não é reaproveitado");

    slots.occupied.set(300);
    auto occupied = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1],
        [](int) { return 2; }, assetExists);
    check(!occupied.ready && occupied.firstType != 300 &&
          !startup.ReadyDescriptors(),
          "a reserva não pode ignorar slot já ocupado por JS");
    slots.occupied.reset(300);

    int calls = 0;
    auto lateJs = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1],
        [&](int first) {
            ++calls;
            slots.occupied.set(first);
            return 2;
        }, assetExists);
    check(!lateJs.ready && calls == 1 &&
          lateJs.phase == "hair-worker-slots-changed-before-seal" &&
          !startup.ReadyDescriptors(),
          "novo registro JS entre MonoVM e Seal cancela lote inteiro");
    slots.occupied.reset(300);

    auto recovered = PrepareOneCompiledHairMod(
        &startup, "examplemodcsharp", 2, argv[1], exported, assetExists);
    check(recovered.ready && recovered.firstType == 300,
          "pipeline pode recuperar sem reservar IDs antigos após falha");

    std::cout << "HAIR_MONOVM_WORKER_NATIVE_PIPELINE_PASS NO_APK\n";
}
