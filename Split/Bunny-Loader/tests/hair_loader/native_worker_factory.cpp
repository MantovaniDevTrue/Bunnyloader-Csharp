#include "experiments/csharp/NativeHairStartup.h"
#include "experiments/csharp/NativeHairSlotMailbox.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace bl::csharp::hair;
using namespace std::chrono_literals;

namespace bl::csharp::hair {
std::atomic<int> mockIl2CppAccesses{0};
NativeHairSlotSnapshot ReadNativeHairOccupiedSlots() {
    ++mockIl2CppAccesses;
    NativeHairSlotSnapshot snapshot;
    snapshot.valid = true;
    snapshot.occupied.set(228);
    snapshot.occupied.set(229);
    return snapshot;
}
}

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK " << label << '\n';
}

int main() {
    auto startup = MakeNativeHairStartupForWorker();
    NativeHairStartup::Block reserved;
    std::string error;
    std::atomic<bool> finished{false};
    std::atomic<bool> good{false};
    std::thread mono([&] {
        if (!startup.Begin("mod-a", 1, &reserved, &error)) {
            finished.store(true);
            return;
        }
        const std::string manifest =
            "BUNNY_HAIR_MANIFEST_V1\n"
            "HAIR\t230\tmod-a\tTestHair\tContent/Hairs/TestHair.png\tContent/Hairs/TestHair.png\t0\t1\n";
        if (!startup.CommitManifest("mod-a", manifest,
            [](std::string_view) { return true; }, &error)) {
            finished.store(true);
            return;
        }
        good.store(startup.Seal(&error));
        finished.store(true);
    });

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
        PumpNativeHairSlotsOnGameThread();
        std::this_thread::sleep_for(1ms);
    }
    mono.join();
    check(good.load() && reserved.first == 230 &&
          startup.ReadyDescriptors() &&
          startup.ReadyDescriptors()->Resolve("mod-a/TestHair") == 230,
          "worker factory conclui Begin + Commit + Seal pela thread do jogo");
    check(mockIl2CppAccesses.load() == 2,
          "apenas dois snapshots nativos para dois pedidos reais");
    for (int i = 0; i < 10000; ++i)
        PumpNativeHairSlotsOnGameThread();
    check(mockIl2CppAccesses.load() == 2,
          "sem worker pendente, game update nao realiza leitura IL2CPP");

    startup.Reset();
    CancelNativeHairSlotRequests();
    check(!startup.ReadyDescriptors(),
          "descarregamento invalida os descritores antes dos hooks");

    std::cout << "HairStartup worker-to-game factory PASS, 2 native reads. NO APK.\n";
}
