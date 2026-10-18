#include "experiments/csharp/ContentTexture.h"
#include "experiments/csharp/SourceParser.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
int main(int argc,char** argv) {
    assert(argc==2);std::set<std::string> names;int files=0;
    for(const auto& file:std::filesystem::recursive_directory_iterator(argv[1])) {
        if(file.path().extension()!=".cs")continue;
        ++files;std::ifstream input(file.path());
        std::string source{std::istreambuf_iterator<char>(input),{}};
        std::vector<bl::csharp::ParsedSourceType> types;std::string error;
        assert(bl::csharp::parseCSharpSource(source,&types,&error,false));
        for(const auto& type:types) {
            assert(type.base=="BunnyItem"&&names.insert(type.name).second);
            assert(!type.properties.at("DisplayName").empty());std::string texture;
            assert(bl::csharp::normalizeContentTexture(type.properties.at("Texture").c_str(),&texture,&error));
            std::ifstream image(std::filesystem::path(argv[1])/texture,std::ios::binary);assert(image.good());
            unsigned char header[8]{};image.read(reinterpret_cast<char*>(header),8);
            const unsigned char png[]={137,80,78,71,13,10,26,10};
            assert(std::equal(std::begin(header),std::end(header),std::begin(png)));
        }
    }
    assert(files==4&&names==std::set<std::string>({"ProbeWorkbench","ProbePlatformA","ProbePlatformB"}));
    std::cout<<"ANDROID_PROBE_EARLY_SOURCE_REGISTRATION_AND_TEXTURE_CONTRACT_PASS\n";
}
