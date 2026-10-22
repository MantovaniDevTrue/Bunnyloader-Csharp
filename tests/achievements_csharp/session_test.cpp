#include "experiments/csharp/ManagedAchievementSession.h"

#include <cassert>
#include <iostream>

int main() {
    bl::csharp::ManagedAchievementSession session;
    assert(!session.active() && session.stamp() == 0);
    assert(!session.accepts(1));
    const auto first = session.resume();
    assert(first && session.accepts(first));
    session.pause();
    assert(!session.accepts(first));
    assert(session.stamp() == 0);
    const auto second = session.resume();
    assert(second != first && session.accepts(second));
    assert(!session.accepts(first));
    session.pause();
    session.pause();
    const auto third = session.resume();
    assert(third != second && session.accepts(third));
    std::cout << "CSHARP_ACHIEVEMENT_SESSION_EPOCH_PASS=YES\n";
}
