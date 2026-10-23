#pragma once
#include <cstdint>
#include <string>

namespace bl::csharp::touch {

struct HostAbiCheck {
    bool controller = false;
    bool arrays = false;
    bool touch = false;
    bool editor = false;
    bool layouts = false;
    bool group = false;
    std::string diagnostic;
    bool complete() const {
        return controller && arrays && touch && editor && layouts && group;
    }
};

// Somente na thread do jogo, após IL2CPP estar inicializado. Descobre e
// valida o contrato da build instalada sem escrever campos ou instalar hooks.
HostAbiCheck inspectNativeTouchAbi();

} // namespace bl::csharp::touch
