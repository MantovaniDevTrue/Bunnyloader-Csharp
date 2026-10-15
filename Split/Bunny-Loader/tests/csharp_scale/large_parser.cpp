#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceLimits.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    static_assert(kSourceMaxFiles >= 4096);
    static_assert(kSourceMaxTypes >= 8192);
    static_assert(kSourceMaxCategory >= 4096);
    static_assert(kSourceMaxTotalBytes >= 32*1024*1024);
    std::string source;
    for(int i=0;i<1200;++i) {
        source += "public sealed class ScaleItem" + std::to_string(i) +
            " : BunnyItem { public override void SetDefaults() { "
            "MaxStack = 9999; Value = 100; } }\n";
    }
    source.append(300*1024,' ');
    if(source.size() <= 256*1024 || source.size() > kSourceMaxFileBytes)return 1;
    std::vector<ParsedSourceType> types;
    std::string error;
    if(!parseCSharpSource(source,&types,&error,false)) {
        std::cerr<<error<<'\n';
        return 2;
    }
    if(types.size()!=1200 || types.front().name!="ScaleItem0" ||
        types.back().name!="ScaleItem1199")return 3;
    source.append(kSourceMaxFileBytes-source.size()+1,' ');
    if(parseCSharpSource(source,&types,&error,false))return 4;
    std::cout<<"BUNNY_CSHARP_LARGE_PARSER=PASS classes=1200"<<'\n';
    return 0;
}
