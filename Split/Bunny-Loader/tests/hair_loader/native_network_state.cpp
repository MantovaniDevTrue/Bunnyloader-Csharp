#include "experiments/csharp/NativeHairNetworkState.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace bl::csharp::hair;

void check(bool ok, const char* name) {
    if (!ok) {
        std::cerr << "FAIL " << name << '\n';
        std::exit(1);
    }
    std::cout << "OK " << name << '\n';
}

int main() {
    Registry server;
    Registry client(300);
    std::string error;
    const Declaration a{"alpha", "Hair", "Content/Hairs/Hair.png",
                        "Content/Hairs/Hair.png"};
    const Declaration b{"beta", "Style", "Content/Hairs/Style.png",
                        "Content/Hairs/Style.png"};
    auto present = [](std::string_view) { return true; };
    check(server.RegisterAt({a, b}, 230, present, &error),
          "servidor com faixa depois de JS");
    check(client.RegisterAt({b, a}, 300, present, &error),
          "cliente com ordem oposta");
    NetworkState serverNetwork(server), clientNetwork(client);

    check(serverNetwork.EncodeOutgoing(7, 230) == "H1|7|alpha/Hair",
          "wire não depende de ID do servidor");
    check(clientNetwork.EncodeOutgoing(7, 301) == "H1|7|alpha/Hair",
          "wire não depende de ID do cliente");
    check(serverNetwork.EncodeOutgoing(7, 5) == "H1|7|#5",
          "hair vanilla também pode ser enviado");
    check(serverNetwork.EncodeOutgoing(256, 230).empty() &&
          serverNetwork.EncodeOutgoing(7, 4095).empty(),
          "índice e tipo inexistentes não produzem pacote");

    NetworkUpdate update;
    check(serverNetwork.Receive("H1|7|alpha/Hair", Receiver::Server, 7, &update) &&
          update.accepted && update.changed && update.relay &&
          update.localType == 230 && update.ignoreClient == 7 &&
          update.relayPacket == "H1|7|alpha/Hair",
          "servidor valida dono e prepara relay sem IDs locais");
    check(!serverNetwork.Receive("H1|7|alpha/Hair", Receiver::Server, 8, &update),
          "pacote forjado de outro jogador é recusado");
    check(!serverNetwork.Receive("H1|7|gamma/Missing", Receiver::Server, 7, &update),
          "servidor recusa cabelo não instalado");
    check(serverNetwork.Receive("H1|7|alpha/Hair", Receiver::Server, 7, &update) &&
          !update.changed && !update.relay,
          "pacote duplicado não causa broadcast repetido");

    check(clientNetwork.Receive("H1|7|alpha/Hair", Receiver::Client, 256, &update) &&
          update.localType == 301 && !update.relay,
          "cliente converte UID/classe para ID local");
    check(!clientNetwork.Receive("H1|7|alpha/Hair", Receiver::Client, 7, &update),
          "cliente só aceita servidor como remetente");
    check(clientNetwork.Receive("H1|7|gamma/Missing", Receiver::Client, 256, &update) &&
          update.fallback && update.localType == 0 &&
          update.stableIdentity == "gamma/Missing",
          "sem mod no cliente recebe fallback sem perder identidade");

    check(serverNetwork.Receive("H1|7|#12", Receiver::Server, 7, &update) &&
          update.changed && update.relay && update.localType == 12,
          "trocar cabelo de mod por vanilla limpa aparência pela rede");
    check(clientNetwork.Receive("H1|7|#12", Receiver::Client, 256, &update) &&
          update.localType == 12 && !update.fallback,
          "cliente aplica cabelo vanilla exato");
    check(!serverNetwork.Receive("H1|7|#228", Receiver::Server, 7, &update) &&
          !serverNetwork.Receive("H1|7|#-1", Receiver::Server, 7, &update) &&
          !serverNetwork.Receive("H1|7|#999", Receiver::Server, 7, &update),
          "vanilla fora dos IDs permitidos é recusado");
    check(!serverNetwork.Receive("H1|07|#12", Receiver::Server, 7, &update) &&
          !serverNetwork.Receive("H1|7|#012", Receiver::Server, 7, &update),
          "índices ambíguos com zeros extras são recusados");
    check(!serverNetwork.Receive("H1|7|#12|junk", Receiver::Server, 7, &update) &&
          !serverNetwork.Receive("H1|7|alpha/Hair\n", Receiver::Server, 7, &update) &&
          !serverNetwork.Receive("H1|7|alpha/../Hair", Receiver::Server, 7, &update),
          "payload malformado não altera o jogador");
    check(!serverNetwork.Receive(std::string(400, 'X'), Receiver::Server, 7, &update),
          "pacote grande é descartado antes de parse");

    serverNetwork.ForgetPlayer(7);
    check(serverNetwork.Receive("H1|7|#12", Receiver::Server, 7, &update) &&
          update.changed && update.relay,
          "reutilização do slot força ressincronização");
    clientNetwork.Reset();
    check(clientNetwork.TrackedPlayers() == 0, "mudança de sessão remove estado de rede");
    std::cout << "CSharp HairNetworkState sender/identity/vanilla contracts PASS. NO APK.\n";
}
