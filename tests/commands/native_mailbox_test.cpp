#include "experiments/csharp/NativeCommandMailbox.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using bl::csharp::NativeCommandMailbox;
using bl::csharp::NativeCommandReply;
using bl::csharp::NativeCommandRoute;
using bl::csharp::NativeCommandSpec;
using bl::csharp::NativeCommandSubmit;

int main() {
    NativeCommandMailbox mailbox;
    std::string error;

    assert(mailbox.submit("/heal 10", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.registerMod("examplemod", {
        {"heal", 1}, {"curar", 1}, {"addtime", 4}}, &error));
    assert(mailbox.registerMod("second", {{"heal", 1}}, &error));
    assert(!mailbox.registerMod("examplemod", {{"other", 1}}, &error));
    assert(!mailbox.registerMod("ExampleMod", {{"same-owner", 1}}, &error));
    assert(!mailbox.registerMod("invalid mod", {{"other", 1}}, &error));
    assert(!mailbox.registerMod("badalias", {{"ok", 1}, {"OK", 1}}, &error));
    assert(mailbox.pending() == 0);
    NativeCommandMailbox deferred;
    assert(deferred.registerMod("pending", {{"later", 1}}, nullptr, false));
    assert(deferred.submit("/later", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(deferred.setModReady("pending", true));
    assert(deferred.submit("/later", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(deferred.setModReady("pending", false));
    assert(deferred.submit("/later", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    deferred.newWorld(true);
    assert(!deferred.setModReady("pending", true));

    assert(mailbox.submit("/vanillahelp", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.submit("not a command", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.submit("   /curar 7", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(mailbox.submit("/SECOND:HEAL 7", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(mailbox.submit("/unknown:heal 7", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(mailbox.submit("/addtime 120", 0, 1, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.submit("/addtime 120", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(mailbox.submit("/addtime 120", 0, 3, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.submit("/addtime 120", 0, 3, NativeCommandRoute::Server) ==
           NativeCommandSubmit::Queued);
    assert(mailbox.submit("/heal 10", 0, 2, NativeCommandRoute::Server) ==
           NativeCommandSubmit::Vanilla);
    assert(mailbox.submit("/help", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);

    auto jobs = mailbox.takeRequests(3);
    assert(jobs.size() == 3 && jobs[0].input == "/curar 7");
    const auto epoch = jobs[0].epoch;
    assert(mailbox.deliver(epoch, jobs[0].sequence,
           {{"Curou", 0, 124, 252, 0}}));
    assert(mailbox.takeReplies(8).size() == 1);
    assert(!mailbox.deliver(epoch - 1, jobs[1].sequence, {{"old", 0}}));
    auto rest = mailbox.takeRequests(8);
    assert(rest.size() == 2);
    assert(mailbox.pending() == 0);

    assert(mailbox.submit("/heal 1", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    auto pending = mailbox.takeRequests();
    assert(pending.size() == 1);
    mailbox.newWorld();
    assert(mailbox.pending() == 0 && mailbox.queuedReplies() == 0);
    assert(!mailbox.deliver(pending[0].epoch, pending[0].sequence, {{"old", 0}}));
    assert(mailbox.submit("/heal 5", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);

    // Se um mod descarrega enquanto Mono executa, descartar a resposta atrasada.
    pending = mailbox.takeRequests();
    assert(pending.size() == 1);
    assert(mailbox.unregisterMod("second"));
    assert(!mailbox.deliver(pending[0].epoch, pending[0].sequence, {{"stale", 0}}));
    assert(mailbox.submit("/heal 1", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    mailbox.newWorld(true);
    assert(mailbox.submit("/heal 1", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Vanilla);

    NativeCommandMailbox bounded;
    assert(bounded.registerMod("sample", {{"echo", 1}}));
    for (size_t i = 0; i < NativeCommandMailbox::MaxPending; ++i)
        assert(bounded.submit("/echo x", 0, 0, NativeCommandRoute::Chat) ==
               NativeCommandSubmit::Queued);
    assert(bounded.submit("/echo x", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Busy);
    auto first = bounded.takeRequests(NativeCommandMailbox::MaxPending + 100);
    assert(first.size() == NativeCommandMailbox::MaxPending);
    assert(first.back().sequence == NativeCommandMailbox::MaxPending);
    for (const auto& request : first)
        assert(bounded.deliver(request.epoch, request.sequence, {{"ok", 0}}));
    auto response = bounded.takeReplies(300);
    assert(response.size() == NativeCommandMailbox::MaxPending);
    assert(!bounded.deliver(first[0].epoch, first[0].sequence, {{"duplicate", 0}}));
    assert(!bounded.deliver(first[0].epoch, first.back().sequence + 1, {{"future", 0}}));
    assert(bounded.submit("/echo valid", 0, 0, NativeCommandRoute::Chat) ==
           NativeCommandSubmit::Queued);
    assert(bounded.submit("/echo " +
        std::string(NativeCommandMailbox::MaxInputBytes, 'a'),
        0, 0, NativeCommandRoute::Chat) == NativeCommandSubmit::Vanilla);
    auto limited = bounded.takeRequests();
    assert(limited.size() == 1);
    assert(!bounded.deliver(limited[0].epoch, limited[0].sequence,
        {{std::string(NativeCommandMailbox::MaxLineBytes + 1, 'a'), 0}}));
    assert(!bounded.deliver(limited[0].epoch, limited[0].sequence,
        {{"wrong recipient", 1}}));
    assert(bounded.deliver(limited[0].epoch, limited[0].sequence, {}));

    // Dois threads exercitam somente o mailbox, sem chamar IL2CPP ou Mono.
    NativeCommandMailbox threaded;
    assert(threaded.registerMod("threads", {{"test", 1}}));
    constexpr int target = 1000;
    std::thread producer([&] {
        for (int i = 0; i < target;) {
            auto outcome = threaded.submit("/test " + std::to_string(i), 0, 0,
                                            NativeCommandRoute::Chat);
            if (outcome == NativeCommandSubmit::Queued) ++i;
            else {
                assert(outcome == NativeCommandSubmit::Busy);
                std::this_thread::yield();
            }
        }
    });
    std::thread consumer([&] {
        int consumed = 0;
        while (consumed < target) {
            auto slice = threaded.takeRequests(8);
            for (const auto& request : slice) {
                assert(request.epoch == threaded.epoch());
                assert(threaded.deliver(request.epoch, request.sequence, {}));
                ++consumed;
            }
            if (slice.empty()) std::this_thread::yield();
        }
    });
    producer.join();
    consumer.join();
    assert(threaded.pending() == 0);
    std::cout << "BUNNY_CSHARP_NATIVE_COMMAND_MAILBOX=PASS" << std::endl;
}
