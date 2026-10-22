#include "experiments/csharp/SourceParser.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
    std::cout << "OK: " << label << '\n';
}

static std::string parseError(const std::string& source, bool* loaded = nullptr) {
    std::vector<bl::csharp::ParsedSourceType> types;
    std::string error;
    bool accepted = bl::csharp::parseCSharpSource(source, &types, &error, true);
    if (loaded) *loaded = accepted;
    return error;
}

int main() {
    bool accepted = false;
    std::string error = parseError(
        "namespace Demo; public sealed class ExampleHair : ModHair { }", &accepted);
    check(!accepted && error.find("ModHair C#") != std::string::npos,
          "ModHair sem hook nativo nao recebe registro falso");

    error = parseError(
        "namespace Demo; public sealed class ExampleHair : BunnyHair { }", &accepted);
    check(!accepted && error.find("ModHair C#") != std::string::npos,
          "alias BunnyHair tambem fica bloqueado");

    error = parseError(
        "namespace Demo; public sealed class SampleItem : BunnyItem { "
        "public override void SetDefaults() { DisplayName = \"Test\"; Texture = \"Content/Items/Test\"; } "
        "} public sealed class SampleHair : ModHair { }", &accepted);
    check(!accepted && error.find("ModHair C#") != std::string::npos,
          "mod misto nao ignora penteado sem suporte");

    error = parseError(
        "namespace Demo; // class ExampleHair : ModHair { }\n"
        "public sealed class SampleItem : BunnyItem { "
        "public override void SetDefaults() { DisplayName = \"Test\"; Texture = \"Content/Items/Test\"; } "
        "}", &accepted);
    check(accepted && error.empty(), "comentario nao bloqueia item C# suportado");

    std::cout << "HairLoader parser preflight OK. Nenhum APK gerado.\n";
}
