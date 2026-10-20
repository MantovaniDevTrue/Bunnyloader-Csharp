#include "experiments/csharp/ManagedGoreDiscovery.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using bl::csharp::gore::Source;
using bl::csharp::gore::discover;

struct Fixture {
    fs::path root;

    Fixture() {
        char name[] = "/tmp/bunny-gore-XXXXXX";
        char* path = mkdtemp(name);
        assert(path);
        root = path;
        fs::create_directories(root / "content");
    }

    ~Fixture() {
        std::error_code error;
        fs::remove_all(root, error);
    }

    void png(const std::string& path, unsigned width = 12, unsigned height = 16) {
        const fs::path file = root / "content" / path;
        fs::create_directories(file.parent_path());
        std::array<unsigned char, 33> value = {
            137, 80, 78, 71, 13, 10, 26, 10,
            0, 0, 0, 13, 'I', 'H', 'D', 'R',
            0, 0, 0, 12, 0, 0, 0, 16,
            8, 6, 0, 0, 0, 0, 0, 0, 0
        };
        for (int i = 0; i < 4; ++i) {
            value[16 + i] = static_cast<unsigned char>(width >> (24 - 8 * i));
            value[20 + i] = static_cast<unsigned char>(height >> (24 - 8 * i));
        }
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(value.data()), value.size());
        assert(stream.good());
    }
};

int main() {
    std::vector<Source> result;
    std::string error;
    {
        Fixture mod;
        mod.png("Content/Gores/Head.png", 12, 18);
        mod.png("Assets/Textures/Gores/Leaf.png", 9, 9);
        mod.png("Content/Gores/Ignore.PNG");
        mod.png("Content/Tiles/Tile.png");
        // Uma classe de conteudo em outra pasta pode possuir o seu gore.
        mod.png("Content/NPCs/Gores/Arm.png", 14, 7);
        assert(discover(mod.root.string(), &result, &error));
        assert(error.empty());
        assert(result.size() == 3);
        assert(result[0].name == "Leaf");
        assert(result[1].name == "Head");
        assert(result[2].name == "Arm");
        assert(result[1].width == 12 && result[1].height == 18);
        assert(result[1].relativePath == "Content/Gores/Head.png");
    }
    {
        Fixture mod;
        mod.png("Content/Gores/Head.png");
        mod.png("Assets/Textures/Gores/Head.png");
        const auto snapshot = result;
        assert(!discover(mod.root.string(), &result, &error));
        assert(error.find("duplicado") != std::string::npos);
        assert(result.size() == snapshot.size());
    }
    {
        Fixture mod;
        mod.png("Content/Gores/Invalid.png", 0, 15);
        assert(!discover(mod.root.string(), &result, &error));
        assert(error.find("dimensoes") != std::string::npos);
    }
    {
        Fixture mod;
        const fs::path file = mod.root / "content" / "Content/Gores/Bad.png";
        fs::create_directories(file.parent_path());
        std::ofstream(file) << "not a png";
        assert(!discover(mod.root.string(), &result, &error));
        assert(error.find("invalido") != std::string::npos);
    }
    {
        Fixture mod;
        Fixture external;
        external.png("Gores/Outside.png");
        fs::create_directories(mod.root / "content" / "Content");
        fs::create_symlink(external.root / "content" / "Gores",
                           mod.root / "content" / "Content" / "Gores");
        assert(discover(mod.root.string(), &result, &error));
        assert(result.empty());
    }
    {
        Fixture mod;
        for (int i = 0; i < 257; ++i)
            mod.png("Gores/Gore" + std::to_string(i) + ".png");
        assert(!discover(mod.root.string(), &result, &error));
        assert(error.find("256") != std::string::npos);
    }
    {
        Fixture mod;
        assert(discover(mod.root.string(), &result, &error));
        assert(result.empty());
    }
    {
        // Exercita o mesmo pacote ExampleMod C# distribuido ao modder,
        // com os dois PNGs originais do ExampleModBunny.
        const std::string sample = "samples/ExampleModCSharp/Playable";
        assert(fs::exists(sample + "/content/Content/Gores/ExampleDroplet.png"));
        assert(discover(sample, &result, &error));
        assert(result.size() == 2);
        assert(result[0].name == "ExampleBoss_GoreFront");
        assert(result[1].name == "ExampleDroplet");
    }
    {
        Fixture mod;
        Fixture external;
        mod.png("Content/Gores/Protected.png", 12, 18);
        external.png("Gores/Escape.png", 12, 18);
        std::vector<Source> found;
        assert(discover(mod.root.string(), &found, &error));
        assert(found.size() == 1);
        std::vector<unsigned char> contents;
        assert(bl::csharp::gore::readOwnedPng(found[0], &contents, &error));
        assert(contents.size() == 33);
        const fs::path original = mod.root / "content/Content/Gores/Protected.png";
        fs::remove(original);
        fs::create_symlink(external.root / "content/Gores/Escape.png", original);
        assert(!bl::csharp::gore::readOwnedPng(found[0], &contents, &error));
        assert(error.find("link simbolico") != std::string::npos);
        assert(contents.size() == 33);
    }
    std::puts("CSHARP_GORE_DISCOVERY_PASS");
}
