#include "experiments/csharp/ManagedLiveSourceRevocations.h"

#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

int main() {
    bl::csharp::ManagedLiveSourceRevocations policy;
    std::map<std::string, int> cache{
        {"alpha", 1},
        {"alpha:BunnyLiveEvents", 2},
        {"alpha:CustomCallback", 3},
        {"alphabet", 4},
        {"beta", 5},
        {"beta:BunnyLiveEvents", 6}
    };
    assert(!policy.revoked("alpha"));
    assert(policy.revoke("", cache) == 0);
    assert(!policy.revoked(""));
    assert(policy.revoke("alpha", cache) == 3);
    assert(policy.revoked("alpha"));
    assert(!policy.revoked("alphabet") && !policy.revoked("beta"));
    assert(cache.size() == 3);
    assert(cache.contains("alphabet") && cache.contains("beta"));
    assert(cache.contains("beta:BunnyLiveEvents"));
    assert(policy.revoke("alpha", cache) == 0);
    cache.emplace("alpha:LateCallback", 7);
    assert(policy.revoked("alpha"));
    assert(policy.revoke("alpha", cache) == 1);
    assert(policy.revoke("beta", cache) == 2);
    assert(cache.size() == 1 && cache.contains("alphabet"));

    std::mutex mutex;
    std::map<std::string, int> concurrentCache;
    bl::csharp::ManagedLiveSourceRevocations concurrent;
    for (int i = 0; i < 1000; ++i)
        concurrentCache.emplace("mod" + std::to_string(i) + ":BunnyLiveEvents", i);
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&, worker] {
            for (int i = worker; i < 1000; i += 4) {
                const std::string modId = "mod" + std::to_string(i);
                std::lock_guard guard(mutex);
                assert(concurrent.revoke(modId, concurrentCache) == 1);
                assert(concurrent.revoked(modId));
            }
        });
    }
    for (auto& worker : workers) worker.join();
    assert(concurrentCache.empty());
    std::cout << "MANAGED_LIVE_SOURCE_REVOCATION_OK\n";
}
