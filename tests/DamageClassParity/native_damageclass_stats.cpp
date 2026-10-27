#include "experiments/csharp/NativeDamageClassStats.h"

#include <cassert>
#include <atomic>
#include <thread>

int main() {
    using namespace bl::csharp;
    NativeDamageClassStatRegistry registry;
    auto expanded = [](std::string row, bool inheritGeneric = false) {
        if (!row.empty() && row.back() == '\n') row.pop_back();
        for (int i = 0; i < 25; ++i)
            row += (inheritGeneric && i < 5) ? "\t1" : "\t0";
        return row + "\n";
    };
    const std::string header = "DAMAGE_CLASS_STATS_V2\n";
    const std::string custom =
        "CLASS\tExample.Damage\t0\t1.2\t1\t3\t0\t1\t1.5\t0\t8\t1.1\t10\t1\n";
    NativeDamageClassStatDefaults stats;
    assert(registry.publish("mod-a", header + expanded(custom, true)));
    assert(registry.publish("mod-b", header + expanded(
        "CLASS\tOther.Damage\t0\t1\t1\t0\t0\t1\t1\t0\t4\t1\t0\t0\n")));
    assert(registry.lookup("mod-a", "C:Example.Damage", &stats));
    assert(stats.damage.additive == 1.2f && stats.damage.flat == 3.f);
    assert(stats.knockback.multiplicative == 1.5f);
    assert(stats.inheritance[0].damage == 1.f && stats.inheritance[0].crit == 1.f);
    assert(stats.inheritance[1].damage == 0.f && stats.inheritance[4].knockback == 0.f);
    assert(stats.crit == 8.f && stats.attackSpeed == 1.1f &&
           stats.armorPenetration == 10.f && stats.standardCrit);
    assert(!registry.lookup("mod-a", "C:Other.Damage", &stats));
    assert(!registry.publish("mod-a", header + custom));
    assert(!registry.publish("mod-c", header + expanded(
        "CLASS\tBad\t0\tNaN\t1\t0\t0\t1\t1\t0\t4\t1\t0\t1\n")));
    assert(!registry.publish("mod-c", header + expanded(custom) + expanded(custom)));
    assert(!registry.publish("mod-c", header + "CLASS\tBad\t0\t1\t0\n"));
    assert(!registry.lookup("mod-c", "C:Bad", &stats));
    assert(!registry.publish("mod-c", header + expanded(
        "CLASS\tBroken.Inheritance\t0\t1\t1\t0\t0\t1\t1\t0\t4\t1\t0\t1\n").replace(24, 1, "x")));
    assert(registry.publish("mod-c", header + expanded(
        "CLASS\tScience.Damage\t0\t1e-5\t1\t0\t0\t1\t1\t0\t4\t1\t0\t1\n")));
    assert(registry.lookup("mod-c", "C:Science.Damage", &stats));
    assert(stats.damage.additive > 0.f && stats.damage.additive < 0.0001f);
    registry.unload("mod-c");
    assert(!registry.publish("mod-c", header + expanded(
        "CLASS\tBad.Damage\t0\t1.0 \t1\t0\t0\t1\t1\t0\t4\t1\t0\t1\n")));


    std::atomic<bool> readerOk{true};
    std::thread reader([&] {
        for (int i = 0; i < 10000; ++i) {
            NativeDamageClassStatDefaults current;
            if (!registry.lookup("mod-b", "C:Other.Damage", &current) ||
                current.crit != 4.f || current.standardCrit)
                readerOk.store(false);
        }
    });
    registry.unload("mod-a");
    reader.join();
    assert(readerOk.load());
    assert(!registry.lookup("mod-a", "C:Example.Damage", &stats));
    assert(registry.lookup("mod-b", "C:Other.Damage", &stats));
    assert(registry.publish("mod-a", header));
    registry.reset();
    assert(!registry.lookup("mod-b", "C:Other.Damage", &stats));
}
