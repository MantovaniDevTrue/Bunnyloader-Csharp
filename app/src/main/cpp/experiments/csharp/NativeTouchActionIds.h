#pragma once
#include <cstdint>

namespace bl::csharp::touch {

// Os IDs do Terraria vêm dos enums descobertos no jogo. A reserva evita
// disputar os índices que o TouchButtonLoader JS adiciona após esses enums.
// Não prova coexistência com mais de 512 ações de terceiros: nessa situação
// o instalador deve recusar as tabelas conflitantes, nunca sobrescrevê-las.
constexpr int kOtherLoaderReservation = 512;
constexpr int kMaxGameActionId = 4095;

struct ActionIds {
    int actionBase = -1;
    int controlBase = -1;
    int quickBase = -1;
    int capacity = 0;

    bool valid() const;
    int action(int index) const;
    int control(int index) const;
    int quick(int index) const;
    int byAction(int raw) const;
    int byControl(int raw) const;
    int byQuick(int raw) const;
};

// layoutCount é o índice "Count" (desvincular); controlCount é a
// primeira posição pós-vanilla; quickUnbound é o ID do ícone desvinculado.
ActionIds resolveActionIds(int layoutCount, int controlCount,
                           int quickUnbound, int capacity);

} // namespace bl::csharp::touch
