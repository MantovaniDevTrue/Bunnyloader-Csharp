#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/ManagedSourceClassCount.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace bl::csharp;
    std::vector<ParsedSourceType> definitions;
    std::string error;
    assert(parseCSharpSource(R"(
        // class Fake : ModType { }
        namespace Example;
        public abstract class SettingsCategory : BunnyLoader.Source.ModType {
            protected override void Register() => ModTypeLookup<SettingsCategory>.Register(this);
        }
        public sealed class Settings : SettingsCategory { }
    )", &definitions, &error));
    assert(definitions.size() == 1 && definitions[0].base == "ModType");
    SourceModStatus service; service.managedContentDeclarations = 1;
    auto status = [](int total, const std::string& managed) {
        return "state=finished\nmodId=sample\nphase=compile-verified\nassemblyReady=1\nexecutedTypes=" +
            std::to_string(total) + "\nmanagedContentTypes=" + managed + "\n";
    };
    assert(managedSourceCompileFinished(status(2, "2"), "sample", service));
    assert(!managedSourceCompileFinished(status(2, "0"), "sample", service));
    assert(!managedSourceCompileFinished(status(0, "0"), "sample", service));
    assert(!managedSourceCompileFinished(status(2, "3"), "sample", service));
    assert(!managedSourceCompileFinished(status(2, "2x"), "sample", service));
    assert(!managedSourceCompileFinished(status(2, "99999"), "sample", service));
    assert(!managedSourceCompileFinished(status(2, "2") + "managedContentTypes=1\n", "sample", service));
    service.items = 1;
    assert(managedSourceCompileFinished(status(3, "2"), "sample", service));
    assert(!managedSourceCompileFinished(status(4, "2"), "sample", service));
    definitions.clear();
    assert(!parseCSharpSource("// class Fake : ModType {}\nclass Helper {}", &definitions, &error));
    assert(definitions.empty());
    std::puts("MANAGED_SERVICE_PARSER_AND_EXACT_NATIVE_PLUS_MANAGED_COUNTS_PASS");
}
