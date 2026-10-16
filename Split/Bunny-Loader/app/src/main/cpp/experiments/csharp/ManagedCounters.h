#pragma once

#include <atomic>
#include <limits>

namespace bl::csharp {

inline int addManagedCounter(int& counter, int amount = 1) {
    const int maximum = std::numeric_limits<int>::max();
    counter = amount > maximum - counter ? maximum : counter + amount;
    return counter;
}

inline int addManagedCounter(std::atomic<int>& counter, int amount = 1) {
    int value = counter.load(std::memory_order_relaxed);
    for (;;) {
        int next = value;
        addManagedCounter(next, amount);
        if (counter.compare_exchange_weak(value, next, std::memory_order_relaxed))
            return next;
    }
}

}
