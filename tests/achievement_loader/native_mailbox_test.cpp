#include "experiments/csharp/NativeAchievementMailbox.h"
#include <cassert>
#include <string>

using namespace bl::csharp::achievement;

int main() {
    Mailbox mailbox;
    assert(mailbox.bind("alpha", "/mods/a.dll|bunny-revision=1"));
    assert(mailbox.bind("beta", "/mods/b.dll|bunny-revision=1"));
    auto pending = mailbox.pending();
    assert(pending.size() == 2);
    assert(mailbox.activate(pending[0], 2));
    assert(mailbox.activate(pending[1], 1));
    assert(mailbox.anyReady());
    assert(mailbox.publish({0, 10, 1, true}));
    auto deliveries = mailbox.drain();
    assert(deliveries.size() == 2);
    assert(deliveries[0].mod == "alpha");
    assert(deliveries[1].mod == "beta");
    assert(mailbox.retry(deliveries[0]));
    auto retry = mailbox.drain();
    assert(retry.size() == 1 && retry[0].attempts == 1);
    assert(mailbox.bind("alpha", "/mods/a.dll|bunny-revision=2"));
    assert(!mailbox.retry(retry[0]));
    assert(!mailbox.anyReady() || mailbox.pending().size() == 1);
    assert(!mailbox.activate(pending[0], 1));
    auto changed = mailbox.pending();
    assert(changed.size() == 1 && changed[0].mod == "alpha");
    assert(mailbox.activate(changed[0], 2));
    assert(mailbox.publish({0, 10, 1, true}));
    assert(mailbox.size() == 2);
    mailbox.retire("alpha");
    auto current = mailbox.drain();
    assert(current.size() == 1 && current[0].mod == "beta");
    mailbox.newWorld();
    assert(!mailbox.publish({-1, 10, 1, true}));
    assert(!mailbox.publish({0, 10, 0, true}));
    assert(mailbox.publish({0, 10, 1, true}));
    assert(mailbox.size() == 1);
    mailbox.newWorld();
    assert(mailbox.size() == 0);
    mailbox.retire("beta");
    assert(mailbox.pending().empty());
    assert(!mailbox.anyReady());
    return 0;
}
