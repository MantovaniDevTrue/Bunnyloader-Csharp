#include "experiments/csharp/NativeHairStartup.h"
#include "experiments/csharp/NativeHairHandoff.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace bl::csharp::hair;

static void require(bool value, const char* label) {
    if (!value) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK " << label << '\n';
}

int main(int argc, char** argv) {
    require(argc == 2, "arquivo de manifesto gerado pelo compilador C# informado");
    std::string managed, error;
    require(ReadNativeHairManifest(argv[1], "examplemodcsharp", &managed, &error),
            "arquivo C# aberto pelo leitor nativo seguro com UID");
    require(managed.size() > 50 && managed.size() < 512 * 1024,
            "payload limitado ao tamanho suportado pelo host C++");
    require(!ReadNativeHairManifest(argv[1], "../examplemodcsharp", &managed, &error) &&
            error == "HairHandoffUnsafePathOrUid",
            "UID com traversal nunca forma um filename");
    require(ReadNativeHairManifest(argv[1], "examplemodcsharp", &managed, &error),
            "UID valido continua legivel apos rejeicao");

    NativeHairSlotSnapshot snapshot;
    snapshot.valid = true;
    for (int id = Registry::kVanillaCount; id < 300; ++id)
        snapshot.occupied.set(static_cast<size_t>(id));
    int reads = 0;
    NativeHairStartup startup([&] {
        ++reads;
        return snapshot;
    });
    NativeHairStartup::Block reserved;
    require(startup.Begin("examplemodcsharp", 2, &reserved, &error) &&
            reserved.first == 300 && reserved.count == 2,
            "reserva global C++ escolhe exatamente IDs 300-301");
    require(!startup.ReadyDescriptors(), "mod não é publicado durante exportacao C#");
    const auto assetExists = [](std::string_view value) {
        return value.starts_with("Content/Hairs/") && value.ends_with(".png");
    };
    require(startup.CommitManifest("examplemodcsharp", managed, assetExists, &error),
            "parser C++ reconhece bytes reais gerados pelas classes C#");
    require(!startup.ReadyDescriptors(), "registro permanece privado ate segunda leitura");
    require(startup.Seal(&error) && reads == 2,
            "segunda leitura das tabelas autoriza publicacao");

    const Registry* entries = startup.ReadyDescriptors();
    require(entries && entries->Size() == 2 &&
            entries->Resolve("examplemodcsharp/AlphaHair") == 300 &&
            entries->Resolve("examplemodcsharp/BetaHair") == 301,
            "identidades C# mantidas sem indices divergentes");
    const Entry* alpha = entries->ByType(300);
    const Entry* beta = entries->ByType(301);
    require(alpha && beta &&
            alpha->texture == "Content/Hairs/AlphaHair.png" &&
            alpha->altTexture == alpha->texture,
            "textura alternativa ausente usa main no wire compilado");
    require(beta->texture == "Content/Hairs/BetaHair.png" &&
            beta->altTexture == "Content/Hairs/BetaHair_Alt.png" &&
            beta->randomGender == RandomGender::Female &&
            !beta->availableDuringCharacterCreation,
            "gênero, criação e textura com chapéu sobrevivem à ponte C#->C++");

    startup.Reset();
    NativeHairStartup foreign([&] { return snapshot; });
    require(foreign.Begin("anothermod", 2, &reserved, &error),
            "segundo mod pode reservar faixa equivalente em outra sessão");
    require(!foreign.CommitManifest("anothermod", managed, assetExists, &error) &&
            error == "HairStagingForeignModIdentity" &&
            !foreign.ReadyDescriptors(),
            "não aceitar manifesto produzido para outro UID");
    std::cout << "CSharp compiler file -> native IL2CPP startup contracts PASS (NO APK).\n";
}
