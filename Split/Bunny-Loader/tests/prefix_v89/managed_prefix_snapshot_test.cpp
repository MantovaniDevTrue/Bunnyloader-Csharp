#include <atomic>
#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "experiments/csharp/ManagedPrefixBridge.h"

namespace bl::log {
void write(int, const char*, ...) {}
}

namespace bl::runtime {
int modPrefixByKey(const std::string& key) {
    if (key == "test/Guarded") return 98;
    if (key == "test/Mighty") return 99;
    if (key == "test/Swift") return 100;
    return -1;
}
int modItemTypeByKey(const std::string& key) {
    if (key == "test/ExampleBlade") return 6207;
    if (key == "test/ExampleCharm") return 6208;
    return -1;
}
}

namespace bl::csharp {
bool installManagedPrefixGameplayHooks() { return true; }
}

#include "../../app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp"

int main() {
    using namespace bl::csharp;
    auto first = managedPrefixView();
    assert(first && first->empty());
    assert(registeredManagedPrefixCount() == 0);

    reserveManagedPrefixDescriptor(98);
    assert(managedPrefixDescriptorPending(98));
    assert(!managedPrefixDescriptorReady(98));

    ManagedPrefixEntry guarded;
    guarded.type = 98;
    guarded.name = "Guarded";
    guarded.displayName = "Guarded";
    guarded.category = 5;
    guarded.rollWeight = 1.f;
    guarded.accessoryDefense = 4;

    assert(applyManagedPrefixDescriptors("test", {guarded}));
    auto second = managedPrefixView();
    assert(second && second != first && first->empty());
    assert(second->size() == 1 && second->front().type == 98);
    assert(!managedPrefixDescriptorPending(98));
    assert(managedPrefixDescriptorReady(98));

    ManagedPrefixEntry mighty;
    mighty.type = 99;
    mighty.name = "Mighty";
    mighty.category = 4;
    mighty.rollWeight = 5.f;
    mighty.multipliers[0] = 1.2f;
    assert(applyManagedPrefixDescriptors("test", {mighty}));
    auto third = managedPrefixView();
    assert(third && third->size() == 2 && second->size() == 1);
    assert(third->at(0).name == "Guarded");
    assert(third->at(1).name == "Mighty");
    assert(snapshotManagedPrefixes().size() == 2);
    assert(registeredManagedPrefixCount() == 2);
    assert(managedPrefixView() == third);

    assert(!applyManagedPrefixDescriptors("test", {mighty}));
    assert(managedPrefixView() == third);

    ManagedPrefixEntry invalid = mighty;
    invalid.type = 100;
    invalid.name = "Swift";
    invalid.category = 6;
    assert(!applyManagedPrefixDescriptors("test", {invalid}));
    assert(managedPrefixView() == third);

    ManagedPrefixEntry swift = invalid;
    swift.category = 1;
    swift.rollWeight = 2.f;
    swift.itemWeights[1] = -1;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemWeights[1] = 21;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemWeights[1] = 0;
    swift.itemWeights[512] = 7;
    swift.itemNames["UnknownItem"] = 3;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemNames.clear();
    swift.itemNames["ExampleBlade"] = 11;
    swift.itemNames["ExampleCharm"] = 0;
    auto duplicate = swift;
    duplicate.itemWeights[6207] = 9;
    assert(!applyManagedPrefixDescriptors("test", {duplicate}));
    assert(managedPrefixView() == third);

    std::atomic<bool> go{false};
    std::atomic<int> snapshots{0};
    std::thread reader([&] {
        while (!go.load(std::memory_order_acquire)) {
            auto current = managedPrefixView();
            assert(current);
            assert(current->size() == 2 || current->size() == 3);
            assert(current->at(0).type == 98);
            assert(current->at(1).type == 99);
            if (current->size() == 3) assert(current->at(2).type == 100);
            snapshots.fetch_add(1, std::memory_order_relaxed);
        }
    });
    while (snapshots.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();
    assert(applyManagedPrefixDescriptors("test", {swift}));
    go.store(true, std::memory_order_release);
    reader.join();

    auto final = managedPrefixView();
    assert(final->size() == 3 && final->at(2).name == "Swift");
    assert(final->at(2).itemWeights.at(1) == 0);
    assert(final->at(2).itemWeights.at(512) == 7);
    assert(final->at(2).itemWeights.at(6207) == 11);
    assert(final->at(2).itemWeights.at(6208) == 0);
    assert(final->at(2).itemNames.empty());
    assert(third->size() == 2);
    assert(second->size() == 1);
    assert(first->empty());
    assert(registeredManagedPrefixCount() == 3);
    assert(snapshots.load(std::memory_order_relaxed) > 0);
}
