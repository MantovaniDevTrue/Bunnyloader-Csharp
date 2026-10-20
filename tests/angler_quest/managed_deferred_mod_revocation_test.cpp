#include "experiments/csharp/ManagedDeferredModRevocation.h"

#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

struct Binding {
    std::string modId;
};

struct Job {
    int binding;
    int sequence;
};

int main() {
    const std::vector<Binding> bindings{
        {"angler"}, {"angler2"}, {"other"}, {"angler"}
    };
    std::deque<Job> jobs{
        {0, 1}, {2, 2}, {1, 3}, {3, 4}, {-1, 5}, {99, 6}, {2, 7}
    };
    assert(bl::csharp::eraseManagedModBoundJobs(jobs, bindings, "") == 0);
    assert(bl::csharp::eraseManagedModBoundJobs(jobs, bindings, "angler") == 2);
    assert(jobs.size() == 5);
    assert(jobs[0].sequence == 2 && jobs[1].sequence == 3 &&
           jobs[2].sequence == 5 && jobs[3].sequence == 6 &&
           jobs[4].sequence == 7);
    assert(bl::csharp::eraseManagedModBoundJobs(jobs, bindings, "angler") == 0);
    assert(bl::csharp::eraseManagedModBoundJobs(jobs, bindings, "angler2") == 1);
    assert(bl::csharp::eraseManagedModBoundJobs(jobs, bindings, "other") == 2);
    assert(jobs.size() == 2 && jobs[0].binding == -1 && jobs[1].binding == 99);
    std::deque<Job> busy;
    for (int i = 0; i < 1000; ++i) busy.push_back({i % 4, i});
    assert(bl::csharp::eraseManagedModBoundJobs(busy, bindings, "angler") == 500);
    assert(busy.size() == 500);
    for (size_t i = 0; i < busy.size(); ++i) {
        assert(busy[i].sequence == static_cast<int>((i / 2) * 4 + 1 + (i % 2)));
        assert(busy[i].binding == 1 || busy[i].binding == 2);
    }
    std::cout << "MANAGED_DEFERRED_MOD_REVOCATION_OK\n";
}
