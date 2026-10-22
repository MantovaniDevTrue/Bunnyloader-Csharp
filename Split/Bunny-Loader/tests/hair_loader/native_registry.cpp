#include "experiments/csharp/NativeHairRegistry.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using bl::csharp::hair::Appearance;
using bl::csharp::hair::Declaration;
using bl::csharp::hair::RandomGender;
using bl::csharp::hair::Receiver;
using bl::csharp::hair::Registry;

static int verified = 0;

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL " << label << '\n';
        std::exit(1);
    }
    ++verified;
    std::cout << "OK " << label << '\n';
}

int main() {
    const std::set<std::string> files = {
        "Content/Hairs/A.png",
        "Content/Hairs/B.png",
        "Content/Hairs/B_Alt.png"
    };
    const auto exists = [&](std::string_view path) {
        return files.find(std::string(path)) != files.end();
    };
    std::string error;
    Registry a;
    Declaration hairA{"mod-a", "A", "Content/Hairs/A", "Content/Hairs/A_Alt"};
    Declaration hairB{"mod-b", "B", "Content/Hairs/B.png", "Content/Hairs/B_Alt",
                      RandomGender::Female, false};

    check(a.RegisterBatch({hairA, hairB}, exists, &error), "lote valido aceito");
    check(a.Size() == 2 && a.NextType() == 230, "IDs nativos reservados apos vanilla");
    check(a.ByType(228) && a.ByType(229) && !a.ByType(230), "consulta tipo protegida");
    check(a.ByType(228)->altTexture == "Content/Hairs/A.png", "fallback de textura _Alt");
    check(a.ByType(229)->altTexture == "Content/Hairs/B_Alt.png", "textura _Alt correta");
    check(a.ByType(229)->randomGender == RandomGender::Female &&
          !a.ByType(229)->availableDuringCharacterCreation, "metadados de criacao intactos");
    check(a.Resolve("mod-b/B") == 229 && a.Resolve("mod-a/A") == 228, "lookup por UID/classe");

    check(!a.RegisterBatch({hairA}, exists, &error) &&
          error.starts_with("NativeHairDuplicateIdentity"), "duplicata rejeitada");
    Declaration unsafe = hairA;
    unsafe.modUid = "other";
    unsafe.texture = "../outside.png";
    check(!a.RegisterBatch({unsafe}, exists, &error) &&
          error.starts_with("NativeHairMainTexture"), "path traversal rejeitado");
    unsafe.texture = "Content/Hairs/A";
    unsafe.altTexture = "Content/../../bad.png";
    check(!a.RegisterBatch({unsafe}, exists, &error), "alternativa insegura rejeitada");
    unsafe.altTexture = "Content/Hairs/A";
    unsafe.randomGender = static_cast<RandomGender>(100);
    check(!a.RegisterBatch({unsafe}, exists, &error), "genero invalido rejeitado");
    unsafe.randomGender = RandomGender::Male;
    unsafe.className = "Missing";
    unsafe.texture = "Content/Hairs/Missing";
    check(!a.RegisterBatch({unsafe}, exists, &error), "textura principal obrigatoria");

    Declaration good{"other", "C", "Content/Hairs/A", "Content/Hairs/A"};
    Declaration bad{"other", "Bad", "../wrong", "Content/Hairs/A"};
    check(!a.RegisterBatch({good, bad}, exists, &error) && a.Size() == 2 &&
          a.NextType() == 230 && !a.ByName("other/C"), "lote atomico, sem reserva parcial");

    check(a.SaveKey(228) == "H1|mod-a/A" && a.SaveKey(0).empty(),
          "salva somente identidade de cabelo mod");
    int loaded = -1;
    check(a.RestoreKey("H1|mod-b/B", &loaded) && loaded == 229, "restaura por identidade");
    check(a.RestoreKey("H1|other/Gone", &loaded) && loaded == 0,
          "mod desabilitado volta ao cabelo vanilla 0");
    check(!a.RestoreKey("H1|../escape", &loaded) && loaded == 0,
          "chave malformada nao e carregada");

    Registry b;
    check(b.RegisterBatch({hairB, hairA}, exists, &error), "segunda sessao com ordem diferente");
    check(a.Resolve("mod-a/A") != b.Resolve("mod-a/A"), "IDs locais diferem entre clientes");
    Appearance incoming;
    const std::string packet = a.EncodeAppearance(7, 228);
    check(packet == "H1|7|mod-a/A", "pacote envia identidade, nao ID local");
    check(b.DecodeAppearance(packet, Receiver::Client, 256, &incoming) &&
          incoming.player == 7 && incoming.type == b.Resolve("mod-a/A") &&
          !incoming.fallback, "cliente reinterpreta tipo local");
    check(!b.DecodeAppearance(packet, Receiver::Client, 7, &incoming),
          "cliente so aceita atualizacao do servidor 256");
    check(!b.DecodeAppearance(packet, Receiver::Server, 8, &incoming),
          "servidor recusa spoof de outro jogador");
    check(b.DecodeAppearance(packet, Receiver::Server, 7, &incoming),
          "servidor aceita apenas jogador remetente");
    check(!b.DecodeAppearance("H1|7|other/Gone", Receiver::Server, 7, &incoming),
          "servidor nao propaga cabelo ausente");
    check(!b.DecodeAppearance("H1|7|mod-a/A|extra", Receiver::Server, 7, &incoming),
          "dados extras rejeitados");
    check(!b.DecodeAppearance("H1|-1|mod-a/A", Receiver::Server, 7, &incoming),
          "indice negativo rejeitado");
    check(!b.DecodeAppearance("H1|256|mod-a/A", Receiver::Client, 256, &incoming),
          "indice fora do limite rejeitado");
    check(!b.DecodeAppearance("H1|07|mod-a/A", Receiver::Server, 7, &incoming) ||
          incoming.player == 7, "indice validado");
    check(b.DecodeAppearance("H1|7|mod-c/Unknown", Receiver::Client, 256, &incoming) &&
          incoming.fallback && incoming.type == 0,
          "cliente sem mod usa fallback seguro");
    check(a.EncodeAppearance(256, 228).empty() && a.EncodeAppearance(7, 200).empty(),
          "saida sem jogador/ID valido bloqueada");

    const auto snapshot = a.Snapshot();
    check(snapshot.size() == 2 && snapshot[0].type == 228 &&
          snapshot[1].type == 229, "snapshot ordenado por tipo");
    a.Reset();
    check(a.Size() == 0 && a.NextType() == 228 && !a.ByName("mod-a/A"),
          "reset zera sessao e identidades");

    Registry badCapacity(4096);
    check(!badCapacity.RegisterBatch({hairA}, exists, &error), "capacidade invalida rejeitada");
    Registry capacity(4095);
    check(!capacity.RegisterBatch({hairA, hairB}, exists, &error) &&
          capacity.Size() == 0, "esgotamento nao registra lote parcial");

    std::cout << "NativeHairRegistry: " << verified
              << " contratos passaram (nenhum APK gerado).\n";
}
