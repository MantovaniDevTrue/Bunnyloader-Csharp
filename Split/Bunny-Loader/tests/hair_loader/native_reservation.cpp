#include "experiments/csharp/NativeHairReservation.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace bl::csharp::hair;

static void check(bool ok, const char* name) {
    if (!ok) {
        std::cerr << "FAIL " << name << '\n';
        std::exit(1);
    }
    std::cout << "OK " << name << '\n';
}

int main() {
    std::set<int> occupied{228, 229, 237};
    const auto used = [&](int id) { return occupied.contains(id); };
    TypeReservations reservation;
    TypeReservations::Block a, b, c, out;
    std::string error;

    check(reservation.Reserve("mod-a", 2, used, &a, &error) &&
          a.first == 230 && a.count == 2,
          "mod A nunca sobrepoe cabelos Bunny JS em 228 e 229");
    check(reservation.Reserve("mod-b", 4, used, &b, &error) &&
          b.first == 232 && b.count == 4,
          "mod B usa bloco livre apos A");
    check(reservation.Reserve("mod-c", 3, used, &c, &error) &&
          c.first == 238 && c.count == 3,
          "mod C pula ocupacao externa mesmo dentro do bloco");
    check(reservation.ModCount() == 3 &&
          reservation.Contains(238) && reservation.Contains(240) &&
          !reservation.Contains(237), "snapshot de reservas correto");
    check(reservation.Reserve("mod-b", 4, used, &out, &error) &&
          out.first == b.first, "repetir reserva para mod idempotente");
    check(!reservation.Reserve("mod-b", 2, used, &out, &error) &&
          error == "HairReservationModCountChanged" && out.first == -1,
          "mod nao pode alterar contagem apos reserva");
    check(!reservation.Reserve("../unsafe", 1, used, &out, &error),
          "UID inseguro rejeitado");

    check(reservation.Release("mod-a") &&
          !reservation.Contains(230) && !reservation.Contains(231) &&
          reservation.Contains(b.first), "unload de A nao reenumera ID de B");
    check(reservation.Reserve("mod-d", 2, used, &out, &error) &&
          out.first == 230, "faixa liberada volta a ser utilizavel");
    check(!reservation.Release("absent"), "unload de mod ausente nao interfere");
    reservation.Reset();
    check(reservation.ModCount() == 0 && !reservation.Contains(230),
          "fim de sessao libera todos IDs");

    TypeReservations restricted(4094, 4095);
    check(!restricted.Reserve("too-many", 3, used, &out, &error) &&
          error == "HairReservationNoContiguousRange",
          "ausencia de bloco contiguo detectada sem alterar estado");
    check(restricted.Reserve("last", 2, used, &out, &error) &&
          out.first == 4094 && out.count == 2,
          "limite do tipo 4095 aceito exatamente");
    check(!restricted.Reserve("overflow", 1, used, &out, &error),
          "esgotamento nao transborda inteiro");

    Registry registry(a.first);
    check(registry.RegisterBatch({
        {"mod-a", "Hair1", "Content/Hairs/Hair1.png", "Content/Hairs/Hair1.png"},
        {"mod-a", "Hair2", "Content/Hairs/Hair2.png", "Content/Hairs/Hair2.png"}
    }, [](std::string_view) { return true; }, &error) &&
       registry.ByName("mod-a/Hair1")->type == 230 &&
       registry.ByName("mod-a/Hair2")->type == 231,
       "Registry atribui IDs coerentes com reserva contigua");
    std::cout << "HairTypeReservations multi-mod/JS collision tests OK. NO APK.\n";
}
