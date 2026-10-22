#include "experiments/csharp/ManagedBannerWorldSession.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace bl::csharp::banners;

int main() {
    char temp[] = "/tmp/bunny-banner-session-XXXXXX";
    const char* folder = mkdtemp(temp);
    assert(folder);
    const std::string one = std::string(folder) + "/world1.wld";
    const std::string two = std::string(folder) + "/world2.wld";
    WorldSession session;
    WorldEntries data;
    const WorldEntries states{{"example/Slime", {99, 2, true}},
                              {"missing/OldSlime", {45, 1, false}}};

    assert(session.begin(1, one, false) == DiskResult::Missing);
    assert(session.view(1, &data) && data.empty());
    assert(!session.view(2, &data));
    assert(!session.commit(2, true, states));
    assert(!session.commit(1, false, states));
    assert(session.commit(1, true, states));
    assert(session.status() == DiskResult::Ok);
    assert(session.begin(1, one, false) == DiskResult::Ok);
    assert(session.view(1, &data) && data == states);

    // Mudanca de sessao, ID ou caminho nunca importa contadores de outro mundo.
    assert(session.begin(2, two, false) == DiskResult::Missing);
    assert(session.view(2, &data) && data.empty());
    assert(!session.view(1, &data));
    assert(!session.commit(1, true, states));
    assert(session.begin(3, one, false) == DiskResult::Ok);
    assert(session.view(3, &data) && data == states);
    assert(session.begin(4, one, true) == DiskResult::UnsupportedCloud);
    assert(!session.view(4, &data));
    assert(!session.commit(4, true, states));
    assert(session.begin(5, one, false) == DiskResult::Ok);

    // Um arquivo corrompido nunca pode ser substituido automaticamente
    // por um estado inicial zero nem por dados de outro mundo.
    const std::string sidecar = bannerSidecarPath(one);
    {
        FILE* file = fopen(sidecar.c_str(), "wb");
        assert(file);
        const char damaged[] = "WRONG";
        assert(fwrite(damaged, 1, sizeof(damaged), file) == sizeof(damaged));
        assert(fclose(file) == 0);
    }
    assert(session.begin(6, one, false) == DiskResult::Corrupt);
    assert(!session.view(6, &data));
    assert(!session.commit(6, true, states));
    assert(session.status() == DiskResult::Corrupt);
    session.end();
    assert(!session.view(6, &data));
    assert(session.status() == DiskResult::InvalidPath);

    assert(unlink(sidecar.c_str()) == 0);
    assert(rmdir(folder) == 0);
    std::cout << "Banner world session PASS (epochs, cloud, ownership, corruption quarantine)\n";
}
