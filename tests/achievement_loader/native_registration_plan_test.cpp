#include "experiments/csharp/NativeAchievementRegistrationPlan.h"

#include <cassert>
#include <vector>

using namespace bl::csharp::achievement;

int main() {
    Descriptor a;
    a.name = "a/ManyWormsKilled";
    a.friendlyName = "Many Worms Killed";
    a.texture = "Content/Achievements/ManyWormsKilled";
    a.conditions.push_back({"Condition", 1, 5.f, {}});
    AchievementState state;
    state.name = a.name;
    state.conditions.push_back({"Condition", 1, 3.f});

    std::vector<Descriptor> defs{a};
    std::vector<AchievementState> states{state};
    auto valid = prepareNativeRegistration("a", "rev1", defs, states, true, true);
    assert(valid.prepared && valid.names.size() == 1);
    assert(prepareNativeRegistration("a", "rev1", defs, states, false, true).reason
           == "ReversibleUnregisterUnavailable");
    assert(prepareNativeRegistration("a", "rev1", defs, states, true, false).reason
           == "AchievementAtlasUnavailable");
    assert(!prepareNativeRegistration("b", "rev1", defs, states, true, true).prepared);
    assert(!prepareNativeRegistration("a", "rev1", defs, {}, true, true).prepared);

    NativeRegistrationLedger ledger;
    assert(!ledger.begin(prepareNativeRegistration("a", "rev1", defs, states, false, true)));
    assert(ledger.begin(valid));
    assert(!ledger.add("b/ManyWormsKilled"));
    assert(ledger.add("a/ManyWormsKilled"));
    assert(!ledger.add("a/ManyWormsKilled"));
    assert(ledger.commit(valid));
    assert(!ledger.add("a/Other"));
    ledger.reset();
    assert(ledger.begin(valid));
    assert(!ledger.commit(valid));
    ledger.reset();

    a.conditions[0].kind = 3;
    assert(prepareNativeRegistration("a", "rev1", {a}, states, true, true).reason
           == "UnsupportedOrInconsistentCondition");
    return 0;
}
