#include "experiments/csharp/UniversalHookMailbox.h"
#include <atomic>
#include <cassert>
#include <iostream>
using namespace bl::csharp;
using namespace std::chrono_literals;

int main() {
    assert(!universalHookCallbackIsTerminal(false, false));
    assert(!universalHookCallbackIsTerminal(false, true));
    assert(!universalHookCallbackIsTerminal(true, true));
    assert(universalHookCallbackIsTerminal(true, false));
    UniversalHookMailbox box;
    const auto caller = std::this_thread::get_id();
    int originals = 0;
    auto original = [&] { assert(std::this_thread::get_id() == caller); ++originals; return true; };
    std::mutex wakeMutex;
    std::condition_variable wakeCv;
    bool wake = false;
    auto notify = [&] { std::lock_guard<std::mutex> l(wakeMutex); wake = true; wakeCv.notify_one(); };
    auto await = [&] { std::unique_lock<std::mutex> l(wakeMutex); wakeCv.wait(l, [&] { return wake; }); wake = false; };
    auto run = [&](auto callback, std::chrono::microseconds budget = 1s) {
        std::thread worker([&] {
            await();
            UniversalHookMailbox::Work work{};
            assert(box.take(work));
            callback(work);
        });
        auto result = box.dispatch(7, 123, original, notify, budget);
        worker.join();
        return result;
    };
    auto ok = run([&](auto work) {
        assert(work.slot == 7 && work.self == 123);
        assert(!box.original(8, work.ticket));
        assert(!box.original(7, work.ticket + 1));
        assert(box.original(7, work.ticket));
        assert(!box.original(7, work.ticket));
        box.complete(work.ticket, true);
    });
    assert(ok.accepted && ok.originalRan && !ok.faulted && originals == 1);
    auto suppress = run([&](auto w) { box.complete(w.ticket, true); });
    assert(suppress.accepted && !suppress.originalRan && !suppress.faulted);
    auto failBefore = run([&](auto w) { box.complete(w.ticket, false); });
    assert(failBefore.faulted && !failBefore.originalRan);
    auto failAfter = run([&](auto w) { assert(box.original(7, w.ticket)); box.complete(w.ticket, false); });
    assert(failAfter.faulted && failAfter.originalRan && originals == 2);
    auto noWorker = box.dispatch(7, 0, original, [] {}, 50us);
    assert(noWorker.faulted && !noWorker.originalRan);
    UniversalHookMailbox::Work stale{};
    assert(!box.take(stale));
    std::atomic<bool> running{false}, release{false}, staleAccepted{true};
    std::thread late([&] {
        await();
        UniversalHookMailbox::Work w{};
        assert(box.take(w));
        running = true;
        while (!release) std::this_thread::yield();
        assert(!box.original(7, w.ticket));
        staleAccepted = box.complete(w.ticket, false);
    });
    auto timed = box.dispatch(7, 0, original, notify, 20ms);
    assert(running && timed.faulted && !timed.originalRan);
    auto busy = box.dispatch(7, 0, original, [] {}, 1ms);
    assert(!busy.accepted);
    release = true;
    late.join();
    assert(!staleAccepted);
    assert(!universalHookCallbackIsTerminal(staleAccepted, false));
    release = false;
    std::thread afterOriginal([&] {
        await();
        UniversalHookMailbox::Work w{};
        assert(box.take(w));
        assert(box.original(7, w.ticket));
        while (!release) std::this_thread::yield();
        box.complete(w.ticket, true);
    });
    const int beforeTimeout = originals;
    auto timedAfter = box.dispatch(7, 0, original, notify, 20ms);
    assert(timedAfter.faulted && timedAfter.originalRan && originals == beforeTimeout + 1);
    release = true;
    afterOriginal.join();
    auto recovered = run([&](auto w) { box.complete(w.ticket, true); });
    assert(recovered.accepted && !recovered.faulted);
    auto cancelled = run([&](auto w) { box.cancel(); assert(!box.original(7, w.ticket)); box.complete(w.ticket, false); });
    assert(cancelled.faulted && !cancelled.originalRan);
    std::cout << "mailbox: original caller affinity, timeout, stale failure rejected, accepted failure terminal, cancellation, recovery PASS\n";
}
