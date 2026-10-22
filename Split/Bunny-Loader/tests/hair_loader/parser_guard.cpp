#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

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

static bool parsed(const std::string& source,
                   std::vector<bl::csharp::ParsedSourceType>* types,
                   std::string* error) {
    types->clear();
    error->clear();
    return bl::csharp::parseCSharpSource(source, types, error, true);
}

int main() {
    using namespace bl::csharp;
    std::vector<ParsedSourceType> types;
    std::string error;

    check(parsed("namespace Demo; public sealed class ExampleHair : ModHair { }",
                 &types, &error) &&
          types.size() == 1 && types[0].base == "ModHair" &&
          types[0].namespaceName == "Demo" && types[0].name == "ExampleHair",
          "ModHair tem registro de metadados com namespace e classe");

    check(parsed("namespace Demo; public sealed class ExampleHair : BunnyHair { }",
                 &types, &error) &&
          types.size() == 1 && types[0].base == "BunnyHair",
          "alias BunnyHair usa o mesmo preflight");

    check(parsed(
          "namespace Demo; public sealed class SampleItem : BunnyItem { "
          "public override void SetDefaults() { DisplayName = \"Test\"; Texture = \"Content/Items/Test\"; } "
          "} public sealed class SampleHair : ModHair { "
          "public override string Texture => \"Content/Hairs/SampleHair\"; "
          "public override bool AvailableDuringCharacterCreation => true; }",
          &types, &error) &&
          types.size() == 2 && types[0].base == "BunnyItem" &&
          types[1].base == "ModHair",
          "mod misto tem duas classes detectadas, incluindo cabelo");

    check(sourceContentDomain("ModHair") == "HAIR" &&
          sourceContentDomain("BunnyHair") == "HAIR" &&
          sourceIdentityForBase("BunnyHair", "SampleHair") == "HAIR:SampleHair" &&
          sourceIdentityForWire("HAIR", "SampleHair") == "HAIR:SampleHair",
          "identidade do cabelo nao colide com dominio ITEM");

    check(parsed(
          "namespace Demo; // class ExampleHair : ModHair { }\n"
          "public sealed class SampleItem : BunnyItem { "
          "public override void SetDefaults() { DisplayName = \"Test\"; Texture = \"Content/Items/Test\"; } "
          "}", &types, &error) &&
          types.size() == 1 && types[0].base == "BunnyItem",
          "comentario nao cria classe ModHair fantasma");

    check(!parsed("namespace Demo; public sealed class BrokenHair : ModHair {",
                  &types, &error) &&
          error.find("fechamento") != std::string::npos,
          "classe mal formada nao passa silenciosamente");

    std::cout << "Parser HairLoader: somente descoberta de classes; "
              << "SourceLoader bloqueia ativacao ate o ciclo completo. NO APK.\n";
}
