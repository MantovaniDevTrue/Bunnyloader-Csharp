#include "experiments/csharp/MonoWorldMailbox.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
using namespace bl::csharp::worldsave;
using namespace std::chrono_literals;

int main() {
    MonoWorldMailbox mailbox;
    MonoWorldRequest enter;
    enter.action = MonoWorldAction::Enter;
    enter.epoch = 1;
    auto first = mailbox.submit(enter);
    std::shared_ptr<MonoWorldTicket> running;
    MonoWorldReply response;
    assert(first && mailbox.take(&running) && mailbox.current(running));
    mailbox.finish(running, {true, "ok", ""});
    assert(mailbox.wait(first, 10ms, &response) && response.payload == "ok");

    MonoWorldRequest save{};
    save.action = MonoWorldAction::Prepare;
    save.epoch = 1;
    for (size_t i = 0; i < MonoWorldMailbox::Limit; ++i)
        assert(mailbox.submit(save));
    assert(!mailbox.submit(save));
    mailbox.invalidate();
    assert(!mailbox.pending());
    auto stale = mailbox.submit(save);
    assert(mailbox.take(&running));
    std::thread waiter([&] { MonoWorldReply r; assert(!mailbox.wait(stale, 300ms, &r)); });
    std::this_thread::sleep_for(1ms);
    mailbox.invalidate();
    waiter.join();
    mailbox.finish(running, {true, "stale", ""});
    assert(!mailbox.wait(stale, 10ms, &response));

    auto timed = mailbox.submit(save);
    assert(timed && mailbox.take(&running));
    assert(!mailbox.wait(timed, 1ms, &response));
    assert(!mailbox.current(timed));
    mailbox.finish(running, {true, "late", ""});

    save.epoch = 2;
    auto fresh = mailbox.submit(save);
    assert(fresh && mailbox.take(&running) && mailbox.current(running));
    mailbox.finish(running, {true, "newworld", ""});
    assert(mailbox.wait(fresh, 10ms, &response) && response.payload == "newworld");
    std::puts("WORLD_MONO_MAILBOX_PASS");
}
