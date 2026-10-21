#include "experiments/csharp/NativeCommandManifest.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    const std::string dll = "/tmp/bunny-csharp-command-manifest-test.dll";
    { std::ofstream output(dll, std::ios::binary | std::ios::trunc);
      output << "compiled-source-fixture";
    }
    const std::string hash = auto_cache::fileHash(dll, kSourceMaxAssemblyBytes);
    assert(hash.size() == 64);
    const std::string manifest = "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\nDESC\tSmoke.Heal\t43757261\t1\nALIAS\tSmoke.Heal\tcurar\t1\n";
    std::vector<NativeCommandSpec> specs;
    std::string error;
    assert(parseNativeCommandManifest(manifest, dll, {"Smoke.Heal"}, &specs, &error));
    assert(specs.size() == 2 && specs[0].name == "heal" && specs[1].name == "curar");
    assert(specs[0].canonical && !specs[1].canonical && specs[0].description == "Cura");
    NativeCommandMailbox mailbox;
    assert(mailbox.registerMod("smoke", specs));
    auto help = mailbox.visibleHelp(0);
    assert(help.size() == 1 && help[0].first == "heal" && help[0].second == "Cura");
    assert(mailbox.registerMod("other", {{"heal", 1, "Restaura", true}}));
    help = mailbox.visibleHelp(0);
    assert(help.size() == 2 && help[0].first == "other:heal" &&
           help[1].first == "smoke:heal");
    assert(mailbox.unregisterMod("other"));
    assert(mailbox.visibleHelp(0).size() == 1);
    assert(mailbox.submit("/curar 5", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(!parseNativeCommandManifest(manifest, dll, {"Wrong.Class"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\nDESC\tSmoke.Heal\tGG\t1\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\nDESC\tSmoke.Heal\t0A\t1\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\nALIAS\tSmoke.Heal\tHEAL\t1\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\nALIAS\tSmoke.Heal\tcurar\t4\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + std::string(64, 'a') +
        "\nCLASSES\t1\nCMD\tSmoke.Heal\theal\t1\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    assert(!parseNativeCommandManifest(
        "BUNNY_COMMANDS_V1\nASSEMBLY_SHA256\t" + hash +
        "\nCLASSES\t2\nCMD\tSmoke.Heal\theal\t1\n",
        dll, {"Smoke.Heal"}, &specs, &error));
    std::remove(dll.c_str());
    std::cout << "BUNNY_CSHARP_NATIVE_COMMAND_MANIFEST=PASS" << std::endl;
}
