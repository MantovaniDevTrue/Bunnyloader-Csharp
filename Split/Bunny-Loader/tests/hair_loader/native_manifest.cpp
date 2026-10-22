#include "experiments/csharp/NativeHairManifest.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using bl::csharp::hair::Registry;
using bl::csharp::hair::ParseAndRegisterManifest;

static void require(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK: " << label << '\n';
}

int main() {
    const std::set<std::string> files = {
        "Content/Hairs/A.png", "Content/Hairs/B.png", "Content/Hairs/B_Alt.png"
    };
    const auto exists = [&](std::string_view path) {
        return files.contains(std::string(path));
    };
    const std::string header = "BUNNY_HAIR_MANIFEST_V1\n";
    const std::string a = "HAIR\t228\tmod-a\tA\tContent/Hairs/A.png\tContent/Hairs/A.png\t0\t1\n";
    const std::string b = "HAIR\t229\tmod-b\tB\tContent/Hairs/B.png\tContent/Hairs/B_Alt.png\t2\t0\n";
    std::string error;

    Registry rejects;
    require(!ParseAndRegisterManifest(header + a +
        "HAIR\t229\tmod-b\tB\t../bad.png\tContent/Hairs/B.png\t2\t0\n",
        &rejects, exists, &error) && rejects.Size() == 0,
        "lote com path inseguro nao registra metade");
    require(!ParseAndRegisterManifest(header + a +
        "HAIR\t231\tmod-b\tB\tContent/Hairs/B.png\tContent/Hairs/B.png\t2\t0\n",
        &rejects, exists, &error) && rejects.Size() == 0,
        "IDs nao sequenciais nao publicam registros");
    require(!ParseAndRegisterManifest(header + a + a, &rejects, exists, &error) &&
        rejects.Size() == 0, "duplicata no mesmo lote e rejeitada");
    require(!ParseAndRegisterManifest(header + a + "garbage", &rejects, exists, &error) &&
        rejects.Size() == 0, "ultima linha truncada e rejeitada");
    require(!ParseAndRegisterManifest(header + a + "HAIR\t229\tmod-b\tB\tContent/Hairs/B.png\tContent/Hairs/B.png\t2\t9\n",
        &rejects, exists, &error), "flag de criacao invalida e rejeitada");
    require(!ParseAndRegisterManifest(header + a + "HAIR\t229\tmod-b\tB\tContent/Hairs/B.png\tContent/Hairs/B.png\t9\t1\n",
        &rejects, exists, &error), "genero invalido e rejeitado");
    require(!ParseAndRegisterManifest("H0\n" + a, &rejects, exists, &error),
        "protocolo desatualizado e rejeitado");

    Registry registry;
    require(ParseAndRegisterManifest(header + a + b, &registry, exists, &error),
        "manifesto V1 e registrado de forma atomica");
    require(registry.Size() == 2 && registry.Resolve("mod-b/B") == 229 &&
        registry.ByType(229)->randomGender == bl::csharp::hair::RandomGender::Female &&
        !registry.ByType(229)->availableDuringCharacterCreation, "metadados tipados preservados");
    require(!ParseAndRegisterManifest(header + a, &registry, exists, &error) &&
        registry.Size() == 2, "replay do manifesto nao corrompe registro");
    registry.Reset();
    require(registry.Size() == 0, "reiniciar sessao limpa identidades");

    std::cout << "Native HairManifest interop contracts OK (NO APK).\n";
}
