#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
int main(int argc,char** argv){
    assert(argc==2);std::ifstream file(argv[1]);std::string source((std::istreambuf_iterator<char>(file)),{}),error;
    std::vector<bl::csharp::ParsedSourceType> defs;
    assert(bl::csharp::parseCSharpSource(source,&defs,&error));
    assert(defs.size()==3);int conversions=0,entities=0;
    for(const auto& d:defs){if(d.base=="ModType")++conversions;else ++entities;}
    assert(conversions==1&&entities==2);
    defs.clear();
    assert(bl::csharp::parseCSharpSource("public class Standalone : BunnyLoader.Source.ModBiomeConversion {}",&defs,&error));
    assert(defs.size()==1&&defs[0].base=="ModType");
    puts("BIOME_CONVERSION_REAL_SOURCE_PARSER_SERVICE_ONLY_AND_MIXED_PASS");
}
