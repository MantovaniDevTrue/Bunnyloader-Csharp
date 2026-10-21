#include "experiments/csharp/NativeWorldStore.h"

#include <cassert>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

using bl::csharp::worldsave::NativeWorldStore;
using bl::csharp::worldsave::Result;

static uint32_t crc32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (auto b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}

static std::vector<uint8_t> document(int value) {
    std::vector<uint8_t> data{'B', 'W', 'C', '1', 1, 0};
    const std::string key = "00000000-0000-4000-8000-000000000001/Example.Progression";
    data.push_back(static_cast<uint8_t>(key.size()));
    data.push_back(0);
    data.insert(data.end(), key.begin(), key.end());
    data.push_back(1);
    data.push_back(0);
    const std::string field = "count";
    data.push_back(static_cast<uint8_t>(field.size()));
    data.push_back(0);
    data.insert(data.end(), field.begin(), field.end());
    data.push_back(2);
    for (int i = 0; i < 4; ++i)
        data.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (i * 8)));
    uint32_t checksum = crc32(data);
    for (int i = 0; i < 4; ++i) data.push_back(static_cast<uint8_t>(checksum >> (i * 8)));
    return data;
}

int main() {
    char folder[] = "/tmp/bunny-worldsave-XXXXXX";
    const char* root = ::mkdtemp(folder);
    assert(root);
    const std::string worldA = std::string(root) + "/first.wld";
    const std::string worldB = std::string(root) + "/second.wld";
    {
        std::ofstream(worldA) << "game world A";
        std::ofstream(worldB) << "game world B";
    }
    NativeWorldStore store;
    assert(!store.beginWorld(1, "../escape.wld", false));
    assert(!store.beginWorld(1, std::string(root) + "/../escape.wld", false));
    assert(!store.beginWorld(1, worldA, true));
    assert(store.beginWorld(1, worldA, false));

    std::vector<uint8_t> loaded;
    assert(store.read(1, &loaded) == Result::Missing);
    assert(store.write(1, document(42)) == Result::Ok);
    assert(store.read(1, &loaded) == Result::Ok && loaded == document(42));

    auto invalid = document(100);
    invalid[invalid.size() - 1] ^= 1;
    assert(store.write(1, invalid) == Result::Corrupt);
    assert(store.read(1, &loaded) == Result::Ok && loaded == document(42));

    store.endWorld(1);
    assert(store.write(1, document(10)) == Result::InvalidSession);
    assert(store.beginWorld(2, worldB, false));
    assert(store.write(1, document(99)) == Result::InvalidSession);
    assert(store.read(2, &loaded) == Result::Missing);
    assert(store.write(2, document(17)) == Result::Ok);
    assert(store.beginWorld(3, worldA, false));
    assert(store.read(3, &loaded) == Result::Ok && loaded == document(42));
    store.endWorld(2);
    assert(store.active(3));

    // Um sidecar suspeito jamais pode ser substituído por dados novos.
    const std::string sidecar = worldA + ".csharp.bwc";
    {
        std::ofstream corrupt(sidecar, std::ios::binary | std::ios::trunc);
        corrupt << "invalid";
    }
    assert(store.read(3, &loaded) == Result::Corrupt && loaded.empty());
    assert(store.write(3, document(200)) == Result::Corrupt);
    {
        std::ifstream input(sidecar, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(input)), {});
        assert(bytes == "invalid");
    }
    assert(::unlink(sidecar.c_str()) == 0);
    assert(::symlink(worldB.c_str(), sidecar.c_str()) == 0);
    assert(store.read(3, &loaded) == Result::IoError);
    assert(store.write(3, document(200)) == Result::IoError);
    assert(::unlink(sidecar.c_str()) == 0);

    // Concorrência usa lock por sessão: nunca publica uma gravação parcial.
    std::atomic<bool> failed{false};
    std::thread first([&]() {
        for (int i = 0; i < 100; ++i)
            if (store.write(3, document(i)) != Result::Ok) failed = true;
    });
    std::thread second([&]() {
        for (int i = 100; i < 200; ++i)
            if (store.write(3, document(i)) != Result::Ok) failed = true;
    });
    first.join();
    second.join();
    assert(!failed);
    assert(store.read(3, &loaded) == Result::Ok);
    assert(NativeWorldStore::validDocument(loaded.data(), loaded.size()));

    // O mundo original não foi modificado pelo sidecar.
    std::ifstream check(worldA);
    std::string worldText;
    std::getline(check, worldText);
    assert(worldText == "game world A");

    assert(::unlink(sidecar.c_str()) == 0);
    assert(::unlink((worldB + ".csharp.bwc").c_str()) == 0);
    assert(::unlink(worldA.c_str()) == 0);
    assert(::unlink(worldB.c_str()) == 0);
    assert(::rmdir(root) == 0);
    std::puts("WORLD_NATIVE_STORE_PASS");
    return 0;
}
