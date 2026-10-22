#include "experiments/csharp/NativeHairSaveState.h"
#include "experiments/csharp/NativeHairSidecar.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace bl::csharp::hair;

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK " << label << '\n';
}

int main() {
    Registry registry;
    std::string error;
    check(registry.RegisterBatch({
        {"sample", "ExampleHair", "Content/Hairs/ExampleHair.png",
         "Content/Hairs/ExampleHair_Alt.png"}
    }, [](std::string_view) { return true; }, &error),
    "mod hair registrado para testes de save");

    int playerHair = 228;
    std::string key;
    {
        VanillaSaveGuard save(&playerHair, registry);
        check(save.Modified() && playerHair == 0,
              "arquivo vanilla nao recebe indice superior a 227");
        key = save.StableKey();
        check(key == "H1|sample/ExampleHair",
              "chave do player guarda UID e classe");
        VanillaSaveGuard nested(&playerHair, registry);
        check(!nested.Modified(), "save aninhado nao altera hair 0 do primeiro");
    }
    check(playerHair == 228, "hair original restaurado ao fim do escopo");
    try {
        VanillaSaveGuard save(&playerHair, registry);
        check(playerHair == 0, "save antes de excecao em andamento");
        throw std::runtime_error("original player save failed");
    } catch (const std::runtime_error&) {}
    check(playerHair == 228, "exception unwinding restaura player");

    int vanillaHair = 12;
    {
        VanillaSaveGuard save(&vanillaHair, registry);
        check(!save.Modified() && vanillaHair == 12, "save vanilla intocado");
    }
    check(vanillaHair == 12, "vanilla preservado no retorno");
    {
        VanillaSaveGuard save(nullptr, registry);
        check(!save.Modified(), "ponteiro nulo nao trava nem salva");
    }

    char folder[] = "/tmp/bunny-csharp-hair-save-XXXXXX";
    char* created = mkdtemp(folder);
    check(created != nullptr, "diretorio de teste temporario criado");
    const std::string player = std::string(created) + "/player.plr";
    const std::string oldJson = player + ".bl.json";
    { std::ofstream json(oldJson); json << "{\"otherMod\":{\"value\":1}}"; }
    check(WriteNativeHairSidecar(player, key, &error), "sidecar escrito atomicamente");

    std::string actual;
    check(ReadNativeHairSidecar(player, &actual, &error) == SidecarRead::Found &&
          actual == key, "sidecar lido com versao H1 intacta");
    std::ifstream json(oldJson);
    std::string previous;
    std::getline(json, previous);
    json.close();
    check(previous == "{\"otherMod\":{\"value\":1}}",
          "Sidecar C# nao altera .plr.bl.json de outros mods");
    PendingRestore pending(actual);
    check(!pending.Apply(&playerHair, registry, false) && playerHair == 228 &&
          pending.Key() == key, "espera texturas prontas antes da escrita");
    playerHair = 0;
    check(pending.Apply(&playerHair, registry, true) && playerHair == 228 &&
          pending.Key().empty(), "restaura a aparencia apos instalar textura");

    Registry withoutMod;
    PendingRestore absent(actual);
    playerHair = 37;
    check(absent.Apply(&playerHair, withoutMod, true) && playerHair == 0,
          "mod desativado preserva personagem com cabelo vanilla 0");
    check(ReadNativeHairSidecar(player, &actual, &error) == SidecarRead::Found,
          "sidecar permanece disponivel para futura reativacao do mod");

    check(!WriteNativeHairSidecar("../outside.plr", key, &error),
          "path traversal sem raiz bloqueado");
    check(!WriteNativeHairSidecar(player, "H1|../danger", &error),
          "chave maliciosa bloqueada");
    check(!WriteNativeHairSidecar(player, "H1|sample/ExampleHair\nmalicious", &error),
          "insercao de nova linha bloqueada");

    const std::string separate = player + ".bl.csharp-hair";
    { std::ofstream broken(separate, std::ios::trunc); broken << "invalid"; }
    check(ReadNativeHairSidecar(player, &actual, &error) == SidecarRead::Invalid &&
          actual.empty(), "sidecar corrompido nao altera cabelo");
    check(WriteNativeHairSidecar(player, key, &error),
          "arquivo corrompido pode ser substituido pelo mod valido");

    const std::string outside = std::string(created) + "/remote";
    { std::ofstream source(outside); source << "forged"; }
    unlink(separate.c_str());
    check(symlink(outside.c_str(), separate.c_str()) == 0, "symlink de ataque criado");
    check(ReadNativeHairSidecar(player, &actual, &error) != SidecarRead::Found,
          "leitura rejeita symlink no sidecar");
    check(WriteNativeHairSidecar(player, key, &error),
          "rename atomico substitui symlink sem seguir seu alvo");
    std::ifstream outsideFile(outside);
    std::string outsideContents;
    std::getline(outsideFile, outsideContents);
    outsideFile.close();
    check(outsideContents == "forged", "symlink nao sobrescreve alvo externo");

    unlink(separate.c_str());
    unlink(outside.c_str());
    unlink(oldJson.c_str());
    rmdir(folder);
    std::cout << "HairSaveState/Sidecar contracts OK. NO APK.\n";
}
