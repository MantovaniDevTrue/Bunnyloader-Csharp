#include "experiments/csharp/WorldNetAsync.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using bl::csharp::worldsave::WorldNetAsync;
using namespace std::chrono_literals;

int main() {
    std::atomic<int> started{0}, received{0};
    WorldNetAsync scheduler([&](const WorldNetAsync::Job& job,
                                std::vector<uint8_t>* result, std::string*) {
        ++started;
        std::this_thread::sleep_for(2ms);
        if (job.kind == WorldNetAsync::Kind::Receive) {
            ++received;
            return result->size() == 14;
        }
        result->assign({1, 2, 3});
        return true;
    });
    assert(scheduler.send(1, 12, -1));
    assert(scheduler.send(1, 13, -1));
    assert(!scheduler.send(0, 1, -1));
    assert(!scheduler.send(1, 256, -1));
    std::vector<uint8_t> packet(14, 7);
    assert(scheduler.receive(1, packet.data(), packet.size()));
    packet[0] = 9;
    int delivered = 0, receivedResults = 0;
    for (int i = 0; i < 200 && delivered < 3; ++i) {
        scheduler.pump(1, [&](const WorldNetAsync::Result& result) {
            assert(result.success);
            if (result.job.kind == WorldNetAsync::Kind::Send) {
                assert(result.job.packet == std::vector<uint8_t>({1, 2, 3}));
                ++delivered;
            } else {
                assert(result.job.packet.front() == 7);
                ++receivedResults;
                ++delivered;
            }
        });
        std::this_thread::sleep_for(2ms);
    }
    assert(delivered == 3 && receivedResults == 1 && received == 1);
    assert(scheduler.send(10, 0, -1));
    scheduler.invalidate();
    for (int i = 0; i < 15; ++i) {
        scheduler.pump(10, [&](const WorldNetAsync::Result&) { assert(false); });
        std::this_thread::sleep_for(2ms);
    }
    assert(scheduler.send(11, 9, -1));
    bool deliveredNew = false;
    for (int i = 0; i < 200 && !deliveredNew; ++i) {
        scheduler.pump(11, [&](const WorldNetAsync::Result& result) {
            assert(result.job.epoch == 11 && result.job.remote == 9);
            deliveredNew = result.success;
        });
        std::this_thread::sleep_for(2ms);
    }
    assert(deliveredNew);
    for (int i = 0; i < 200; ++i) assert(!scheduler.receive(0, packet.data(), 14));
    std::puts("WORLD_NET_ASYNC_SCHEDULER_PASS");
}
