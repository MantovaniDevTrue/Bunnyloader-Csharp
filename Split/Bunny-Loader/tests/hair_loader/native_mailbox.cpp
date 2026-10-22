#include "experiments/csharp/NativeHairSlotMailbox.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace bl::csharp::hair;
using namespace std::chrono_literals;

static void check(bool ok, const char* reason) {
    if (!ok) {
        std::cerr << "FAIL " << reason << '\n';
        std::exit(1);
    }
    std::cout << "OK " << reason << '\n';
}

static void waitRequest(const NativeHairSlotMailbox& mailbox) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!mailbox.HasPending() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    check(mailbox.HasPending(), "worker entregou solicitação ao pump");
}

int main() {
    NativeHairSlotMailbox mailbox;
    int readerCalls = 0;
    for (int i = 0; i < 10000; ++i) {
        mailbox.Pump([&] {
            ++readerCalls;
            return NativeHairSlotSnapshot{};
        });
    }
    check(readerCalls == 0, "10.000 frames ociosos não consultam IL2CPP");

    NativeHairSlotSnapshot answer;
    std::thread worker([&] { answer = mailbox.Request(2s); });
    waitRequest(mailbox);
    mailbox.Pump([&] {
        ++readerCalls;
        NativeHairSlotSnapshot snapshot;
        snapshot.valid = true;
        snapshot.occupied.set(228);
        snapshot.occupied.set(234);
        return snapshot;
    });
    worker.join();
    check(answer.valid && answer.IsOccupied(228) &&
          answer.IsOccupied(234) && !answer.IsOccupied(235) &&
          readerCalls == 1 && !mailbox.HasPending(),
          "worker recebe bits copiados do único acesso da game thread");

    std::thread cancelled([&] { answer = mailbox.Request(2s); });
    waitRequest(mailbox);
    mailbox.Cancel();
    cancelled.join();
    check(!answer.valid && answer.error == "HairSlotMailboxCancelled" &&
          !mailbox.HasPending(), "mudança de mundo cancela worker bloqueado");

    std::thread one([&] { answer = mailbox.Request(2s); });
    waitRequest(mailbox);
    const auto duplicate = mailbox.Request(20ms);
    check(!duplicate.valid &&
          duplicate.error == "HairSlotMailboxAlreadyPending",
          "no máximo um pedido por vez evita race");
    mailbox.Pump([&] {
        NativeHairSlotSnapshot data;
        data.valid = true;
        return data;
    });
    one.join();
    check(answer.valid, "pedido original ainda foi concluído");

    auto timeout = mailbox.Request(30ms);
    check(!timeout.valid && timeout.error == "HairSlotMailboxTimedOut" &&
          !mailbox.HasPending(), "timeout devolve erro sem polling infinito");
    const int previousCalls = readerCalls;
    mailbox.Pump([&] {
        ++readerCalls;
        return NativeHairSlotSnapshot{};
    });
    check(readerCalls == previousCalls,
          "timeout não permite pump tardio publicar snapshot obsoleto");
    NativeHairSlotSnapshot expired, fresh;
    std::atomic<bool> oldReading{false};
    std::thread expiredWorker([&] { expired = mailbox.Request(80ms); });
    waitRequest(mailbox);
    std::thread oldPump([&] {
        mailbox.Pump([&] {
            oldReading.store(true);
            std::this_thread::sleep_for(150ms);
            NativeHairSlotSnapshot oldData;
            oldData.valid = true;
            oldData.occupied.set(234);
            return oldData;
        });
    });
    const auto started = std::chrono::steady_clock::now() + 2s;
    while (!oldReading.load() && std::chrono::steady_clock::now() < started)
        std::this_thread::sleep_for(1ms);
    check(oldReading.load(), "leitura IL2CPP antiga iniciada");
    expiredWorker.join();
    check(expired.error == "HairSlotMailboxTimedOut",
          "pedido antigo expira enquanto o game thread lê");
    std::thread nextWorker([&] { fresh = mailbox.Request(2s); });
    waitRequest(mailbox);
    oldPump.join();
    mailbox.Pump([&] {
        NativeHairSlotSnapshot current;
        current.valid = true;
        current.occupied.set(235);
        return current;
    });
    nextWorker.join();
    check(fresh.valid && fresh.IsOccupied(235) && !fresh.IsOccupied(234),
          "leitura obsoleta não pode entregar resposta ao próximo request");

    const auto invalid = mailbox.Request(6s);
    check(invalid.error == "HairSlotMailboxInvalidTimeout",
          "worker não pode bloquear por tempo arbitrário");

    std::cout << "HAIR_SLOT_MAILBOX_WORKER_GAME_THREAD=PASS NO_APK\n";
}
