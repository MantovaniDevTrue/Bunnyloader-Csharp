#include "experiments/csharp/ManagedBannerWorldFile.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace bl::csharp::banners;

int main() {
    char buffer[] = "/tmp/bunny-banner-world-XXXXXX";
    const char* folder = mkdtemp(buffer);
    assert(folder);
    const std::string world = std::string(folder) + "/first.wld";
    const std::string second = std::string(folder) + "/second.wld";
    WorldEntries saved{{"example/Slime", {177, 3, true}},
                       {"uninstalled/SpookySlime", {23, 1, false}}};
    WorldEntries target{{"unchanged/Entry", {7, 1, false}}};

    assert(readBannerWorldFile(world, false, &target) == DiskResult::Missing);
    assert(target.size() == 1 && target.contains("unchanged/Entry"));
    assert(writeBannerWorldFile(world, false, saved) == DiskResult::Ok);
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Ok);
    assert(target == saved);
    assert(writeBannerWorldFile(second, false, WorldEntries{}) == DiskResult::Ok);
    WorldEntries isolation;
    assert(readBannerWorldFile(second, false, &isolation) == DiskResult::Ok);
    assert(isolation.empty());
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Ok && target == saved);

    struct stat meta{};
    assert(stat(bannerSidecarPath(world).c_str(), &meta) == 0);
    assert(S_ISREG(meta.st_mode) && (meta.st_mode & 0077) == 0);

    assert(writeBannerWorldFile(world, true, WorldEntries{}) == DiskResult::UnsupportedCloud);
    assert(readBannerWorldFile(world, true, &target) == DiskResult::UnsupportedCloud);
    assert(target == saved);
    assert(writeBannerWorldFile("/tmp/../escape.wld", false, saved) == DiskResult::InvalidPath);
    assert(writeBannerWorldFile("relative.wld", false, saved) == DiskResult::InvalidPath);
    assert(writeBannerWorldFile("/tmp/world.wld.bak", false, saved) == DiskResult::InvalidPath);

    // Falha de validacao nao deve substituir um save valido.
    assert(writeBannerWorldFile(world, false,
        {{"example/Slime", {-1, 0, false}}}) == DiskResult::Corrupt);
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Ok);
    assert(target == saved);

    // Dump com checksum quebrado deve falhar sem zerar o progresso em memoria.
    {
        std::fstream file(bannerSidecarPath(world), std::ios::in | std::ios::out | std::ios::binary);
        assert(file);
        file.seekp(8);
        char overwritten = '\x7f';
        file.write(&overwritten, 1);
        assert(file);
    }
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Corrupt);
    assert(target == saved);
    assert(writeBannerWorldFile(world, false, saved) == DiskResult::Ok);
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Ok);

    // Link simbolico para save hostil nao pode ser seguido.
    assert(unlink(bannerSidecarPath(world).c_str()) == 0);
    assert(symlink(bannerSidecarPath(second).c_str(), bannerSidecarPath(world).c_str()) == 0);
    assert(readBannerWorldFile(world, false, &target) == DiskResult::IoError);
    assert(target == saved);
    assert(writeBannerWorldFile(world, false, saved) == DiskResult::Ok);
    assert(readBannerWorldFile(world, false, &target) == DiskResult::Ok);
    assert(readBannerWorldFile(second, false, &isolation) == DiskResult::Ok);
    assert(isolation.empty());

    assert(unlink(bannerSidecarPath(world).c_str()) == 0);
    assert(unlink(bannerSidecarPath(second).c_str()) == 0);
    assert(rmdir(folder) == 0);
    std::cout << "Banner disk sidecar PASS (atomic, world isolation, cloud, corruption, symlink)\n";
}
