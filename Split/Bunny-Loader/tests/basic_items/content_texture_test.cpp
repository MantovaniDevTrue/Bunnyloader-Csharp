#include "experiments/csharp/ContentTexture.h"
#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char** argv) {
    using namespace bl::csharp;
    assert(argc == 2 || argc == 3);
    std::string path, error;
    assert(normalizeContentTexture("Content/Items/Test", &path, &error));
    assert(path == "Content/Items/Test.png" && error.empty());
    assert(normalizeContentTexture(path.c_str(), &path, &error));
    assert(path == "Content/Items/Test.png");
    for (const char* bad : {"", "/Content/a.png", "Content/../a.png", "Content/a.jpg",
                           "Content/", "Content//a.png", "Content/.png", "Content/a\n.png", "Content/a\\b.png"})
        assert(!normalizeContentTexture(bad, &path, &error) && !error.empty());
    assert(!normalizeContentTexture(nullptr, &path, &error));
    assert(!normalizeContentTexture(("Content/" + std::string(150, 'x')).c_str(), &path, &error));
    int items = 0, stems = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator(argv[1])) {
        if (file.path().extension() != ".cs") continue;
        std::ifstream input(file.path());
        const std::string source{std::istreambuf_iterator<char>(input), {}};
        std::vector<ParsedSourceType> types;
        assert(parseCSharpSource(source, &types, &error, false));
        for (const auto& type : types) {
            if (type.base != "BunnyItem") continue;
            const auto texture = type.properties.at("Texture");
            assert(normalizeContentTexture(texture.c_str(), &path, &error));
            assert(path == texture || path == texture + ".png");
            if (argc == 3) assert(std::filesystem::is_regular_file(std::filesystem::path(argv[1]) / path));
            if (path != texture) ++stems;
            ++items;
        }
    }
    assert(items > 0 && stems > 0);
    std::cout << "CONTENT_TEXTURE_SOURCE_REGISTRATION_CONTRACT_PASS items=" << items << " stems=" << stems << "\n";
}
