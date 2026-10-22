#include "experiments/csharp/ManagedBannerSaveDispatch.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

using bl::csharp::banners::SaveDispatch;

int main() {
    SaveDispatch dispatch;
    assert(!dispatch.requestFromWorldSave(true));
    dispatch.enterWorld(12);
    assert(!dispatch.requestFromWorldSave(false));
    assert(SaveDispatch::networkDepth() == 0);
    {
        SaveDispatch::NetworkScope network;
        assert(SaveDispatch::networkDepth() == 1);
        assert(!dispatch.requestFromWorldSave(true));
        {
            SaveDispatch::NetworkScope nested;
            assert(SaveDispatch::networkDepth() == 2);
            assert(!dispatch.requestFromWorldSave(true));
        }
        assert(SaveDispatch::networkDepth() == 1);
    }
    assert(SaveDispatch::networkDepth() == 0);
    assert(!dispatch.consumeOnMainThread(12));
    assert(dispatch.requestFromWorldSave(true));
    assert(!dispatch.consumeOnMainThread(13));
    assert(!dispatch.consumeOnMainThread(12));
    assert(dispatch.requestFromWorldSave(true));
    assert(dispatch.consumeOnMainThread(12));
    assert(!dispatch.consumeOnMainThread(12));

    // O contexto de pacote de rede nao deve vazar para uma thread de save.
    std::atomic<bool> unrelatedThread{false};
    {
        SaveDispatch::NetworkScope network;
        std::thread other([&] {
            unrelatedThread.store(dispatch.requestFromWorldSave(true));
        });
        other.join();
        assert(!dispatch.requestFromWorldSave(true));
    }
    assert(unrelatedThread.load());
    assert(dispatch.consumeOnMainThread(12));
    assert(dispatch.requestFromWorldSave(true));

    // Um world change cancela qualquer gravacao atrasada.
    dispatch.enterWorld(20);
    assert(!dispatch.consumeOnMainThread(12));
    assert(!dispatch.consumeOnMainThread(20));
    assert(dispatch.requestFromWorldSave(true));
    dispatch.leaveWorld();
    assert(!dispatch.consumeOnMainThread(20));
    assert(!dispatch.requestFromWorldSave(true));

    std::cout << "Banner save dispatch PASS (network TLS, nested packets, world epoch, single consumption)\n";
}
