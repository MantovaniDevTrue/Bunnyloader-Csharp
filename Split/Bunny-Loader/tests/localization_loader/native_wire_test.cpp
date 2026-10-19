#include "experiments/csharp/NativeLocalizationRuntime.h"
#include <cassert>
#include <string>
#include <vector>
using bl::csharp::localization::Entry;
using bl::csharp::localization::decodeWire;
int main() {
    std::string error;
    Entry valid;
    const std::vector<std::string> row={
        "LOCALE","pt-BR","4D6F64732E736F757263652E4974656D4E616D652E4578616D706C654974656D",
        "4578656D706C6F206465204974656D"
    };
    assert(decodeWire(row,"examplemodcsharp",&valid,&error));
    assert(valid.culture=="pt-BR");
    assert(valid.key=="Mods.examplemodcsharp.ItemName.ExampleItem");
    assert(valid.value=="Exemplo de Item");
    auto invalid=row;
    invalid[0]="ITEM";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[1]="pt_BR";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[2]="4D6F64732E6F746865722E49";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]="FF";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]="EFBFBD";assert(decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]="EDA080";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]="C0AF";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]="4142A";assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[2]=std::string(600,'A');assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    invalid=row;invalid[3]=std::string(16386,'A');assert(!decodeWire(invalid,"examplemodcsharp",&valid,&error));
    assert(!decodeWire(row,"bad/mod",&valid,&error));
    assert(!decodeWire(row,"examplemodcsharp",nullptr,&error));
    assert(!decodeWire(row,"examplemodcsharp",&valid,nullptr));
    return 0;
}
