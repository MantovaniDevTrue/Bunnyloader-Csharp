#include "experiments/csharp/SourceParser.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    for (int i = 1; i < argc; ++i) {
        std::ifstream input(argv[i], std::ios::binary);
        if (!input) {
            std::cerr << "Nao foi possivel abrir: " << argv[i] << "\n";
            return 2;
        }
        std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        std::vector<bl::csharp::ParsedSourceType> definitions;
        std::string error;
        if (!bl::csharp::parseCSharpSource(source, &definitions, &error, false)) {
            std::cerr << argv[i] << ": " << error << "\n";
            return 1;
        }
    }
    std::cout << "CSHARP_HOOK_QA_NATIVE_TOKENIZER=PASS arquivos=" << argc - 1 << "\n";
    return 0;
}
